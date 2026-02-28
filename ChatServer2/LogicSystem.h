#pragma once
#include "Singleton.h"
#include <queue>
#include <thread>
#include "CSession.h"
#include <queue>
#include <map>
#include <functional>
#include "const.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <unordered_map>
#include "data.h"

class CServer;
typedef  function<void(shared_ptr<CSession>, const short &msg_id, const string &msg_data)> FunCallBack;
class LogicSystem:public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void PostMsgToQue(shared_ptr < LogicNode> msg);
	void SetServer(std::shared_ptr<CServer> pserver);

private:
	LogicSystem();
	void DealMsg();
	void RegisterCallBacks();

	//登录处理函数：1.先从redis获取用户token是否正确；2.如果token正确则说明登录成功，接着从数据库获取用户基本信息；
	//3.从数据库获取申请列表；4.从数据库获取好友列表；5.增加登录数量；6.session绑定用户uid；7.为用户设置登录ip server的名字；8.uid和session绑定管理，方便踢人操作
	void LoginHandler(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data);
	//处理用户搜索的逻辑：1.先判断客户端搜索的是uid还是名字；2.根据搜索条件从数据库获取用户信息；3.把用户信息返回给客户端
	void SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//处理添加好友申请的逻辑：1.先更新数据库；2.查询redis 查找touid对应的server ip；3.如果在本服务器则直接通知对方有申请消息；
	//4.如果不在本服务器则通过grpc通知对应服务器有申请消息
	void AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//处理好友申请认证的逻辑：1.先更新数据库；2.查询redis 查找申请人对应的server ip；3.如果在本服务器则直接通知对方认证结果；
	void AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//处理聊天消息的逻辑：1.先从数据库获取发送者基本信息；2.查询redis 查找接收者对应的server ip；3.如果在本服务器则直接通知对方有消息；
	//4.如果不在本服务器则通过grpc通知对应服务器有消息
	void DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//心跳处理函数：1.更新session中的心跳时间；2.如果心跳过期则说明客户端已经掉线，进行相应的处理
	void HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//获取服务器存储的聊天线程信息
	void GetUserThreadsHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//处理创建私聊线程的逻辑：1.先从数据库创建私聊线程；2.查询redis 查找对方对应的server ip；3.如果在本服务器则直接通知对方有新线程；4.如果不在本服务器则通过grpc通知对应服务器有新线程
	void CreatePrivateChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);

	//判断字符串是否为纯数字（用来判断客户端搜索的是uid还是名字）
	bool isPureDigit(const std::string& str);
	//获取用户信息的函数，分别根据uid和名字获取
	void GetUserByUid(std::string uid_str, Json::Value& rtvalue);
	void GetUserByName(std::string name, Json::Value& rtvalue);
	//获取用户基本信息的函数，主要是为了获取用户的头像、昵称等信息，减少数据库访问
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
	//获取好友申请信息列表
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list);
	//获取好友列表
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> & user_list);
	//获取聊天线程
	bool GetUserThreads(int64_t userId, int64_t last_id, int page_size,
		std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& load_more, int& next_last_id);

	std::thread _worker_thread;
	std::queue<shared_ptr<LogicNode>> _msg_que;
	std::mutex _mutex;
	std::condition_variable _consume;
	bool _b_stop;
	std::map<short, FunCallBack> _fun_callbacks;
	std::shared_ptr<CServer> _p_server;
};

