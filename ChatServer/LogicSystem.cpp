#include "LogicSystem.h"

#include <iomanip>
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "DistLock.h"
#include <string>
#include "CServer.h"
#include "Logger.h"
using namespace std;

LogicSystem::LogicSystem():_b_stop(false), _p_server(nullptr){
	RegisterCallBacks();
	_worker_thread = std::thread (&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem(){
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(shared_ptr < LogicNode> msg) {
	std::unique_lock<std::mutex> unique_lk(_mutex);
	_msg_que.push(msg);
	//��0��Ϊ1����֪ͨ�ź�
	if (_msg_que.size() == 1) {
		unique_lk.unlock();
		_consume.notify_one();
	}
}

void LogicSystem::SetServer(std::shared_ptr<CServer> pserver) {
	_p_server = pserver;
}

std::string LogicSystem::getCurrentTimestamp()
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t tt = std::chrono::system_clock::to_time_t(now);

	std::tm tm_time{};
#ifdef _WIN32
	localtime_s(&tm_time, &tt);
#else
	localtime_r(&tt, &tm_time);
#endif

	std::ostringstream oss;
	oss << std::put_time(&tm_time, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

void LogicSystem::DealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> unique_lk(_mutex);
		//�ж϶���Ϊ�������������������̲߳��ͷ���
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}
		//�ж��Ƿ�Ϊ�ر�״̬���������߼�ִ��������˳�ѭ��
		if (_b_stop ) {
			while (!_msg_que.empty()) {
				auto msg_node = _msg_que.front();
				_msg_que.pop();
				LOG_DEBUG("recv_msg id is " << msg_node->_recvnode->_msg_id);
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end()) {
					_msg_que.pop();
					continue;
				}
				unique_lk.unlock();
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
					std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				unique_lk.lock();
			}
			break;
		}
		//���û��ֹͣ����˵������������
		auto msg_node = _msg_que.front();
		LOG_DEBUG("recv_msg id is " << msg_node->_recvnode->_msg_id);
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			LOG_WARN("msg id [" << msg_node->_recvnode->_msg_id << "] handler not found");
			continue;
		}

		// ��ִ���߼�ǰ�Ƚ�� LogicSystem �� _mutex����������
		unique_lk.unlock();  
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id, 
			std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks() {
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_HEART_BEAT_REQ] = std::bind(&LogicSystem::HeartBeatHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_LOAD_CHAT_THREAD_REQ] = std::bind(&LogicSystem::GetUserThreadsHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CREATE_PRIVATE_CHAT_REQ] = std::bind(&LogicSystem::CreatePrivateChat, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_LOAD_CHAT_MSG_REQ] = std::bind(&LogicSystem::LoadChatMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_IMG_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatImgMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_FILE_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatFileMsg, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CALL_INVITE_REQ] = std::bind(&LogicSystem::VideoCallInvite, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CALL_ACCEPT_REQ] = std::bind(&LogicSystem::VideoCallAccept, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CALL_REJECT_REQ] = std::bind(&LogicSystem::VideoCallReject, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	// 群聊相关回调
	_fun_callbacks[ID_CREATE_GROUP_CHAT_REQ] = std::bind(&LogicSystem::CreateGroupChatHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_GET_GROUP_MEMBERS_REQ] = std::bind(&LogicSystem::GetGroupMembersHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
	_fun_callbacks[ID_UPDATE_GROUP_NOTICE_REQ] = std::bind(&LogicSystem::UpdateGroupNoticeHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
}

//�����û���¼���߼���1.�ȴ�redis�л�ȡ�û�token�Ƿ���ȷ��2.���token��ȷ��˵����¼�ɹ���������ݿ��ȡ�û�������Ϣ��
//3.�����ݿ��ȡ�����б���4.�����ݿ��ȡ�����б���5.���ӵ�¼������6.session���û�uid��7.Ϊ�û����õ�¼ip server������������8.uid��session�󶨹�ϵ�����ظ��ͻ���
void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	LOG_INFO("User login request - uid: " << uid << ", token: " << token);

	Json::Value rtvalue;
	Defer defer([this,&rtvalue,session]()
	{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, MSG_CHAT_LOGIN_RSP);
	});

	//��redis�л�ȡ�û�token�Ƿ���ȷ
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_WARN("Login failed - uid: " << uid << ", token not found in redis");
		return;
	}
	if (token_value != token) {
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		LOG_WARN("Login failed - uid: " << uid << ", token invalid");
		return;
	}
	rtvalue["error"] = ErrorCodes::Success;

	//�����ݿ��ȡ�û�������Ϣ
	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_ERROR("Login failed - uid: " << uid << ", user info not found");
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	rtvalue["token"] = token;

	//�����ݿ��ȡ���������б�
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply) {
		for (auto& apply : apply_list) {
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj);
		}
	}

	//��ȡ�����б�
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto& friend_ele : friend_list) {
		Json::Value obj;
		obj["name"] = friend_ele->name;
		obj["uid"] = friend_ele->uid;
		obj["icon"] = friend_ele->icon;
		obj["nick"] = friend_ele->nick;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		rtvalue["friend_list"].append(obj);
	}

	// 获取用户加入的所有群聊基本信息
	std::vector<int> group_thread_ids;
	bool b_group = MysqlMgr::GetInstance()->GetUserGroupChats(uid, group_thread_ids);
	if (b_group) {
		for (int thread_id : group_thread_ids) {
			GroupInfo group_info;
			bool ok = MysqlMgr::GetInstance()->GetGroupInfo(thread_id, group_info);
			if (ok) {
				Json::Value obj;
				obj["thread_id"] = group_info.thread_id;
				obj["name"] = group_info.name;
				obj["icon"] = group_info.icon;
				obj["notice"] = group_info.notice;
				obj["desc"] = group_info.desc;
				obj["owner_id"] = group_info.owner_id;
				obj["member_count"] = group_info.member_count;
				rtvalue["group_list"].append(obj);
			}
		}
	}
	LOG_INFO("Loaded " << group_thread_ids.size() << " group chats for uid: " << uid);

	//��¼����ͳ��
	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	////********** ���ﱻ�Ż��ˣ��Ȳ����� **********
	//RedisMgr::GetInstance()->IncreaseCount(server_name);
	{
		//******* �˴����Ϸֲ�ʽ����������߳�ռ�õ�¼ *******
		//ƴ���û�ip��Ӧ��key
		auto lock_key = LOCK_PREFIX + uid_str;
		auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);

		//����defer����
		Defer defer2([this, identifier, lock_key]() {
			RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
			});

		//�˴��жϸ��û��Ƿ��ڱ������߱������ڵ�¼
		std::string uid_ip_value = "";
		auto uid_ip_key = USERIPPREFIX + uid_str;
		bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
		//˵���û��Ѿ���¼�ˣ��˴�Ӧ���ߵ�֮ǰ�ĸ��û���¼״̬
		if (b_ip) {
			//��ȡ��ǰ������ip��Ϣ
			auto& cfg = ConfigMgr::Inst();
			auto self_name = cfg["SelfServer"]["Name"];
			//���֮ǰ��¼�ķ������뵱ǰ��ͬ����ֱ���ڱ��������ߵ�
			if (uid_ip_value == self_name) {
				//���Ҿɵ�����
				auto old_session = UserMgr::GetInstance()->GetSession(uid);

				//�˴�Ӧ�÷����ߵ���Ϣ
				if (old_session) {
					LOG_INFO("Kick old session - uid: " << uid << ", session_id: " << old_session->GetSessionId());
					old_session->NotifyOffline(uid);
					//�����ɵ�����
					_p_server->ClearSession(old_session->GetSessionId());
				}
			}
			else {
				//������Ǳ�����������֪ͨgrpc֪ͨ�÷������ߵ�
				//����֪ͨ
				LOG_INFO("Notify kick user via grpc - uid: " << uid << ", target_server: " << uid_ip_value);
				KickUserReq kick_req;
				kick_req.set_uid(uid);
				ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
			}
		}
		//session���û�uid
		session->SetUserId(uid);
		//Ϊ�û����õ�¼ip server����������
		std::string  ipkey = USERIPPREFIX + uid_str;
		RedisMgr::GetInstance()->Set(ipkey, server_name);
		//uid��session�󶨹�ϵ,�����Ժ����
		UserMgr::GetInstance()->SetUserSession(uid, session);
		std::string  uid_session_key = USER_SESSION_PREFIX + uid_str;
		RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());

	}
	LOG_INFO("User login success - uid: " << uid << ", name: " << user_info->name);
	return;
}

