#include "RedisMgr.h"
#include "const.h"
#include "ConfigMgr.h"
#include "Logger.h"
//#include "DistLock.h"

RedisMgr::RedisMgr() {
	auto& gCfgMgr = ConfigMgr::Instance();
	auto host = gCfgMgr["Redis"]["host"];
	auto port = gCfgMgr["Redis"]["port"];
	auto pwd = gCfgMgr["Redis"]["password"];
	_con_pool.reset(new RedisConPool(5, host.c_str(), atoi(port.c_str()), pwd.c_str()));
}

RedisMgr::~RedisMgr() {
	_con_pool->Close();
	_con_pool->ClearConnections();
}

bool RedisMgr::Get(const std::string& key, std::string& value)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "GET %s", key.c_str());
	if (reply == NULL) {
		LOG_WARN("GET " << key << " failed");
		// freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_STRING) {
		LOG_WARN("GET " << key << " failed");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	value = reply->str;
	freeReplyObject(reply);

	LOG_DEBUG("Succeed to execute command GET " << key);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::Set(const std::string& key, const std::string& value) {
	//执  redis      
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

	if (NULL == reply)
	{
		LOG_ERROR("Execut command SET " << key << " " << value << " failure!");
		//freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply == NULL) {
		LOG_ERROR("Execut command SET " << key << " " << value << " failure: reply is null!");
		return false;
	}
	if (reply->type == REDIS_REPLY_ERROR) {
		LOG_ERROR("Redis error in SET: " << reply->str);
	}

	if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0)))
	{
		LOG_ERROR("Execut command SET " << key << " " << value << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	freeReplyObject(reply);
	LOG_DEBUG("Execut command SET " << key << " " << value << " success!");
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::LPush(const std::string& key, const std::string& value)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		LOG_ERROR("Execut command LPUSH " << key << " " << value << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		LOG_ERROR("Execut command LPUSH " << key << " " << value << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("Execut command LPUSH " << key << " " << value << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::LPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "LPOP %s ", key.c_str());
	if (reply == nullptr) {
		LOG_WARN("Execut command LPOP " << key << " failure!");
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		LOG_WARN("Execut command LPOP " << key << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	value = reply->str;
	LOG_DEBUG("Execut command LPOP " << key << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::RPush(const std::string& key, const std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
	if (NULL == reply)
	{
		LOG_ERROR("Execut command RPUSH " << key << " " << value << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
		LOG_ERROR("Execut command RPUSH " << key << " " << value << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("Execut command RPUSH " << key << " " << value << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::RPop(const std::string& key, std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "RPOP %s ", key.c_str());
	if (reply == nullptr) {
		LOG_WARN("Execut command RPOP " << key << " failure!");
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type == REDIS_REPLY_NIL) {
		LOG_WARN("Execut command RPOP " << key << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}
	value = reply->str;
	LOG_DEBUG("Execut command RPOP " << key << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::HSet(const std::string& key, const std::string& hkey, const std::string& value) {
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(), value.c_str());
	if (reply == nullptr) {
		LOG_ERROR("Execut command HSet " << key << " " << hkey << " " << value << " failure!");
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("Execut command HSet " << key << " " << hkey << " " << value << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("Execut command HSet " << key << " " << hkey << " " << value << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::HSet(const char* key, const char* hkey, const char* hvalue, size_t hvaluelen)
{
	auto connect = _con_pool->GetRedisCon();
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
	if (reply == nullptr) {
		LOG_ERROR("Execut command HSet " << key << " " << hkey << " " << hvalue << " failure!");
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_ERROR("Execut command HSet " << key << " " << hkey << " " << hvalue << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}
	LOG_DEBUG("Execut command HSet " << key << " " << hkey << " " << hvalue << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

std::string RedisMgr::HGet(const std::string& key, const std::string& hkey)
{
	auto connect = _con_pool->GetRedisCon();
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
	if (reply == nullptr) {
		LOG_ERROR("Execut command HGet " << key << " " << hkey << " failure: reply is null!");
		_con_pool->ReturnRedisCon(connect);
		return "";
	}

	if (reply->type == REDIS_REPLY_ERROR) {
		LOG_ERROR("Redis error in HGet: " << reply->str);
	}

	if (reply->type == REDIS_REPLY_NIL) {
		freeReplyObject(reply);
		LOG_WARN("Execut command HGet " << key << " " << hkey << " failure!");
		_con_pool->ReturnRedisCon(connect);
		return "";
	}

	std::string value = reply->str;
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	LOG_DEBUG("Execut command HGet " << key << " " << hkey << " success!");
	return value;
}

bool RedisMgr::HDel(const std::string& key, const std::string& field)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}

	Defer defer([&connect, this]() {
		_con_pool->ReturnRedisCon(connect);
		});

	redisReply* reply = (redisReply*)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
	if (reply == nullptr) {
		LOG_ERROR("HDEL command failed");
		return false;
	}

	bool success = false;
	if (reply->type == REDIS_REPLY_INTEGER) {
		success = reply->integer > 0;
	}

	freeReplyObject(reply);
	return success;
}

bool RedisMgr::Del(const std::string& key)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}
	auto reply = (redisReply*)redisCommand(connect, "DEL %s", key.c_str());
	if (reply == nullptr) {
		LOG_WARN("Execut command Del " << key << " failure!");
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER) {
		LOG_WARN("Execut command Del " << key << " failure!");
		freeReplyObject(reply);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	LOG_DEBUG("Execut command Del " << key << " success!");
	freeReplyObject(reply);
	_con_pool->ReturnRedisCon(connect);
	return true;
}

bool RedisMgr::ExistsKey(const std::string& key)
{
	auto connect = _con_pool->GetRedisCon();
	if (connect == nullptr) {
		return false;
	}

	auto reply = (redisReply*)redisCommand(connect, "exists %s", key.c_str());
	if (reply == nullptr) {
		LOG_DEBUG("Not Found Key " << key);
		_con_pool->ReturnRedisCon(connect);
		return false;
	}

	if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
		LOG_DEBUG("Not Found Key " << key);
		_con_pool->ReturnRedisCon(connect);
		freeReplyObject(reply);
		return false;
	}
	LOG_DEBUG("Found Key " << key << " exists!");
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



