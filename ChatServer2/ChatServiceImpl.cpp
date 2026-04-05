#include "ChatServiceImpl.h"
#include "UserMgr.h"
#include "CSession.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "Logger.h"
#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

ChatServiceImpl::ChatServiceImpl()
{
}
Status ChatServiceImpl::NotifyAddFriend(ServerContext* context, const AddFriendReq* request,
	AddFriendRsp* reply)
{
	//��ѯ�û��Ƿ��ڱ���������
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);
	Defer defer([request,reply]()
	{
		reply->set_error(ErrorCodes::Success);
		reply->set_applyuid(request->applyuid());
		reply->set_touid(request->touid());
	});

	//�û������ڴ��У�ֱ�ӷ��سɹ����ȴ��Է���¼��ͨ����ѯredis��ȡ������Ϣ
	if (session == nullptr)
	{
		LOG_DEBUG("NotifyAddFriend - user not online, touid: " << touid);
		return Status::OK;
	}

	//���ڴ��У�ֱ��֪ͨ�Է�
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["applyuid"] = request->applyuid();
	rtvalue["name"] = request->name();
	rtvalue["desc"] = request->desc();
	rtvalue["icon"] = request->icon();
	rtvalue["sex"] = request->sex();
	rtvalue["nick"] = request->nick();

	std::string return_str = rtvalue.toStyledString();
	session->Send(return_str,ID_NOTIFY_ADD_FRIEND_REQ);
	LOG_DEBUG("NotifyAddFriend success - touid: " << touid);

	return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext* context,
	const AuthFriendReq* request, AuthFriendRsp* response)
{
	//��ѯ�û��Ƿ��ڱ�������
	auto touid = request->touid();
	auto fromuid = request->fromuid();
	auto session = UserMgr::GetInstance()->GetSession(touid);

	Defer defer([request, response]() {
		response->set_error(ErrorCodes::Success);
		response->set_fromuid(request->fromuid());
		response->set_touid(request->touid());
		});

	//�û������ڴ��У�ֱ�ӷ���
	if (session == nullptr) {
		LOG_DEBUG("NotifyAuthFriend - user not online, touid: " << touid);
		return Status::OK;
	}

	//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = request->fromuid();
	rtvalue["touid"] = request->touid();

	std::string base_key = USER_BASE_INFO + std::to_string(fromuid);
	auto user_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, fromuid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	for (auto& msg : request->textmsgs())
	{
		Json::Value  chat;
		chat["sender"] = msg.sender_id();
		chat["msg_id"] = msg.msg_id();
		chat["thread_id"] = msg.thread_id();
		chat["unique_id"] = msg.unique_id();
		chat["msg_content"] = msg.msgcontent();
		rtvalue["chat_datas"].append(chat);
	}

	std::string return_str = rtvalue.toStyledString();
	session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
	LOG_DEBUG("NotifyAuthFriend success - fromuid: " << fromuid << ", touid: " << touid);
	return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(::grpc::ServerContext* context,
	const TextChatMsgReq* request, TextChatMsgRsp* response)
{
	//��ѯ�û��Ƿ��ڱ�������
	auto touid = request->touid();
	auto session = UserMgr::GetInstance()->GetSession(touid);
	response->set_error(ErrorCodes::Success);

	//�û������ڴ��У�ֱ�ӷ���
	if (session == nullptr) {
		LOG_DEBUG("NotifyTextChatMsg - user not online, touid: " << touid);
		return Status::OK;
	}

	//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = request->fromuid();
	rtvalue["touid"] = request->touid();
	rtvalue["thread_id"] = request->thread_id();

	//���ı���Ϣ��֯Ϊ����
	Json::Value text_array;
	for (auto& msg : request->textmsgs()) {
		Json::Value msg_value;
		msg_value["unique_id"] = msg.unique_id();
		msg_value["content"] = msg.msgcontent();
		msg_value["message_id"] = msg.msg_id();
		msg_value["chat_time"] = msg.chat_time();
		text_array.append(msg_value);
	}
	//����Ϣ������뷵��ֵ��
	rtvalue["chat_datas"] = text_array;
	//������Ϣ
	std::string return_str = rtvalue.toStyledString();
	session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
	LOG_DEBUG("NotifyTextChatMsg success - fromuid: " << request->fromuid() << ", touid: " << touid);

	return Status::OK;
}

Status ChatServiceImpl::NotifyKickUser(grpc::ServerContext* context, const message::KickUserReq* request,
	message::KickUserRsp* response)
{
	//���ﲻ�ܼӷֲ�ʽ������Ϊ����һ���û���¼�ķ������ڼӷֲ�ʽ�������rpc��������ӿڣ�������������ᵼ������
	//��ѯ�û��Ƿ��ڱ�������
	auto uid = request->uid();
	auto session = UserMgr::GetInstance()->GetSession(uid);
	Defer defer([request,response]()
	{
		response->set_error(ErrorCodes::Success);
		response->set_uid(request->uid());
	});

	//�û������ڴ��У�ֱ�ӷ���
	if (session == nullptr) {
		LOG_DEBUG("NotifyKickUser - user not online, uid: " << uid);
		return Status::OK;
	}

	//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
	LOG_INFO("NotifyKickUser - kicking user, uid: " << uid << ", session_id: " << session->GetSessionId());
	session->NotifyOffline(uid);
	//�����ɵ�session
	_p_server->ClearSession(session->GetSessionId());

	return Status::OK;
}

Status ChatServiceImpl::NotifyChatImgMsg(grpc::ServerContext* context, const message::NotifyChatImgReq* request,
	message::NotifyChatImgRsp* response)
{
	//�����û��Ƿ��ڱ�������
	auto uid = request->to_uid();
	auto session = UserMgr::GetInstance()->GetSession(uid);

	Defer defer([request, response]() {
		//���þ���Ļذ���Ϣ
		response->set_error(ErrorCodes::Success);
		response->set_message_id(request->message_id());
		});

	//�û������ڴ�����ֱ�ӷ���
	if (session == nullptr) {
		//����ֻ�Ƿ���1��״̬
		return Status::OK;
	}

	//���ڴ�����ֱ�ӷ���֪ͨ�û�����ͼƬ��Ϣ
	session->NotifyChatImgRecv(request);
	//����ֻ�Ƿ���1��״̬
	return Status::OK;
}

Status ChatServiceImpl::NotifyChatFileMsg(::grpc::ServerContext* context,
	const ::message::NotifyChatFileReq* request, ::message::NotifyChatFileRsp* response)
{
	auto uid = request->to_uid();
	auto session = UserMgr::GetInstance()->GetSession(uid);

	Defer defer([request, response]() {
		response->set_error(ErrorCodes::Success);
		response->set_message_id(request->message_id());
		});

	if (session == nullptr) {
		return Status::OK;
	}

	session->NotifyChatFileRecv(request);
	return Status::OK;
}

void ChatServiceImpl::RegisterServer(std::shared_ptr<CServer> pServer)
{
	_p_server = pServer;
}

bool ChatServiceImpl::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
{
	//�����Ȳ�redis�в�ѯ�û���Ϣ
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		LOG_DEBUG("GetBaseInfo from redis - uid: " << uid << ", name: " << userinfo->name);
	}
	else {
		//redis��û�У����ѯmysql
		//��ѯ���ݿ�
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			LOG_WARN("GetBaseInfo failed - uid not found: " << uid);
			return false;
		}

		userinfo = user_info;

		//�����ݿ���ȡ��д��redis����
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
		LOG_DEBUG("GetBaseInfo from mysql - uid: " << uid << ", name: " << userinfo->name);
	}

	return true;
}