//������ѯ�û���Ϣ���߼�
void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	LOG_INFO("Search user info request - search_key: " << uid_str);

	Json::Value  rtvalue;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP);
		});

	//�ж��ǲ��Ǵ����֣�����Ǵ�����˵����uid������˵�����û���
	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		GetUserByUid(uid_str, rtvalue);
	}
	else {
		GetUserByName(uid_str, rtvalue);
	}
	return;
}

//�������Ӻ��ѵ�ҵ���߼���1.�ȸ������ݿ⣻2.��ѯredis ��ȡtouid��Ӧ��server ip��
//3.����ڱ�����������ֱ��֪ͨ�Է��յ�������Ϣ��4.������ڱ�����������ͨ��grpc֪ͨ��Ӧ���������ͺ�����Ϣ
void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto desc = root["applyname"].asString();
	auto bakname = root["bakname"].asString();
	auto touid = root["touid"].asInt();
	LOG_INFO("Add friend apply - from_uid: " << uid << ", to_uid: " << touid << ", desc: " << desc);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_FRIEND_RSP);
		});

	//�ȸ������ݿ�
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid,desc,bakname);

	//��ѯredis ��ȡtouid��Ӧ��server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		LOG_DEBUG("Add friend apply - target user offline, to_uid: " << touid);
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	//�����ݿ��ȡ�����˻�����Ϣ
	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	//ֱ��֪ͨ�Է��յ�������Ϣ
	//��touid�ڱ���������¼ʱ����ֱ�Ӵ��ڴ��л�ȡsession����֪ͨ��
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = apply_info->name;
			notify["desc"] = desc;
			//�����ȡ�����˻�����Ϣ�ɹ����򽫻�����ϢҲ���͸��Է����ڶԷ�չʾ
			if (b_info) {
				notify["icon"] = apply_info->icon;
				notify["sex"] = apply_info->sex;
				notify["nick"] = apply_info->nick;
			}
			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
			LOG_DEBUG("Notify add friend locally - to_uid: " << touid);
		}
		return ;
	}

	//��touid���ڱ���������¼ʱ����ͨ��grpc֪ͨ��Ӧ���������ͺ�����Ϣ
	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);
	add_req.set_name(apply_info->name);
	add_req.set_desc(desc);
	if (b_info) {
		add_req.set_icon(apply_info->icon);
		add_req.set_sex(apply_info->sex);
		add_req.set_nick(apply_info->nick);
	}
	//����֪ͨ
	LOG_DEBUG("Notify add friend via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value,add_req);
}

