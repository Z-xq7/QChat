#include "RedisConPool.h"

RedisConPool::RedisConPool(std::size_t size, const char* host, int port, const char* pwd):
	_poolsize(size), _host(host), _port(port), _pwd(pwd), _b_stop(false), _counter(0), _fail_count(0)
{
	//创建连接池
	for (std::size_t i = 1; i < size; ++i) {
		auto* conn = redisConnect(host, port);
		if (conn == nullptr || conn->err) {
			if (conn) {
				redisFree(conn);
			}
			SetColor(RED);
			std::cout << "*** RedisConPool create redis connection error! ***" << std::endl;
			SetColor(RESET);
			continue;
		}
		//创建成功，输入密码
		redisReply* reply = (redisReply*)redisCommand(conn, "AUTH %s", _pwd);
		if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
			SetColor(RED);
			std::cout << "*** RedisConPool AUTH error! ***" << std::endl;
			SetColor(RESET);
			freeReplyObject(reply);
			redisFree(conn);
			continue;
		}
		SetColor(GREEN);
		std::cout << "--- RedisConPool create redis connection success! ---" << std::endl;
		SetColor(RESET);
		freeReplyObject(reply);
		_pool.push(conn);
	}

	_check_thread = std::thread([this]() {
		while (!_b_stop) {
			_counter++;
			if (_counter >= 60) {
				CheckThreadPro();
				_counter = 0;
			}

			std::this_thread::sleep_for(std::chrono::seconds(1)); // 每隔 30 秒发送一次 PING 命令
		}
	});
}

RedisConPool::~RedisConPool()
{
}

//获取连接(阻塞方式)
redisContext* RedisConPool::GetRedisCon()
{
	std::unique_lock<std::mutex> lock(_mutex);
	_cond.wait(lock, [&]()
	{
		if (_b_stop) return true;
		return !_pool.empty();
	});
	//如果连接池已经停止，则返回nullptr
	if (_b_stop) return nullptr;

	auto* conn = _pool.front();
	_pool.pop();
	return conn;
}

//获取连接(非阻塞方式)
redisContext* RedisConPool::GetConNonBlock() {
	std::unique_lock<std::mutex> lock(_mutex);
	if (_b_stop) {
		return nullptr;
	}

	if (_pool.empty()) {
		return nullptr;
	}

	auto* context = _pool.front();
	_pool.pop();
	return context;
}

//释放连接
void RedisConPool::ReturnRedisCon(redisContext* con)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_b_stop) return;
	_pool.push(con);
	_cond.notify_one();
}

void RedisConPool::ClearConnections()
{
	std::lock_guard<std::mutex> lock(_mutex);
	//释放连接池
	while (!_pool.empty()) {
		auto* conn = _pool.front();
		_pool.pop();
		redisFree(conn);
	}
}

//关闭连接池
void RedisConPool::Close() {
	_b_stop = true;
	_cond.notify_all();
}

//重连操作
bool RedisConPool::Reconnect() {
	auto context = redisConnect(_host, _port);
	if (context == nullptr || context->err != 0) {
		std::cout << "[*** 连接失败: Redis Reconnect Failed! context error: " << context->errstr << " ***]" << std::endl;
		if (context != nullptr) {
			redisFree(context);
		}
		return false;
	}

	auto reply = (redisReply*)redisCommand(context, "AUTH %s", _pwd);
	if (reply->type == REDIS_REPLY_ERROR) {
		std::cout << "[*** 认证失败: Redis Reconnect Failed! ***]" << std::endl;
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(reply);
		redisFree(context);
		return false;
	}

	//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	std::cout << "[--- 连接成功: Redis Reconnect Success! ---]" << std::endl;
	ReturnRedisCon(context);
	return true;
}

//检测redis有无断线
void RedisConPool::CheckThreadPro() {
	size_t pool_size;
	{
		// 先拿到当前连接数
		std::lock_guard<std::mutex> lock(_mutex);
		pool_size = _pool.size();
	}

	for (int i = 0; i < pool_size && !_b_stop; ++i) {
		redisContext* ctx = nullptr;
		// 1) 取出一个连接(持有锁)(非阻塞)
		//bool bsuccess = false;
		auto* context = GetConNonBlock();
		if (context == nullptr) {
			break;
		}

		redisReply* reply = nullptr;
		try {
			reply = (redisReply*)redisCommand(context, "PING");
			// 2. 先看底层 I/O／协议层有没有错
			if (context->err) {
				std::cout << "[*** Redis Connection error: " << context->errstr << " ***]" << std::endl;
				if (reply) {
					freeReplyObject(reply);
				}
				redisFree(context);
				_fail_count++;
				continue;
			}

			// 3. 再看 Redis 自身返回的是不是 ERROR
			if (!reply || reply->type == REDIS_REPLY_ERROR) {
				std::cout << "[*** Reply is null, redis ping failed: ***]" << std::endl;
				if (reply) {
					freeReplyObject(reply);
				}
				redisFree(context);
				_fail_count++;
				continue;
			}
			// 4. 如果都没问题，则还回去
			//std::cout << "connection alive" << std::endl;
			freeReplyObject(reply);
			ReturnRedisCon(context);
		}
		catch (std::exception& exp) {
			if (reply) {
				freeReplyObject(reply);
			}

			redisFree(context);
			_fail_count++;
		}
	}

	std::cout << "[*** Redis Connection check completed, " << _fail_count << " connections failed ***]" << std::endl;

	//执行重连操作
	while (_fail_count > 0) {
		auto res = Reconnect();
		if (res) {
			_fail_count--;
		}
		else {
			//留给下次再重试
			break;
		}
	}
}