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
	//由0变为1，则通知信号
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
		//判断队列为空则用条件变量阻塞线程并释放锁
		while (_msg_que.empty() && !_b_stop) {
			_consume.wait(unique_lk);
		}
		//判断是否为关闭状态，把所有逻辑执行完后再退出循环
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
		//如果没有停止，则说明队列有数据
		auto msg_node = _msg_que.front();
		LOG_DEBUG("recv_msg id is " << msg_node->_recvnode->_msg_id);
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
		if (call_back_iter == _fun_callbacks.end()) {
			_msg_que.pop();
			LOG_WARN("msg id [" << msg_node->_recvnode->_msg_id << "] handler not found");
			continue;
		}

		// 在执行逻辑前先解除 LogicSystem 的 _mutex，避免死锁
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

	_fun_callbacks[ID_CALL_INVITE_REQ] = std::bind(&LogicSystem::VideoCallInvite, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CALL_ACCEPT_REQ] = std::bind(&LogicSystem::VideoCallAccept, this,
		placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_CALL_REJECT_REQ] = std::bind(&LogicSystem::VideoCallReject, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
}

//处理用户登录的逻辑：1.先从redis中获取用户token是否正确；2.如果token正确，说明登录成功，则从数据库获取用户个人信息；
//3.从数据库获取申请列表；4.从数据库获取好友列表；5.增加登录人数；6.session绑定用户uid；7.为用户设置登录ip server（服务名）；8.uid和session绑定关系并返回给客户端
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

	//从redis中获取用户token是否正确
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

	//从数据库获取用户个人信息
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

	//从数据库获取好友申请列表
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

	//获取好友列表
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

	//登录人数统计
	auto server_name = ConfigMgr::Inst().GetValue("SelfServer", "Name");
	////********** 这里被优化了，先不再用 **********
	//RedisMgr::GetInstance()->IncreaseCount(server_name);
	{
		//******* 此处加上分布式锁，避免多线程占用登录 *******
		//拼接用户ip对应的key
		auto lock_key = LOCK_PREFIX + uid_str;
		auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);

		//设置defer机制
		Defer defer2([this, identifier, lock_key]() {
			RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
			});

		//此处判断该用户是否在本处或者本机正在登录
		std::string uid_ip_value = "";
		auto uid_ip_key = USERIPPREFIX + uid_str;
		bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
		//说明用户已经登录了，此处应该踢掉之前的该用户登录状态
		if (b_ip) {
			//获取当前服务器ip信息
			auto& cfg = ConfigMgr::Inst();
			auto self_name = cfg["SelfServer"]["Name"];
			//如果之前登录的服务器与当前相同，则直接在本服务器踢掉
			if (uid_ip_value == self_name) {
				//查找旧的连接
				auto old_session = UserMgr::GetInstance()->GetSession(uid);

				//此处应该发送踢掉信息
				if (old_session) {
					LOG_INFO("Kick old session - uid: " << uid << ", session_id: " << old_session->GetSessionId());
					old_session->NotifyOffline(uid);
					//清理旧的连接
					_p_server->ClearSession(old_session->GetSessionId());
				}
			}
			else {
				//如果不是本服务器，则通知grpc通知该服务器踢掉
				//发送通知
				LOG_INFO("Notify kick user via grpc - uid: " << uid << ", target_server: " << uid_ip_value);
				KickUserReq kick_req;
				kick_req.set_uid(uid);
				ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
			}
		}
		//session绑定用户uid
		session->SetUserId(uid);
		//为用户设置登录ip server（服务名）
		std::string  ipkey = USERIPPREFIX + uid_str;
		RedisMgr::GetInstance()->Set(ipkey, server_name);
		//uid和session绑定关系,用于以后查找
		UserMgr::GetInstance()->SetUserSession(uid, session);
		std::string  uid_session_key = USER_SESSION_PREFIX + uid_str;
		RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId());

	}
	LOG_INFO("User login success - uid: " << uid << ", name: " << user_info->name);
	return;
}

//处理查询用户信息的逻辑
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

	//判断是不是纯数字，如果是纯数字说明是uid，否则说明是用户名
	bool b_digit = isPureDigit(uid_str);
	if (b_digit) {
		GetUserByUid(uid_str, rtvalue);
	}
	else {
		GetUserByName(uid_str, rtvalue);
	}
	return;
}

//处理添加好友的业务逻辑：1.先更新数据库；2.查询redis 获取touid对应的server ip；
//3.如果在本服务器，则直接通知对方收到好友信息；4.如果不在本服务器，则通过grpc通知对应服务器发送好友信息
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

	//先更新数据库
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid,desc,bakname);

	//查询redis 获取touid对应的server ip
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

	//从数据库获取申请人基本信息
	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	//直接通知对方收到好友信息
	//如touid在本服务器登录时，则直接从内存中获取session并且通知他
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中，直接发送通知对方
			Json::Value  notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = apply_info->name;
			notify["desc"] = desc;
			//如果获取申请人基本信息成功，则将基本信息也发送给对方便于对方展示
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

	//如touid不在本服务器登录时，则通过grpc通知对应服务器发送好友信息
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
	//发送通知
	LOG_DEBUG("Notify add friend via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value,add_req);
}