//��������������֤���߼���1.�ȸ������ݿ⣻2.��ѯredis ��ȡ�����˶�Ӧ��server ip��3.����ڱ�����������ֱ��֪ͨ�Է���֤���
void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto back_name = root["back"].asString();
	LOG_INFO("Auth friend apply - from_uid: " << uid << ", to_uid: " << touid);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid);
	//�����ݿ��ȡ�Է��û�������Ϣ
	bool b_info = GetBaseInfo(base_key, touid, user_info);
	if (b_info) {
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
		rtvalue["uid"] = touid;
	}
	else {
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP);
		});

	//�ȸ������ݿ⣬ͬ�����Ӻ��Ѻ͸������ݿ������Ӻ��ѹ�ϵ��������״̬��Ϊͬ��(�޸ģ��˴����ں��棬���ﲻ��������)
	//MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	std::vector<std::shared_ptr<message::AddFriendMsg>> chat_datas;

	//�����ݿ����Ӻ���
	MysqlMgr::GetInstance()->AddFriend(uid, touid,back_name,chat_datas);

	//��ѯredis ��ȡtouid��Ӧ��server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		LOG_DEBUG("Auth friend - target user offline, to_uid: " << touid);
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//ֱ��֪ͨ�Է���֤ͨ������Ϣ
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info) {
				notify["name"] = user_info->name;
				notify["nick"] = user_info->nick;
				notify["icon"] = user_info->icon;
				notify["sex"] = user_info->sex;
			}
			else {
				notify["error"] = ErrorCodes::UidInvalid;
			}

			for (auto& chat_data : chat_datas)
			{
				Json::Value  chat;
				chat["sender"] = chat_data->sender_id();
				chat["msg_id"] = chat_data->msg_id();
				chat["thread_id"] = chat_data->thread_id();
				chat["unique_id"] = chat_data->unique_id();
				chat["msg_content"] = chat_data->msgcontent();
				notify["chat_datas"].append(chat);
				rtvalue["chat_datas"].append(chat);
			}

			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
			LOG_DEBUG("Notify auth friend locally - to_uid: " << touid);
		}
		return ;
	}

	//�����ڴ��У����ڱ�����������ͨ��grpc֪ͨ��Ӧ��������֤���
	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);
	for (auto& chat_data : chat_datas)
	{
		auto text_msg = auth_req.add_textmsgs();
		text_msg->CopyFrom(*chat_data);
		Json::Value chat;
		chat["sender"] = chat_data->sender_id();
		chat["msg_id"] = chat_data->msg_id();
		chat["thread_id"] = chat_data->thread_id();
		chat["unique_id"] = chat_data->unique_id();
		chat["msg_content"] = chat_data->msgcontent();
		rtvalue["chat_datas"].append(chat);
	}

	//����֪ͨ
	LOG_DEBUG("Notify auth friend via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

//�����ı���Ϣ���߼���1.�ȴ����ݿ��ȡ�Ự�򴴽���Ϣ��2.��ѯredis �ҵ������߶�Ӧ��server ip��3.����ڱ�����������ֱ��֪ͨ�Է��ı���Ϣ��
void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto thread_id = root["thread_id"].asInt();
	bool is_group = root.get("is_group", false).asBool();

	const Json::Value  arrays = root["text_array"];
	
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	rtvalue["thread_id"] = thread_id;
	rtvalue["is_group"] = is_group;

	std::vector <std::shared_ptr<ChatMessage>> chat_datas;
	auto timestamp = getCurrentTimestamp();
	for (const auto& txt_obj : arrays) {
		auto content = txt_obj["content"].asString();
		auto unique_id = txt_obj["unique_id"].asString();
		LOG_DEBUG("Chat text msg - content: " << content << ", unique_id: " << unique_id);
		auto chat_msg = std::make_shared<ChatMessage>();
		chat_msg->chat_time = timestamp;
		chat_msg->sender_id = uid;
		chat_msg->recv_id = touid;
		chat_msg->unique_id = unique_id;
		chat_msg->thread_id = thread_id;
		chat_msg->content = content;
		chat_msg->status = 2;
		chat_msg->msg_type = int(ChatMsgType::TEXT);
		chat_datas.push_back(chat_msg);
	}

	//�������ݿ�
	MysqlMgr::GetInstance()->AddChatMsg(chat_datas);

	for (const auto& chat_data : chat_datas) {
		Json::Value  chat_msg;
		chat_msg["message_id"] = chat_data->message_id;
		chat_msg["unique_id"] = chat_data->unique_id;
		chat_msg["content"] = chat_data->content;
		chat_msg["status"] = chat_data->status;
		chat_msg["chat_time"] = chat_data->chat_time;
		rtvalue["chat_datas"].append(chat_msg);
	}

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
		});

	// ����Ƿ�����Ⱥ��
	if (is_group) {
		// Ⱥ��ģʽ��ȡȺ��Ա�б���������ÿ���Ա
		std::vector<std::shared_ptr<GroupMemberInfo>> members;
		bool res = MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members);
		if (!res) {
			LOG_ERROR("DealChatTextMsg - failed to get group members, thread_id: " << thread_id);
			return;
		}

		auto& cfg = ConfigMgr::Inst();
		auto self_name = cfg["SelfServer"]["Name"];

		// ����ÿ��Ⱥ��Ա�������ͻ���
		for (const auto& member : members) {
			int member_uid = member->uid;
			// �������Լ�
			if (member_uid == uid) {
				continue;
			}

			// ��ѯredis��ȡ��Ա������server ip
			auto member_str = std::to_string(member_uid);
			auto member_ip_key = USERIPPREFIX + member_str;
			std::string member_ip_value = "";
			bool b_member_ip = RedisMgr::GetInstance()->Get(member_ip_key, member_ip_value);
			if (!b_member_ip) {
				LOG_DEBUG("Group chat text msg - member offline, member_uid: " << member_uid);
				continue;
			}

			// ֱ��֪ͨ�Է�
			if (member_ip_value == self_name) {
				auto member_session = UserMgr::GetInstance()->GetSession(member_uid);
				if (member_session) {
					std::string return_str = rtvalue.toStyledString();
					member_session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
					LOG_DEBUG("Notify group text chat msg locally - member_uid: " << member_uid);
				}
			}
			else {
				// ͨ��gRPC֪ͨ����������
				TextChatMsgReq text_msg_req;
				text_msg_req.set_fromuid(uid);
				text_msg_req.set_touid(member_uid);
				text_msg_req.set_thread_id(thread_id);
				for (const auto& chat_data : chat_datas) {
					auto* text_msg = text_msg_req.add_textmsgs();
					text_msg->set_unique_id(chat_data->unique_id);
					text_msg->set_msgcontent(chat_data->content);
					text_msg->set_msg_id(chat_data->message_id);
					text_msg->set_chat_time(chat_data->chat_time);
				}
				LOG_DEBUG("Notify group text chat msg via grpc - member_uid: " << member_uid << ", server: " << member_ip_value);
				ChatGrpcClient::GetInstance()->NotifyTextChatMsg(member_ip_value, text_msg_req, rtvalue);
			}
		}
		return;
	}

	// ˽��ģʽ�����е�touid����
	//��ѯredis ��ȡtouid��Ӧ��server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		LOG_DEBUG("Chat text msg - target user offline, to_uid: " << touid);
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//ֱ��֪ͨ�Է���֤ͨ������Ϣ
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//���ڱ���У�ֱ�ӷ���֪ͨ�Է�
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
			LOG_DEBUG("Notify text chat msg locally - to_uid: " << touid);
		}
		return ;
	}

	TextChatMsgReq text_msg_req;
	text_msg_req.set_fromuid(uid);
	text_msg_req.set_touid(touid);
	text_msg_req.set_thread_id(thread_id);
	for (const auto& chat_data : chat_datas) {
		auto* text_msg = text_msg_req.add_textmsgs();
		text_msg->set_unique_id(chat_data->unique_id);
		text_msg->set_msgcontent(chat_data->content);
		text_msg->set_msg_id(chat_data->message_id);
		text_msg->set_chat_time(chat_data->chat_time);
	}

	//����֪ͨ todo...
	LOG_DEBUG("Notify text chat msg via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

//��������������1.����session�е�����ʱ�䣻2.�������������˵���ͻ����Ѿ����ߣ�������Ӧ�Ĵ���
void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["fromuid"].asInt();
	LOG_DEBUG("Receive heart beat - uid: " << uid);
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	session->Send(rtvalue.toStyledString(), ID_HEART_BEAT_RSP);
}

