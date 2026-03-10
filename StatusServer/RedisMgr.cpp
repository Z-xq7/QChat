#include "RedisMgr.h"
#include "const.h"
#include "ConfigMgr.h"
#include "DistLock.h"
#include "Logger.h"

RedisMgr::RedisMgr() {
	auto& gCfgMgr = ConfigMgr::Instance();
	auto host = gCfgMgr["Redis"]["host"];
	auto port = gCfgMgr["Redis"]["port"];
	auto pwd = gCfgMgr["Redis"]["password"];
	_con_pool.reset(new RedisConPool(5, host.c_str(), atoi(port.c_str()), pwd.c_str()));
	LOG_INFO("RedisMgr initialized with host: " << host << ", port: " << port);
}

RedisMgr::~RedisMgr() {
	_con_pool->Close();
	_con_pool->ClearConnections();
	LOG_DEBUG("RedisMgr destructed");
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for GET " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "GET %s", key.c_str());
	if (reply == NULL) {
		LOG_ERROR("GET command failed for key: " << key);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_STRING) {
		LOG_WARN("GET command returned non-string type for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	value = reply->str;
	freeReplyObject(reply);

	LOG_DEBUG("GET command success for key: " << key);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::Set(const std::string& key, const std::string& value) {
	//ִ  redis      
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for SET " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

	if (NULL == reply)
	{
		LOG_ERROR("SET command failed for key: " << key << ", value: " << value);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply == NULL) {
		LOG_ERROR("SET command reply is null for key: " << key);
		return false;
	}
	if (reply->type == REDIS_REPLY_ERROR) {
		LOG_ERROR("Redis error in SET: " << reply->str);
	}

	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		LOG_ERROR("SET command failed for key: " << key << ", value: " << value);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	freeReplyObject(reply);
	LOG_DEBUG("SET command success for key: " << key);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::SetWithExpire(const std::string& key, const std::string& value, int expire_seconds)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for SETEX " << key);
		return false;
	}

	// 使用SETEX命令，同时设置值和过期时间
	auto reply = (redisReply*)redisCommand(connect, "SETEX %s %d %s",
		key.c_str(), expire_seconds, value.c_str());

	if (NULL == reply) {
		LOG_ERROR("SETEX command failed for key: " << key << ", expire: " << expire_seconds);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (!(reply->type == REDIS_REPLY_STATUS &&
		(strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0))) {
		LOG_ERROR("SETEX command failed for key: " << key << ", expire: " << expire_seconds);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	freeReplyObject(reply);
	LOG_DEBUG("SETEX command success for key: " << key << ", expire: " << expire_seconds);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for LPUSH " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		LOG_ERROR("LPUSH command failed for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		LOG_ERROR("LPUSH command failed for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("LPUSH command success for key: " << key);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for LPOP " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr) {
		LOG_ERROR("LPOP command failed for key: " << key);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		LOG_WARN("LPOP command returned NIL for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	value = reply->str;
	LOG_DEBUG("LPOP command success for key: " << key);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for RPUSH " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		LOG_ERROR("RPUSH command failed for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		LOG_ERROR("RPUSH command failed for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("RPUSH command success for key: " << key);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for RPOP " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr) {
		LOG_ERROR("RPOP command failed for key: " << key);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		LOG_WARN("RPOP command returned NIL for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}
	value = reply->str;
	LOG_DEBUG("RPOP command success for key: " << key);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for HSET " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr) {
		LOG_ERROR("HSET command failed for key: " << key << ", hkey: " << hkey);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("HSET command failed for key: " << key << ", hkey: " << hkey);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("HSET command success for key: " << key << ", hkey: " << hkey);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for HSET " << key);
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
	if (reply == nullptr) {
		LOG_ERROR("HSET command failed for key: " << key << ", hkey: " << hkey);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("HSET command failed for key: " << key << ", hkey: " << hkey);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}
	LOG_DEBUG("HSET command success for key: " << key << ", hkey: " << hkey);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for HGET " << key);
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
	if (reply == nullptr) {
		LOG_ERROR("HGET command failed (reply is null) for key: " << key << ", hkey: " << hkey);
		_con_pool->ReturnRedisCon(connect);
		return "";
	}

	if (reply->type == REDIS_REPLY_ERROR) {
		LOG_ERROR("Redis error in HGET: " << reply->str);
	}

	if (reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
		LOG_WARN("HGET command returned NIL for key: " << key << ", hkey: " << hkey);
		_con_pool->ReturnRedisCon(connect);
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	LOG_DEBUG("HGET command success for key: " << key << ", hkey: " << hkey);
	return value;
}

bool RedisMgr::HDel(const std::string& key, const std::string& field)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for HDEL " << key);
		return false;
	}

	Defer defer([&connect, this]() {
		_con_pool->ReturnRedisCon(connect);
		});

	redisReply* reply = (redisReply*)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
	if (reply == nullptr) {
		LOG_ERROR("HDEL command failed for key: " << key << ", field: " << field);
		return false;
	}

	bool success = false;
	if (reply->type == REDIS_REPLY_INTEGER) {
		success = reply->integer > 0;
		if (success) {
			LOG_DEBUG("HDEL command success for key: " << key << ", field: " << field);
		} else {
			LOG_WARN("HDEL command returned 0 for key: " << key << ", field: " << field);
		}
	}

	freeReplyObject(reply);
	return success;
}

bool RedisMgr::Del(const std::string& key)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for DEL " << key);
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr) {
		LOG_ERROR("DEL command failed for key: " << key);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("DEL command failed for key: " << key);
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("DEL command success for key: " << key);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::ExistsKey(const std::string& key)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		LOG_ERROR("Failed to get Redis connection for EXISTS " << key);
		return false;
	}

	auto reply = (redisReply*)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr) {
		LOG_WARN("Key not found: " << key);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
		LOG_DEBUG("Key not found: " << key);
		_con_pool->ReturnRedisCon(connect);
		freeReplyObject(reply);
		return false;
	}
	LOG_DEBUG("Key exists: " << key);
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

//std::string RedisMgr::acquireLock(const std::string& lockName,
//	int lockTimeout, int acquireTimeout) {
//
//	auto connect = _con_pool->GetRedisCon();
//	if (connect == nullptr) {
//		return "";
//	}
//
//	Defer defer([&connect, this]() {
//		_con_pool->ReturnRedisCon(connect);
//		});
//
//	return DistLock::Inst().acquireLock(connect, lockName, lockTimeout, acquireTimeout);
//}
//
//bool RedisMgr::releaseLock(const std::string& lockName,
//	const std::string& identifier) {
//	if (identifier.empty()) {
//		return true;
//	}
//	auto connect = _con_pool->GetRedisCon();
//	if (connect == nullptr) {
//		return false;
//	}
//
//
//	Defer defer([&connect, this]() {
//		_con_pool->ReturnRedisCon(connect);
//		});
//
//	return DistLock::Inst().releaseLock(connect, lockName, identifier);
//}



