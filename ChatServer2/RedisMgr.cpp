#include "RedisMgr.h"
#include "const.h"
#include "ConfigMgr.h"
#include "DistLock.h"
#include "Logger.h"

RedisMgr::RedisMgr() {
	auto& gCfgMgr = ConfigMgr::Inst();
	auto host = gCfgMgr["Redis"]["Host"];
	auto port = gCfgMgr["Redis"]["Port"];
	auto pwd = gCfgMgr["Redis"]["Passwd"];
	_con_pool.reset(new RedisConPool(10, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}

RedisMgr::~RedisMgr() {
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	 auto reply = (redisReply*)redisCommand(connect, "GET %s", key.c_str());
	 if (reply == NULL) {
		 LOG_ERROR("Redis GET failed - key: " << key);
		// freeReplyObject(reply);
		 _con_pool->returnConnection(connect);
		  return false;
	}

	 if (reply->type != REDIS_REPLY_STRING) {
		 LOG_DEBUG("Redis GET - key not found: " << key);
		 freeReplyObject(reply);
		 _con_pool->returnConnection(connect);
		 return false;
	}

	 value = reply->str;
	 freeReplyObject(reply);

	 LOG_DEBUG("Redis GET success - key: " << key);
	 _con_pool->returnConnection(connect);
	 return true;
}

bool RedisMgr::Set(const std::string &key, const std::string &value){
	//执行redis命令行
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

	//如果返回NULL，则说明执行失败
	if (NULL == reply)
	{
		LOG_ERROR("Redis SET failed - key: " << key);
		//freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	//如果执行失败，则释放连接
	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		LOG_ERROR("Redis SET failed - key: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(reply);
	LOG_DEBUG("Redis SET success - key: " << key);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		LOG_ERROR("Redis LPUSH failed - key: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		LOG_ERROR("Redis LPUSH failed - key: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	LOG_DEBUG("Redis LPUSH success - key: " << key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::LPop(const std::string &key, std::string& value){
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr ) {
		LOG_ERROR("Redis LPOP failed - key: " << key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		LOG_DEBUG("Redis LPOP - key empty: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	value = reply->str;
	LOG_DEBUG("Redis LPOP success - key: " << key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		LOG_ERROR("Redis RPUSH failed - key: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		LOG_ERROR("Redis RPUSH failed - key: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	LOG_DEBUG("Redis RPUSH success - key: " << key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr ) {
		LOG_ERROR("Redis RPOP failed - key: " << key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		LOG_DEBUG("Redis RPOP - key empty: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}
	value = reply->str;
	LOG_DEBUG("Redis RPOP success - key: " << key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value) {
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr ) {
		LOG_ERROR("Redis HSET failed - key: " << key << ", hkey: " << hkey);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("Redis HSET failed - key: " << key << ", hkey: " << hkey);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	LOG_DEBUG("Redis HSET success - key: " << key << ", hkey: " << hkey);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	 const char* argv[4];
	 size_t argvlen[4];
	 argv[0] = "HSET";
	argvlen[0] = 4;
	argv[1] = key;
	argvlen[1] = strlen(key);
	argv[2] = hkey;
	argvlen[2] = strlen(hkey);
	argv[3] = hvalue;
	argvlen[3] = hvaluelen;

	auto reply = (redisReply*)redisCommandArgv(connect, 4, argv, argvlen);
	if (reply == nullptr ) {
		LOG_ERROR("Redis HSET failed - key: " << key << ", hkey: " << hkey);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("Redis HSET failed - key: " << key << ", hkey: " << hkey);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}
	LOG_DEBUG("Redis HSET success - key: " << key << ", hkey: " << hkey);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return "";
	}
	const char* argv[3];
	size_t argvlen[3];
	argv[0] = "HGET";
	argvlen[0] = 4;
	argv[1] = key.c_str();
	argvlen[1] = key.length();
	argv[2] = hkey.c_str();
	argvlen[2] = hkey.length();
	
	auto reply = (redisReply*)redisCommandArgv(connect, 3, argv, argvlen);
	if (reply == nullptr ) {
		LOG_ERROR("Redis HGET failed - key: " << key << ", hkey: " << hkey);
		_con_pool->returnConnection(connect);
		return "";
	}

	if ( reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
		LOG_DEBUG("Redis HGET - key not found: " << key << ", hkey: " << hkey);
		_con_pool->returnConnection(connect);
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	LOG_DEBUG("Redis HGET success - key: " << key << ", hkey: " << hkey);
	return value;
}

bool RedisMgr::HDel(const std::string& key, const std::string& field)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}

	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});

	redisReply* reply = (redisReply*)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
	if (reply == nullptr) {
		LOG_ERROR("Redis HDEL failed - key: " << key << ", field: " << field);
		return false;
	}

	bool success = false;
	if (reply->type == REDIS_REPLY_INTEGER) {
		success = reply->integer > 0;
	}

	freeReplyObject(reply);
	if (success) {
		LOG_DEBUG("Redis HDEL success - key: " << key << ", field: " << field);
	}
	return success;
}

bool RedisMgr::Del(const std::string &key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr ) {
		LOG_ERROR("Redis DEL failed - key: " << key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if ( reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("Redis DEL failed - key: " << key);
		freeReplyObject(reply);
		_con_pool->returnConnection(connect);
		return false;
	}

	LOG_DEBUG("Redis DEL success - key: " << key);
	 freeReplyObject(reply);
	 _con_pool->returnConnection(connect);
	 return true;
}

bool RedisMgr::ExistsKey(const std::string &key)
{
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}

	auto reply = (redisReply*)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr ) {
		LOG_DEBUG("Redis EXISTS - key not found: " << key);
		_con_pool->returnConnection(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
		LOG_DEBUG("Redis EXISTS - key not found: " << key);
		_con_pool->returnConnection(connect);
		freeReplyObject(reply);
		return false;
	}
	LOG_DEBUG("Redis EXISTS - key found: " << key);
	freeReplyObject(reply);
	_con_pool->returnConnection(connect);
	return true;
}

std::string RedisMgr::acquireLock(const std::string& lockName,
	int lockTimeout, int acquireTimeout) {

	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return "";
	}

	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
	});

	return DistLock::Inst().acquireLock(connect, lockName, lockTimeout, acquireTimeout);
}

bool RedisMgr::releaseLock(const std::string& lockName,
	const std::string& identifier) {
	if (identifier.empty()) {
		return true;
	}
	auto connect = _con_pool->getConnection();
	if (connect == nullptr) {
		return false;
	}


	Defer defer([&connect, this]() {
		_con_pool->returnConnection(connect);
		});

	return DistLock::Inst().releaseLock(connect, lockName, identifier);
}

void RedisMgr::IncreaseCount(std::string server_name)
{
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	//利用defer解锁
	Defer defer2([this, identifier, lock_key]() {
		RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
		});

	//将登录数量增加
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!rd_res.empty()) {
		count = std::stoi(rd_res);
	}

	count++;
	auto count_str = std::to_string(count);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, count_str);
}

void RedisMgr::DecreaseCount(std::string server_name)
{
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	//利用defer解锁
	Defer defer2([this, identifier, lock_key]() {
		RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
		});

	//将登录数量减少
	auto rd_res = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
	int count = 0;
	if (!rd_res.empty()) {
		count = std::stoi(rd_res);
		if (count > 0) {
			count--;
		}
		
	}

	auto count_str = std::to_string(count);
	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, count_str);
}


void RedisMgr::InitCount(std::string server_name) {
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	//利用defer解锁
	Defer defer2([this, identifier, lock_key]() {
		RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
		});

	RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0");
}

void RedisMgr::DelCount(std::string server_name) {
	auto lock_key = LOCK_COUNT;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	//利用defer解锁
	Defer defer2([this, identifier, lock_key]() {
		RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
		});

	RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
}