//��ȡ���ݿ�洢�ĶԻ��߳���Ϣ
void LogicSystem::GetUserThreadsHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	//�����ݿ��chat_threads��¼
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	int last_id = root["thread_id"].asInt();
	LOG_INFO("Get user threads request - uid: " << uid << ", last_id: " << last_id);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	Defer defer([this,&rtvalue,session]()
	{
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str,ID_LOAD_CHAT_THREAD_RSP);
	});

	std::vector<std::shared_ptr<ChatThreadInfo>> threads;

	//��ȡһ�β�ѯ�������Ƿ�����һҳ��������һҳ��last_id
	int page_size = 10;
	bool load_more = false;
	int next_last_id = 0;
	bool res = GetUserThreads(uid,last_id,page_size,threads,load_more,next_last_id);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_ERROR("Get user threads failed - uid: " << uid);
		return;
	}

	rtvalue["load_more"] = load_more;
	rtvalue["next_thread_id"] = (int)next_last_id;
	//��threads����д��json����
	for (auto& thread : threads)
	{
		Json::Value thread_value;
		thread_value["thread_id"] = int(thread->_thread_id);
		thread_value["type"] = thread->_type;
		thread_value["user1_id"] = thread->_user1_id;
		thread_value["user2_id"] = thread->_user2_id;
		thread_value["group_name"] = thread->_group_name;
		rtvalue["threads"].append(thread_value);
	}
	LOG_DEBUG("Get user threads success - uid: " << uid << ", thread_count: " << threads.size());
}

