#include "ChatGrpcClient.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"
#include "CSession.h"
#include "MysqlMgr.h"
#include "Logger.h"

ChatGrpcClient::ChatGrpcClient()
{
	auto& cfg = ConfigMgr::Inst();
	auto server_list = cfg["PeerServer"]["Servers"];
	std::vector<std::string> words;
	std::stringstream ss(server_list);
	std::string word;

	while (std::getline(ss,word,','))
	{
		words.push_back(word);
	}

	for (auto& word:words)
	{
		if (cfg[word]["Name"].empty())
		{
			continue;
		}
		_pools[cfg[word]["Name"]] = std::make_unique<ChatConPool>(5, cfg[word]["Host"], cfg[word]["Port"]);
	}
}

//处理添加好友的请求
AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq& request)
{
	AddFriendRsp rsp;
	Defer defer([&rsp,&request]() {
		rsp.set_error(ErrorCodes::Success);
		rsp.set_applyuid(request.applyuid());
		rsp.set_touid(request.touid());
	});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end())
	{
		LOG_WARN("NotifyAddFriend - server not found in pool, server_ip: " << server_ip);
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Status status = stub->NotifyAddFriend(&context, request, &rsp);

	Defer defercon([&stub,this,&pool]()
	{
		pool->returnConnection(std::move(stub));
	});

	if (!status.ok())
	{
		LOG_ERROR("NotifyAddFriend RPC failed - error: " << status.error_message());
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}
	
AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq& request)
{
	AuthFriendRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &request]() {
		rsp.set_fromuid(request.fromuid());
		rsp.set_touid(request.touid());
		});

	//先从连接池中获取对应的服务器连接，如果没有说明连接池没有对应的服务器连接，或没有连接（没有上线），直接返回成功即可
	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}

	//如果连接池中有对应的服务器连接，则通过grpc通知对应服务器认证结果
	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Status status = stub->NotifyAuthFriend(&context, request, &rsp);

	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) {
		LOG_ERROR("NotifyAuthFriend RPC failed - error: " << status.error_message());
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	return true;
}

//处理文本消息的发送
TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& request, const Json::Value& rtvalue)
{
	TextChatMsgRsp rsp;
	rsp.set_error(ErrorCodes::Success);

	Defer defer([&rsp, &request]()
		{
			rsp.set_fromuid(request.fromuid());
			rsp.set_touid(request.touid());
			for (const auto& text_data : request.textmsgs())
			{
				TextChatData* new_msg = rsp.add_textmsgs();
				new_msg->set_unique_id(text_data.unique_id());
				new_msg->set_msgcontent(text_data.msgcontent());
			}
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end())
	{
		LOG_WARN("NotifyTextChatMsg - server not found in pool, server_ip: " << server_ip);
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Status status = stub->NotifyTextChatMsg(&context, request, &rsp);

	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
	});

	if (!status.ok())
	{
		LOG_ERROR("NotifyTextChatMsg RPC failed - error: " << status.error_message());
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}

//踢掉用户的请求
KickUserRsp ChatGrpcClient::NotifyKickUser(std::string server_ip, const KickUserReq& request)
{
	KickUserRsp rsp;
	Defer defer([&rsp, &request]()
		{
			rsp.set_error(ErrorCodes::Success);
			rsp.set_uid(request.uid());
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end())
	{
		LOG_WARN("NotifyKickUser - server not found in pool, server_ip: " << server_ip);
		return rsp;
	}

	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
	});

	Status status = stub->NotifyKickUser(&context, request, &rsp);
	if (!status.ok())
	{
		LOG_ERROR("NotifyKickUser RPC failed - error: " << status.error_message());
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}