//处理好友申请认证的逻辑：1.先更新数据库；2.查询redis 获取申请人对应的server ip；3.如果在本服务器，则直接通知对方认证结果
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
	//从数据库获取对方用户个人信息
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

	//先更新数据库，同步添加好友和更新数据库中添加好友关系并将申请状态改为同意(修改：此处放在后面，这里不增加链接)
	//MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	std::vector<std::shared_ptr<message::AddFriendMsg>> chat_datas;

	//向数据库添加好友
	MysqlMgr::GetInstance()->AddFriend(uid, touid,back_name,chat_datas);

	//查询redis 获取touid对应的server ip
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
	//直接通知对方认证通过的信息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中，直接发送通知对方
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

	//不在内存中，则不在本服务器，则通过grpc通知对应服务器认证结果
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

	//发送通知
	LOG_DEBUG("Notify auth friend via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

//处理文本消息的逻辑：1.先从数据库获取会话或创建信息；2.查询redis 找到接收者对应的server ip；3.如果在本服务器，则直接通知对方文本消息；
void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	auto thread_id = root["thread_id"].asInt();

	const Json::Value  arrays = root["text_array"];
	
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["fromuid"] = uid;
	rtvalue["touid"] = touid;
	rtvalue["thread_id"] = thread_id;

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

	//存入数据库
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

	//查询redis 获取touid对应的server ip
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
	//直接通知对方认证通过的信息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session) {
			//在内存中，直接发送通知对方
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

	//发送通知 todo...
	LOG_DEBUG("Notify text chat msg via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

//心跳处理函数：1.更新session中的心跳时间；2.如果心跳过期则说明客户端已经掉线，进行相应的处理
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

//获取数据库存储的对话线程信息
void LogicSystem::GetUserThreadsHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	//从数据库查chat_threads记录
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

	//拉取一次查询，看看是否还有下一页，如有下一页传last_id
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
	//将threads数据写入json数组
	for (auto& thread : threads)
	{
		Json::Value thread_value;
		thread_value["thread_id"] = int(thread->_thread_id);
		thread_value["typr"] = thread->_type;
		thread_value["user1_id"] = thread->_user1_id;
		thread_value["user2_id"] = thread->_user2_id;
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
	//设置图片文件状态为未上传，等文件上传成功后再更新状态并通知对方
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

	//存入数据库
	MysqlMgr::GetInstance()->AddChatMsg(chat_msg);

	rtvalue["message_id"] = chat_msg->message_id;
	//这里可以不用defer机制，因为后续没有需要处理的逻辑了，直接发送响应即可
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_IMG_CHAT_MSG_RSP);
		});

	//发送通知 todo... 以后等文件上传成功再通知
}

void LogicSystem::VideoCallInvite(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto caller_uid = root["caller_uid"].asInt();	//发起者的uid当做room_id房间号
	auto caller_nick = root["caller_nick"].asString();
	auto callee_uid = root["callee_uid"].asInt();
	std::string call_id = std::to_string(caller_uid);

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CALL_INVITE_RSP);
		});

	//查询redis 获取callee_uid对应的server ip
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
	//直接通知对方认证通过的信息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(callee_uid);
		if (session) {
			//在内存中，直接发送通知对方
			rtvalue["caller_nick"] = caller_nick;
			rtvalue["caller_uid"] = caller_uid;
			rtvalue["call_id"] = call_id;	//暂时先用caller_uid当做call_id，后续可以优化成全局唯一的call_id
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_CALL_INCOMING_NOTIFY);
			LOG_DEBUG("Notify Call Incoming locally - to_uid: " << callee_uid);
		}
		return;
	}

	//todo...不在同一个服务器，则通过grpc通知对应服务器发送文本消息
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
	////发送通知 todo...
	//LOG_DEBUG("Notify text chat msg via grpc - to_uid: " << touid << ", target_server: " << to_ip_value);
	//ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