void LogicSystem::CreatePrivateChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto other_id = root["other_id"].asInt();
	LOG_INFO("Create private chat - uid: " << uid << ", other_id: " << other_id);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;
	rtvalue["other_id"] = other_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CREATE_PRIVATE_CHAT_RSP);
		});

	int thread_id = 0;
	bool res = MysqlMgr::GetInstance()->CreatePrivateChat(uid, other_id, thread_id);
	if (!res) {
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		LOG_ERROR("Create private chat failed - uid: " << uid << ", other_id: " << other_id);
		return;
	}

	rtvalue["thread_id"] = thread_id;
	LOG_INFO("Create private chat success - thread_id: " << thread_id);
}

void LogicSystem::LoadChatMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto thread_id = root["thread_id"].asInt();
	auto message_id = root["message_id"].asInt();
	LOG_INFO("Load chat messages - thread_id: " << thread_id << ", message_id: " << message_id);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["thread_id"] = thread_id;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_LOAD_CHAT_MSG_RSP);
		});

	int page_size = 10;
	std::shared_ptr<PageResult> res = MysqlMgr::GetInstance()->LoadChatMsg(thread_id, message_id, page_size);
	if (!res)
	{
		rtvalue["error"] = ErrorCodes::LOAD_CHAT_MSG_FAILED;
		LOG_ERROR("Load chat messages failed - thread_id: " << thread_id);
		return;
	}

	rtvalue["last_message_id"] = res->next_cursor;
	rtvalue["load_more"] = res->load_more;
	for (auto& msg : res->messages)
	{
		Json::Value chat_data;
		chat_data["sender"] = msg.sender_id;
		chat_data["msg_id"] = msg.message_id;
		chat_data["thread_id"] = msg.thread_id;
		chat_data["unique_id"] = 0;
		chat_data["msg_content"] = msg.content;
		chat_data["chat_time"] = msg.chat_time;
		chat_data["status"] = msg.status;
		chat_data["msg_type"] = msg.msg_type;
		chat_data["receiver"] = msg.recv_id;

		rtvalue["chat_datas"].append(chat_data);
	}
	LOG_DEBUG("Load chat messages success - message_count: " << res->messages.size());
}

void LogicSystem::DealChatImgMsg(std::shared_ptr<CSession> session,
	const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	auto md5 = root["md5"].asString();
	auto unique_name = root["name"].asString();
	auto token = root["token"].asString();
	auto unique_id = root["unique_id"].asString();
	auto chat_time = root["chat_time"].asString();
	auto status = root["status"].asInt();
	LOG_INFO("Deal chat image msg - from_uid: " << uid << ", to_uid: " << touid << ", unique_name: " << unique_name);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	auto thread_id = root["thread_id"].asInt();
	rtvalue["thread_id"] = thread_id;
	rtvalue["md5"] = md5;
	rtvalue["unique_name"] = unique_name;
	rtvalue["unique_id"] = unique_id;
	rtvalue["chat_time"] = chat_time;
	//����ͼƬ�ļ�״̬Ϊδ�ϴ������ļ��ϴ��ɹ����ٸ���״̬��֪ͨ�Է�
	rtvalue["status"] = MsgStatus::UN_UPLOAD;

	auto timestamp = getCurrentTimestamp();
	auto chat_msg = std::make_shared<ChatMessage>();
	chat_msg->chat_time = timestamp;
	chat_msg->sender_id = uid;
	chat_msg->recv_id = touid;
	chat_msg->unique_id = unique_id;
	chat_msg->thread_id = thread_id;
	chat_msg->content = unique_name;
	chat_msg->status = MsgStatus::UN_UPLOAD;
	chat_msg->msg_type = int(ChatMsgType::PIC);

	//�������ݿ�
	MysqlMgr::GetInstance()->AddChatMsg(chat_msg);

	rtvalue["message_id"] = chat_msg->message_id;
	//������Բ���defer���ƣ���Ϊ����û����Ҫ�������߼��ˣ�ֱ�ӷ�����Ӧ����
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_IMG_CHAT_MSG_RSP);
		});

	//����֪ͨ todo... �Ժ���ļ��ϴ��ɹ���֪ͨ
}

void LogicSystem::DealChatFileMsg(std::shared_ptr<CSession> session,
	const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();

	auto md5 = root["md5"].asString();
	auto unique_name = root["name"].asString();
	auto origin_name = root["origin_name"].asString();
	auto token = root["token"].asString();
	auto unique_id = root["unique_id"].asString();
	auto chat_time = root["chat_time"].asString();
	auto status = root["status"].asInt();
	LOG_INFO("Deal chat file msg - from_uid: " << uid << ", to_uid: " << touid << ", unique_name: " << unique_name << ", origin_name: " << origin_name);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	auto thread_id = root["thread_id"].asInt();
	rtvalue["thread_id"] = thread_id;
	rtvalue["md5"] = md5;
	rtvalue["unique_name"] = unique_name;
	rtvalue["origin_name"] = origin_name;
	rtvalue["unique_id"] = unique_id;
	rtvalue["chat_time"] = chat_time;
	rtvalue["status"] = MsgStatus::UN_UPLOAD;

	auto timestamp = getCurrentTimestamp();
	auto chat_msg = std::make_shared<ChatMessage>();
	chat_msg->chat_time = timestamp;
	chat_msg->sender_id = uid;
	chat_msg->recv_id = touid;
	chat_msg->unique_id = unique_id;
	chat_msg->thread_id = thread_id;
	chat_msg->content = unique_name + "|" + origin_name;
	chat_msg->status = MsgStatus::UN_UPLOAD;
	chat_msg->msg_type = int(ChatMsgType::FILE);

	MysqlMgr::GetInstance()->AddChatMsg(chat_msg);

	rtvalue["message_id"] = chat_msg->message_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_FILE_CHAT_MSG_RSP);
	});
}

