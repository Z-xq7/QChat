#include "ChatGrpcClient.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"
#include "CSession.h"
#include "MysqlMgr.h"

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

//处理添加好友请求
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
		std::cout << "--- server ip: " << server_ip << " not found in pool ---" << std::endl;
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
		std::cout << "NotifyAddFriend RPC failed: " << status.error_message() << std::endl;
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

	//先从连接池中获取对应服务器的连接，如果没有说明连接池中没有对应服务器的连接，即没有这个用户在线，直接返回成功即可
	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end()) {
		return rsp;
	}

	//若果连接池中有对应服务器的连接，则通过grpc通知对应服务器有认证结果
	auto& pool = find_iter->second;
	ClientContext context;
	auto stub = pool->getConnection();
	Status status = stub->NotifyAuthFriend(&context, request, &rsp);

	Defer defercon([&stub, this, &pool]() {
		pool->returnConnection(std::move(stub));
		});

	if (!status.ok()) {
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}

bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	return true;
}

//处理文本消息请求
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
				new_msg->set_msgid(text_data.msgid());
				new_msg->set_msgcontent(text_data.msgcontent());
			}
		});

	auto find_iter = _pools.find(server_ip);
	if (find_iter == _pools.end())
	{
		std::cout << "--- server ip: " << server_ip << " not found in pool ---" << std::endl;
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
		std::cout << "*** NotifyTextChatMsg RPC failed: " << status.error_message() << " ***" << std::endl;
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}

//处理踢人请求
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
		std::cout << "--- server ip: " << server_ip << " not found in pool ---" << std::endl;
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
		std::cout << "*** NotifyKickUser RPC failed: " << status.error_message() << " ***" << std::endl;
		rsp.set_error(ErrorCodes::RPCFailed);
		return rsp;
	}
	return rsp;
}
