#pragma once
#include "const.h"

class RedisConPool :public Singleton<RedisConPool>
{
public:
	RedisConPool(std::size_t size, const char * host, int port, const char* password);
	~RedisConPool();
	//获取连接
	redisContext* GetRedisCon();
	//获取连接(非阻塞方式)
	redisContext* GetConNonBlock();
	//释放连接
	void ReturnRedisCon(redisContext* con);
	void ClearConnections();
	void Close();

private:
	bool Reconnect();
	void CheckThreadPro();

private:
	std::size_t _poolsize;
	const char* _host;
	const char* _pwd;
	int _port;
	std::atomic<bool> _b_stop;
	std::queue<redisContext*> _pool;
	std::mutex _mutex;
	std::condition_variable _cond;
	std::thread _check_thread;
	std::atomic<int> _fail_count;
	int _counter;
};

