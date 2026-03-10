#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "Logger.h"
#include <climits>

std::string generate_unique_string() {
	// 创建UUID对象
	boost::uuids::uuid uuid = boost::uuids::random_generator()();
	// 将UUID转换为字符串
	std::string unique_string = boost::uuids::to_string(uuid);

	return unique_string;
}

StatusServiceImpl::StatusServiceImpl()
{
	auto& cfg = ConfigMgr::Instance();
	auto server_list = cfg["chatservers"]["name"];

	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;

	while (std::getline(ss, word, ',')) {
		words.push_back(word);
	}

	for (auto& word : words) {
		if (cfg[word]["name"].empty()) {
			continue;
		}

		ChatServer server;
		server.port = cfg[word]["port"];
		server.host = cfg[word]["host"];
		server.name = cfg[word]["name"];
		_servers[server.name] = server;
	}
	
	LOG_INFO("StatusServiceImpl initialized with " << _servers.size() << " chat servers");
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
	const auto& server = getChatServer();
	reply->set_host(server.host);
	reply->set_port(server.port);
	reply->set_error(ErrorCodes::Success);
	reply->set_token(generate_unique_string());
	insertToken(request->uid(), reply->token());
	
	LOG_INFO("GetChatServer for uid: " << request->uid() << ", assigned server: " << server.name << ":" << server.port);
	return Status::OK;
}

ChatServer StatusServiceImpl::getChatServer() {
	std::lock_guard<std::mutex> guard(_server_mtx);
	auto minServer = _servers.begin()->second;

	//从Redis中获取服务器连接数，获取连接数最少的服务器
	auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, minServer.name);
	if (count_str.empty()) {
		//如果不存在默认设置为最大
		minServer.con_count = INT_MAX;
	}
	else {
		//否则设置为实际连接数
		minServer.con_count = std::stoi(count_str);
	}

	// 使用范围基于for循环
	for ( auto& server : _servers) {
		
		if (server.second.name == minServer.name) {
			continue;
		}
		
		auto count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.second.name);
		if (count_str.empty()) {
			server.second.con_count = INT_MAX;
		}
		else {
			server.second.con_count = std::stoi(count_str);
		}

		if (server.second.con_count < minServer.con_count) {
			minServer = server.second;
		}
	}
	
	LOG_DEBUG("Selected server: " << minServer.name << " with connection count: " << minServer.con_count);
	return minServer;
}

Status StatusServiceImpl::Login(ServerContext* context, const LoginReq* request, LoginRsp* reply)
{
	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (success) {
		reply->set_error(ErrorCodes::UidInvalid);
		LOG_WARN("Login failed: invalid uid " << uid);
		return Status::OK;
	}
	
	if (token_value != token) {
		reply->set_error(ErrorCodes::TokenInvalid);
		LOG_WARN("Login failed: invalid token for uid " << uid);
		return Status::OK;
	}
	reply->set_error(ErrorCodes::Success);
	reply->set_uid(uid);
	reply->set_token(token);
	
	LOG_INFO("Login success for uid: " << uid);
	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token)
{
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(token_key, token);
	LOG_DEBUG("Inserted token for uid: " << uid);
}