void LogicSystem::VideoCallInvite(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto caller_uid = root["caller_uid"].asInt();	//�����ߵ�uid����room_id�����
	auto caller_nick = root["caller_nick"].asString();
	auto callee_uid = root["callee_uid"].asInt();
	std::string call_id = std::to_string(caller_uid);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CALL_INVITE_RSP);
		});

	//��ѯredis ��ȡcallee_uid��Ӧ��server ip
	auto to_str = std::to_string(callee_uid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";

	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		LOG_DEBUG("Video Call Invite - target user offline, to_uid: " << callee_uid);
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//ֱ��֪ͨ�Է���֤ͨ������Ϣ
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(callee_uid);
		if (session) {
			//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
			rtvalue["caller_nick"] = caller_nick;
			rtvalue["caller_uid"] = caller_uid;
			rtvalue["call_id"] = call_id;	//��ʱ����caller_uid����call_id�����������Ż���ȫ��Ψһ��call_id
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_CALL_INCOMING_NOTIFY);
			LOG_DEBUG("Notify Call Incoming locally - to_uid: " << callee_uid);
		}
		return;
	}

	//todo...����ͬһ������������ͨ��grpc֪ͨ��Ӧ�����������ı���Ϣ
	//TextChatMsgReq text_msg_req;
	//text_msg_req.set_fromuid(uid);
	//text_msg_req.set_touid(touid);
	//text_msg_req.set_thread_id(thread_id);
	//for (const auto& chat_data : chat_datas) {
	//	auto* text_msg = text_msg_req.add_textmsgs();
	//	text_msg->set_unique_id(chat_data->unique_id);
	//	text_msg->set_msgcontent(chat_data->content);
	//	text_msg->set_msg_id(chat_data->message_id);
	//	text_msg->set_chat_time(chat_data->chat_time);
	//}
	////����֪ͨ todo...
	//LOG_DEBUG("Notify text chat msg via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	//ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

void LogicSystem::VideoCallAccept(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto caller_uid = root["caller_uid"].asInt();
	auto call_id    = root["call_id"].asString();

	auto& cfg = ConfigMgr::Inst();

	// Bug#3 Fix: Build WebRTC params BEFORE the Defer, so CALL_ACCEPT_RSP (callee)
	// always carries complete room/ICE info regardless of which code path is taken.
	auto turn_ws_url = cfg["TurnServer"]["WsUrl"];
	if (turn_ws_url.empty())
	{
		// Fallback: use the public IP of the server where TurnServer (Node.js signaling) runs.
		// ws://127.0.0.1 would only work if the client runs on the same machine as the server,
		// which is never the case in production. Change this to match your actual deployment.
		turn_ws_url = "ws://127.0.0.1:3000";
	}

	Json::Value ice_servers(Json::arrayValue);
	{
		Json::Value turn_server;
		Json::Value turn_urls(Json::arrayValue);
		turn_urls.append("turn:47.108.113.95:3478?transport=udp");
		turn_urls.append("turn:47.108.113.95:3478?transport=tcp");
		turn_server["urls"]       = turn_urls;
		turn_server["username"]   = "webrtc";
		turn_server["credential"] = "520zxq20050713";
		ice_servers.append(turn_server);

		Json::Value stun_server;
		Json::Value stun_urls(Json::arrayValue);
		stun_urls.append("stun:47.108.113.95:3478");
		stun_server["urls"] = stun_urls;
		ice_servers.append(stun_server);
	}

	Json::Value rtvalue;
	rtvalue["error"]       = ErrorCodes::Success;
	rtvalue["call_id"]     = call_id;
	// room_id == call_id: both sides must join the same signaling room
	rtvalue["room_id"]     = call_id;
	rtvalue["turn_ws_url"] = turn_ws_url;
	rtvalue["ice_servers"] = ice_servers;

	// Defer: send CALL_ACCEPT_RSP to callee on function exit (rtvalue is fully populated above)
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CALL_ACCEPT_RSP);
		});

	// Look up which ChatServer the caller is currently on
	auto to_str    = std::to_string(caller_uid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value;
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip)
	{
		LOG_DEBUG("Video Call Accept - target user offline, caller_uid: " << caller_uid);
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	auto self_name = cfg["SelfServer"]["Name"];

	if (to_ip_value == self_name)
	{
		auto caller_session = UserMgr::GetInstance()->GetSession(caller_uid);
		if (!caller_session)
		{
			LOG_DEBUG("Video Call Accept - caller session not found, caller_uid: " << caller_uid);
			// Bug#3 sub-fix: set error so callee knows the call failed, not silent Success
			rtvalue["error"] = ErrorCodes::UidInvalid;
			return;
		}

		// Push CALL_ACCEPT_NOTIFY to caller with the same fully-populated rtvalue
		std::string notify_str = rtvalue.toStyledString();
		caller_session->Send(notify_str, ID_CALL_ACCEPT_NOTIFY);
		LOG_DEBUG("Notify Call Accept locally - caller_uid: " << caller_uid);
		return;
	}

	// TODO: caller on remote ChatServer - forward CALL_ACCEPT_NOTIFY via gRPC
	// callee CALL_ACCEPT_RSP will still be sent by Defer with complete WebRTC params
	LOG_DEBUG("Video Call Accept - caller on remote server, caller_uid: " << caller_uid << ", target_server: " << to_ip_value);
}

