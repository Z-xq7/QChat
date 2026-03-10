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
		//连接成功后，进行验证
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
	if (_b_stop) {
		// 修复：在停止状态下，调用者仍持有连接，需要释放
		if (con) {
			redisFree(con);
		}
		return;
	}
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
		// 添加安全检查
		if (conn) {
			redisFree(conn);
		}
	}
}

//关闭连接池
void RedisConPool::Close() {
	_b_stop = true;
	_cond.notify_all();

	// 等待检查线程完全退出
	if (_check_thread.joinable()) {
		_check_thread.join();
	}
}

//重新连接
bool RedisConPool::Reconnect() {
	// 如果正在停止，不要创建新连接
	if (_b_stop) {
		return false;
	}
	auto context = redisConnect(_host, _port);
	if (context == nullptr || context->err != 0) {
		LOG_ERROR("Redis Reconnect Failed! context error: " << (context ? context->errstr : "null context"));
		if (context != nullptr) {
			redisFree(context);
		}
		return false;
	}

	auto reply = (redisReply*)redisCommand(context, "AUTH %s", _pwd);
	if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
		LOG_ERROR("Redis Reconnect AUTH Failed!");
		//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
		freeReplyObject(reply);
		redisFree(context);
		return false;
	}

	//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	LOG_INFO("Redis Reconnect Success!");
	ReturnRedisCon(context);
	return true;
}

//检测redis连接健康
void RedisConPool::CheckThreadPro() {
	// 提前检查是否正在停止
	if (_b_stop) {
		return;
	}

	size_t pool_size;
	{
		// 先获得当前池大小
		std::lock_guard<std::mutex> lock(_mutex);
		pool_size = _pool.size();
	}

	for (int i = 0; i < pool_size && !_b_stop; ++i) {
		redisContext* ctx = nullptr;
		// 1) 取出一个连接(非阻塞)(如果有)
		//bool bsuccess = false;
		auto* context = GetConNonBlock();
		if (context == nullptr) {
			break;
		}

		redisReply* reply = nullptr;
		try {
			reply = (redisReply*)redisCommand(context, "PING");
			// 2. 先看底层 I/O、协议是否出错
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
			// 4. 如果没有问题，就还回去
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

	if (_fail_count > 0) {
		LOG_WARN("Redis Connection check completed, " << _fail_count << " connections failed");
	}

	//执行重新连接
	while (_fail_count > 0) {
		auto res = Reconnect();
		if (res) {
			_fail_count--;
		}
		else {
			//放弃这次重连尝试
			break;
		}
	}
}