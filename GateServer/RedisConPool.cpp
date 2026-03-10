#include "RedisConPool.h"
#include "Logger.h"

RedisConPool::RedisConPool(std::size_t size, const char* host, int port, const char* pwd):
	_poolsize(size), _host(host), _port(port), _pwd(pwd), _b_stop(false), _counter(0), _fail_count(0)
{
	//初始化连接池
	for (std::size_t i = 0; i < size; ++i) {
		auto* conn = redisConnect(host, port);
		if (conn == nullptr || conn->err) {
			if (conn) {
				redisFree(conn);
			}
			LOG_ERROR("RedisConPool create redis connection error!");
			continue;
		}
		//连接成功后进行验证
		redisReply* reply = (redisReply*)redisCommand(conn, "AUTH %s", _pwd);
		if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
			LOG_ERROR("RedisConPool AUTH error!");
			freeReplyObject(reply);
			redisFree(conn);
			continue;
		}
		LOG_INFO("RedisConPool create redis connection success!");
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
	// 将线程设为分离状态，这样在程序结束时不需要等待线程结束
	_check_thread.detach();
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

//检查连接
void RedisConPool::CheckThreadPro() {
	size_t pool_size;
	{
		// 获取到当前连接数
		std::lock_guard<std::mutex> lock(_mutex);
		pool_size = _pool.size();
	}

	for (int i = 0; i < pool_size && !_b_stop; ++i) {
		redisContext* ctx = nullptr;
		// 1) 取出一个连接(非阻塞)(不等待)
		//bool bsuccess = false;
		auto* context = GetConNonBlock();
		if (context == nullptr) {
			break;
		}

		redisReply* reply = nullptr;
		try {
			reply = (redisReply*)redisCommand(context, "PING");
			// 2. 先看底层 I/O、协议层有没有错
			if (context->err) {
				LOG_ERROR("Redis Connection error: " << context->errstr);
				if (reply) {
					freeReplyObject(reply);
				}
				redisFree(context);
				_fail_count++;
				continue;
			}

			// 3. 再看 Redis 服务端返回的是不是 ERROR
			if (!reply || reply->type == REDIS_REPLY_ERROR) {
				LOG_ERROR("Reply is null, redis ping failed");
				if (reply) {
					freeReplyObject(reply);
				}
				redisFree(context);
				_fail_count++;
				continue;
			}
			// 4. 如果都没问题，就还回去
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

	LOG_INFO("Redis Connection check completed, " << _fail_count << " connections failed");

	//执行重连逻辑
	while (_fail_count > 0) {
		auto res = Reconnect();
		if (res) {
			_fail_count--;
		}
		else {
			//跳出下次重连流程
			break;
		}
	}
}

//重新连接
bool RedisConPool::Reconnect() {
	auto context = redisConnect(_host, _port);
	if (context == nullptr || context->err != 0) {
		LOG_ERROR("连接失败: Redis Reconnect Failed! context error: " << context->errstr);
		if (context != nullptr) {
			redisFree(context);
		}
		return false;
	}

	auto reply = (redisReply*)redisCommand(context, "AUTH %s", _pwd);
	if (reply->type == REDIS_REPLY_ERROR) {
		LOG_ERROR("认证失败: Redis Reconnect Failed!");
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(reply);
		redisFree(context);
		return false;
	}

	//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	LOG_INFO("连接成功: Redis Reconnect Success!");
	ReturnRedisCon(context);
	return true;
}