void LogicSystem::VideoCallReject(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto call_id = root["call_id"].asString();
	auto caller_uid = root["caller_uid"].asInt();
	auto reason = root["reason"].asString();

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CALL_REJECT_RSP);
		});

	//��ѯredis ��ȡcaller_uid��Ӧ��server ip
	auto to_str = std::to_string(caller_uid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value;
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		LOG_DEBUG("Video Call Invite - target user offline, to_uid: " << caller_uid);
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	//ֱ��֪ͨ�Է���֤ͨ������Ϣ
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(caller_uid);
		if (session) {
			//���ڴ��У�ֱ�ӷ���֪ͨ�Է�
			rtvalue["caller_uid"] = caller_uid;
			rtvalue["call_id"] = call_id;
			rtvalue["reason"] = reason;
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_CALL_REJECT_NOTIFY);
			LOG_DEBUG("Notify Call Incoming locally - to_uid: " << caller_uid);
		}
		return;
	}

	//todo...����ͬһ������������ͨ��grpc֪ͨ��Ӧ�����������ı���Ϣ
}

// 创建群聊逻辑
void LogicSystem::CreateGroupChatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto creator_uid = root["creator_uid"].asInt();
	auto group_name = root["group_name"].asString();
	LOG_INFO("Create group chat - creator_uid: " << creator_uid << ", group_name: " << group_name);

	// 解析成员列表
	std::vector<int> member_uids;
	const Json::Value members = root["member_uids"];
	for (const auto& member : members) {
		member_uids.push_back(member.asInt());
	}

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["creator_uid"] = creator_uid;
	rtvalue["group_name"] = group_name;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CREATE_GROUP_CHAT_RSP);
		});

	// 创建群聊
	int thread_id = 0;
	bool res = MysqlMgr::GetInstance()->CreateGroupChat(creator_uid, group_name, member_uids, thread_id);
	if (!res) {
		rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
		LOG_ERROR("Create group chat failed - creator_uid: " << creator_uid);
		return;
	}

	rtvalue["thread_id"] = thread_id;
	LOG_INFO("Create group chat success - thread_id: " << thread_id);

	// 获取群成员信息
	std::vector<std::shared_ptr<GroupMemberInfo>> group_members;
	MysqlMgr::GetInstance()->GetGroupMembers(thread_id, group_members);

	// 构建成员信息JSON
	for (const auto& member : group_members) {
		Json::Value member_json;
		member_json["uid"] = member->uid;
		member_json["name"] = member->name;
		member_json["icon"] = member->icon;
		member_json["role"] = member->role;
		rtvalue["members"].append(member_json);
	}

	// 通知所有成员被加入群聊
	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	for (int member_uid : member_uids) {
		if (member_uid == creator_uid) continue;  // 跳过创建者

		// 查询redis获取成员所在服务器
		auto member_str = std::to_string(member_uid);
		auto member_ip_key = USERIPPREFIX + member_str;
		std::string member_ip_value = "";
		bool b_ip = RedisMgr::GetInstance()->Get(member_ip_key, member_ip_value);

		if (!b_ip) {
			LOG_DEBUG("Member offline, skip notify - uid: " << member_uid);
			continue;
		}

		// 构建通知消息
		Json::Value notify;
		notify["error"] = ErrorCodes::Success;
		notify["thread_id"] = thread_id;
		notify["group_name"] = group_name;
		notify["creator_uid"] = creator_uid;

		// 添加所有成员信息
		for (const auto& member : group_members) {
			Json::Value member_json;
			member_json["uid"] = member->uid;
			member_json["name"] = member->name;
			member_json["icon"] = member->icon;
			member_json["role"] = member->role;
			notify["members"].append(member_json);
		}

		// 如果成员在本服务器，直接发送通知
		if (member_ip_value == self_name) {
			auto member_session = UserMgr::GetInstance()->GetSession(member_uid);
			if (member_session) {
				std::string notify_str = notify.toStyledString();
				member_session->Send(notify_str, ID_NOTIFY_GROUP_CHAT_CREATED);
				LOG_DEBUG("Notify group chat created locally - member_uid: " << member_uid);
			}
		}
		else {
			// TODO: 通过gRPC通知其他服务器
			LOG_DEBUG("Notify group chat created via grpc - member_uid: " << member_uid << ", server: " << member_ip_value);
		}
	}
}

