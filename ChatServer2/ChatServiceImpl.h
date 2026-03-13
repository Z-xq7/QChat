#pragma once
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <mutex>
#include "data.h"
#include "CServer.h"
#include <memory>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using message::AddFriendReq;
using message::AddFriendRsp;
using message::AddFriendMsg;

using message::AuthFriendReq;
using message::AuthFriendRsp;

using message::ChatService;
using message::TextChatMsgReq;
using message::TextChatMsgRsp;
using message::TextChatData;
using message::KickUserReq;
using message::KickUserRsp;
using message::NotifyChatImgReq;

class ChatServiceImpl final : public ChatService::Service
{
public:
	ChatServiceImpl();
	Status NotifyAddFriend(ServerContext* context, const AddFriendReq* request,
		AddFriendRsp* reply) override;

	Status NotifyAuthFriend(ServerContext* context,
		const AuthFriendReq* request, AuthFriendRsp* response) override;

	Status NotifyTextChatMsg(::grpc::ServerContext* context,
		const TextChatMsgReq* request, TextChatMsgRsp* response) override;

	// 处理踢人请求
	Status NotifyKickUser(grpc::ServerContext* context,
		const message::KickUserReq* request, message::KickUserRsp* response) override;

	//接收Resource服务器发送的图片聊天通知
	Status NotifyChatImgMsg(::grpc::ServerContext* context, 
		const ::message::NotifyChatImgReq* request, ::message::NotifyChatImgRsp* response) override;

	void RegisterServer(std::shared_ptr<CServer> pServer);

	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo);

private:
	std::shared_ptr<CServer> _p_server;
};

