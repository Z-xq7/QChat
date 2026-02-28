#pragma once
#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <queue>
#include "data.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::GetChatServerReq;
using message::LoginReq;
using message::LoginRsp;
using message::ChatService;

using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;

using message::KickUserReq;
using message::KickUserRsp;

//连接池类，管理grpc连接
class ChatConPool
{
public:
	ChatConPool(size_t poolSize, std::string host, std::string port):poolSize_(poolSize),
	host_(host), port_(port), b_stop_(false)
	{
		for (size_t i = 0; i < poolSize_; ++i)
		{
			auto channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
			auto stub = std::make_unique<message::ChatService::Stub>(channel);
			connections_.push(std::move(stub));
		}
	}

	~ChatConPool()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		Close();
		while (!connections_.empty())
		{
			connections_.pop();
		}
	}

	//获取一个连接
	std::unique_ptr<ChatService::Stub> getConnection()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this]
		{
			if (b_stop_) return true;
			return !connections_.empty();
		});
		//如果连接池已关闭，返回空指针
		if (b_stop_) return nullptr;

		auto conn = std::move(connections_.front());
		connections_.pop();
		return conn;
	}

	//归还一个连接
	void returnConnection(std::unique_ptr<ChatService::Stub> conn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (b_stop_) return;
		connections_.push(std::move(conn));
		cond_.notify_one();
	}

	//关闭连接池
	void Close()
	{
		b_stop_ = true;
		cond_.notify_all();
	}

private:
	atomic<bool> b_stop_;
	size_t poolSize_;
	std::string host_;
	std::string port_;
	std::queue<std::unique_ptr<message::ChatService::Stub>> connections_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

//聊天服务的grpc客户端，使用连接池管理grpc连接
class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
	friend class Singleton<ChatGrpcClient>;
public:
	~ChatGrpcClient(){}
	AddFriendRsp NotifyAddFriend(std::string server_ip,const AddFriendReq& request);
	AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq& request);
	bool GetBaseInfo(std::string base_key,int uid,std::shared_ptr<UserInfo>& userinfo);
	TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq& request,const Json::Value& rtvalue);
	KickUserRsp NotifyKickUser(std::string server_ip, const KickUserReq& request);

private:
	ChatGrpcClient();
	unordered_map<std::string,std::unique_ptr<ChatConPool>> _pools;

};