// 获取群成员列表逻辑
void LogicSystem::GetGroupMembersHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto thread_id = root["thread_id"].asInt();
	auto requester_uid = root["requester_uid"].asInt();
	LOG_INFO("Get group members - thread_id: " << thread_id << ", requester_uid: " << requester_uid);

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["thread_id"] = thread_id;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_GET_GROUP_MEMBERS_RSP);
		});

	// 获取群成员列表
	std::vector<std::shared_ptr<GroupMemberInfo>> members;
	bool res = MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members);
	if (!res) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_ERROR("Get group members failed - thread_id: " << thread_id);
		return;
	}

	// 构建成员信息JSON
	for (const auto& member : members) {
		Json::Value member_json;
		member_json["uid"] = member->uid;
		member_json["name"] = member->name;
		member_json["icon"] = member->icon;
		member_json["role"] = member->role;
		rtvalue["members"].append(member_json);
	}

	LOG_DEBUG("Get group members success - thread_id: " << thread_id << ", member_count: " << members.size());
}

// 更新群公告逻辑：更新DB，然后通知群内所有在线成员（除发布者外）
void LogicSystem::UpdateGroupNoticeHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto thread_id = root["thread_id"].asInt();
	auto from_uid  = root["from_uid"].asInt();
	auto notice    = root["notice"].asString();
	LOG_INFO("Update group notice - thread_id: " << thread_id << ", from_uid: " << from_uid);

	Json::Value rtvalue;
	rtvalue["error"]     = ErrorCodes::Success;
	rtvalue["thread_id"] = thread_id;
	rtvalue["notice"]    = notice;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_UPDATE_GROUP_NOTICE_RSP);
	});

	// 1. 更新数据库
	bool ok = MysqlMgr::GetInstance()->UpdateGroupNotice(thread_id, notice);
	if (!ok) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_ERROR("UpdateGroupNotice DB failed - thread_id: " << thread_id);
		return;
	}

	// 2. 获取群所有成员
	std::vector<std::shared_ptr<GroupMemberInfo>> members;
	MysqlMgr::GetInstance()->GetGroupMembers(thread_id, members);

	// 3. 构建通知消息
	Json::Value notify;
	notify["thread_id"] = thread_id;
	notify["notice"]    = notice;
	std::string notify_str = notify.toStyledString();

	auto& cfg       = ConfigMgr::Inst();
	auto self_name  = cfg["SelfServer"]["Name"];

	for (const auto& member : members) {
		int member_uid = member->uid;
		if (member_uid == from_uid) continue;  // 跳过发布者

		// 查询 redis 获取成员所在服务器
		auto member_uid_str = std::to_string(member_uid);
		auto member_ip_key  = USERIPPREFIX + member_uid_str;
		std::string member_ip_value;
		bool b_ip = RedisMgr::GetInstance()->Get(member_ip_key, member_ip_value);
		if (!b_ip) {
			LOG_DEBUG("Member offline, skip notify group notice - uid: " << member_uid);
			continue;
		}

		if (member_ip_value == self_name) {
			// 在本服务器，直接推送
			auto member_session = UserMgr::GetInstance()->GetSession(member_uid);
			if (member_session) {
				member_session->Send(notify_str, ID_NOTIFY_GROUP_NOTICE_UPDATE);
				LOG_DEBUG("Notify group notice update locally - member_uid: " << member_uid);
			}
		} else {
			// TODO: 通过 gRPC 通知其他服务器节点
			LOG_DEBUG("Notify group notice update via grpc - member_uid: " << member_uid << ", server: " << member_ip_value);
		}
	}

	LOG_INFO("Update group notice success - thread_id: " << thread_id << ", notified members: " << members.size() - 1);
}

bool LogicSystem::isPureDigit(const std::string& str)
{
	for (unsigned char c : str) {
		if (!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	//�����Ȳ�redis�в�ѯ�û���Ϣ
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		LOG_DEBUG("Get user by uid from redis - uid: " << uid << ", name: " << name);

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = std::stoi(uid_str);
	//redis��û�У����ѯmysql
	//��ѯ���ݿ�
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_WARN("Get user by uid failed - uid not found: " << uid);
		return;
	}

	//�����ݿ���ȡ��д��redis����
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	//��������
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	LOG_DEBUG("Get user by uid from mysql - uid: " << uid << ", name: " << user_info->name);
}

void LogicSystem::GetUserByName(std::string name, Json::Value& rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	//�����Ȳ�redis�в�ѯ�û���Ϣ
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base) {
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		LOG_DEBUG("Get user by name from redis - name: " << name);

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	//redis��û�У����ѯmysql
	//��ѯ���ݿ�
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_WARN("Get user by name failed - name not found: " << name);
		return;
	}

	//�����ݿ���ȡ��д��redis����
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;

	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	
	//��������
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
	LOG_DEBUG("Get user by name from mysql - name: " << name);
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo>& userinfo)
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
		LOG_DEBUG("Get base info from redis - uid: " << uid << ", name: " << userinfo->name);
	}
	else
	{
		//redis��û�У����ѯmysql
		//��ѯ���ݿ�
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			LOG_WARN("Get base info failed - uid not found: " << uid);
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
		LOG_DEBUG("Get base info from mysql - uid: " << uid << ", name: " << userinfo->name);
	}

	return true;
}

bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list) {
	//��mysql��ȡ���������б�
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	//��mysql��ȡ�����б�
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}

bool LogicSystem::GetUserThreads(int64_t userId, int64_t last_id, int page_size,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& load_more, int& next_last_id)
{
	return MysqlMgr::GetInstance()->GetUserThreads(userId,last_id,page_size,threads,load_more,next_last_id);
}