void LogicSystem::VideoCallAccept(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	// 发起方 uid，当做房间号使用
	auto caller_uid = root["caller_uid"].asInt();
	// 通话 id，后续可以改成全局唯一字符串，这里先按 int 处理
	auto call_id = root["call_id"].asString();

	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;

	// 统一的响应：无论后面是否成功给对端发通知，先把当前端的响应发回去
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_CALL_ACCEPT_RSP);
		});

	// 查询 redis，获取发起方当前所在的 ChatServer 名称
	auto to_str = std::to_string(caller_uid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value;
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip)
	{
		LOG_DEBUG("Video Call Accept - target user offline, caller_uid: " << caller_uid);
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	auto& cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	// 只处理“对端在本机 ChatServer” 的简单情况
	if (to_ip_value == self_name)
	{
		auto caller_session = UserMgr::GetInstance()->GetSession(caller_uid);
		if (!caller_session)
		{
			LOG_DEBUG("Video Call Accept - caller session not found, caller_uid: " << caller_uid);
			return;
		}

		// 组织给发起方的通知数据
		rtvalue["error"] = ErrorCodes::Success;
		rtvalue["call_id"] = call_id;
		// 先用 caller_uid 作为 room_id，保证两端一致地 join 同一个房间
		rtvalue["room_id"] = call_id;

		// WebRTC 信令服务器（Node TurnServer）的 WebSocket 地址
		// 建议放到配置文件中，这里先读取配置（如无则可以先写死调试）
		auto turn_ws_url = cfg["TurnServer"] ["WsUrl"];
		if (turn_ws_url.empty())
		{
			// 如果暂时没配置，则使用一个占位值，方便客户端调试
			turn_ws_url = "ws://127.0.0.1:3000";
		}
		rtvalue["turn_ws_url"] = turn_ws_url;

		// 组装 ICE 服务器配置，供客户端创建 WebRTC PeerConnection 使用
		Json::Value ice_servers(Json::arrayValue);

		// TURN 服务器（使用你在云服务器上部署的 coturn 配置）
		Json::Value turn_server;
		Json::Value turn_urls(Json::arrayValue);
		// 这里的地址和端口来自 coturn：external-ip=47.113.108.95, listening-port=3478
		turn_urls.append("turn:47.113.108.95:3478?transport=udp");
		turn_urls.append("turn:47.113.108.95:3478?transport=tcp");
		turn_server["urls"] = turn_urls;
		// 对应 coturn 配置：user=webrtc:520zxq20050713
		turn_server["username"] = "webrtc";
		turn_server["credential"] = "520zxq20050713";
		ice_servers.append(turn_server);

		// 可选：再加一个 STUN，仅做连通性探测
		Json::Value stun_server;
		Json::Value stun_urls(Json::arrayValue);
		stun_urls.append("stun:47.113.108.95:3478");
		stun_server["urls"] = stun_urls;
		ice_servers.append(stun_server);

		rtvalue["ice_servers"] = ice_servers;

		std::string rtvalue_str = rtvalue.toStyledString();
		caller_session->Send(rtvalue_str, ID_CALL_ACCEPT_NOTIFY);
		LOG_DEBUG("Notify Call Accept locally - caller_uid: " << caller_uid);
		return;
	}

	// todo: 对端在其他 ChatServer 上时，通过 gRPC 转发 CALL_ACCEPT_NOTIFY
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

	//查询redis 获取caller_uid对应的server ip
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
	//直接通知对方认证通过的信息
	if (to_ip_value == self_name) {
		auto session = UserMgr::GetInstance()->GetSession(caller_uid);
		if (session) {
			//在内存中，直接发送通知对方
			rtvalue["caller_uid"] = caller_uid;
			rtvalue["call_id"] = call_id;
			rtvalue["reason"] = reason;
			std::string return_str = rtvalue.toStyledString();
			session->Send(return_str, ID_CALL_REJECT_NOTIFY);
			LOG_DEBUG("Notify Call Incoming locally - to_uid: " << caller_uid);
		}
		return;
	}

	//todo...不在同一个服务器，则通过grpc通知对应服务器发送文本消息
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

	//先优先查redis中查询用户信息
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
	//redis里没有，则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_WARN("Get user by uid failed - uid not found: " << uid);
		return;
	}

	//从数据库拉取，写入redis缓存
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

	//返回数据
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

	//先优先查redis中查询用户信息
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

	//redis里没有，则查询mysql
	//查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr) {
		rtvalue["error"] = ErrorCodes::UidInvalid;
		LOG_WARN("Get user by name failed - name not found: " << name);
		return;
	}

	//从数据库拉取，写入redis缓存
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
	
	//返回数据
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
	//先优先查redis中查询用户信息
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
		//redis里没有，则查询mysql
		//查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr) {
			LOG_WARN("Get base info failed - uid not found: " << uid);
			return false;
		}

		userinfo = user_info;

		//从数据库拉取，写入redis缓存
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
	//从mysql获取好友申请列表
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>>& user_list) {
	//从mysql获取好友列表
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}

bool LogicSystem::GetUserThreads(int64_t userId, int64_t last_id, int page_size,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& load_more, int& next_last_id)
{
	return MysqlMgr::GetInstance()->GetUserThreads(userId,last_id,page_size,threads,load_more,next_last_id);
}
