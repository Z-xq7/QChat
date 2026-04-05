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
	std::string getCurrentTimestamp();
	// 通知好友用户上线
	void NotifyFriendsUserOnline(int uid);
	// 通知好友用户下线
	void NotifyFriendsUserOffline(int uid);

private:
	LogicSystem();
	void DealMsg();
	void RegisterCallBacks();

	//��¼����������1.�ȴ�redis��ȡ�û�token�Ƿ���ȷ��2.���token��ȷ��˵����¼�ɹ������Ŵ����ݿ��ȡ�û�������Ϣ��
	//3.�����ݿ��ȡ�����б���4.�����ݿ��ȡ�����б���5.���ӵ�¼������6.session���û�uid��7.Ϊ�û����õ�¼ip server�����֣�8.uid��session�󶨹������������˲���
	void LoginHandler(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data);
	//�����û��������߼���1.���жϿͻ�����������uid�������֣�2.�����������������ݿ��ȡ�û���Ϣ��3.���û���Ϣ���ظ��ͻ���
	void SearchInfo(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//�������Ӻ���������߼���1.�ȸ������ݿ⣻2.��ѯredis ����touid��Ӧ��server ip��3.����ڱ���������ֱ��֪ͨ�Է���������Ϣ��
	//4.������ڱ���������ͨ��grpc֪ͨ��Ӧ��������������Ϣ
	void AddFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//��������������֤���߼���1.�ȸ������ݿ⣻2.��ѯredis ���������˶�Ӧ��server ip��3.����ڱ���������ֱ��֪ͨ�Է���֤�����
	void AuthFriendApply(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//����������Ϣ���߼���1.�ȴ����ݿ��ȡ�����߻�����Ϣ��2.��ѯredis ���ҽ����߶�Ӧ��server ip��3.����ڱ���������ֱ��֪ͨ�Է�����Ϣ��
	//4.������ڱ���������ͨ��grpc֪ͨ��Ӧ����������Ϣ
	void DealChatTextMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//��������������1.����session�е�����ʱ�䣻2.�������������˵���ͻ����Ѿ����ߣ�������Ӧ�Ĵ���
	void HeartBeatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//��ȡ�������洢�������߳���Ϣ
	void GetUserThreadsHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//��������˽���̵߳��߼���1.�ȴ����ݿⴴ��˽���̣߳�2.��ѯredis ���ҶԷ���Ӧ��server ip��3.����ڱ���������ֱ��֪ͨ�Է������̣߳�4.������ڱ���������ͨ��grpc֪ͨ��Ӧ�����������߳�
	void CreatePrivateChat(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//��������������Ϣ���߼���1.�ȴ����ݿ��ȡ������Ϣ��2.��������Ϣ���ظ��ͻ���
	void LoadChatMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//��������ͼƬ��Ϣ���߼�
	void DealChatImgMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//���������ļ���Ϣ���߼�
	void DealChatFileMsg(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//������Ƶͨ��������߼���1.�ȴ����ݿ��ȡ�����߻�����Ϣ��2.��ѯredis ���ҽ����߶�Ӧ��server ip��3.����ڱ���������ֱ��֪ͨ�Է�����Ƶͨ�����룻4.������ڱ���������ͨ��grpc֪ͨ��Ӧ����������Ƶͨ������
	void VideoCallInvite(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//������Ƶͨ�����ܵ��߼�
	void VideoCallAccept(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	//������Ƶͨ���ܾ����߼�
	void VideoCallReject(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);

	// 创建群聊逻辑
	void CreateGroupChatHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	// 获取群成员列表逻辑
	void GetGroupMembersHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	// 更新群公告逻辑：更新DB并通知群内所有在线成员
	void UpdateGroupNoticeHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	// 查询好友在线状态逻辑
	void GetFriendOnlineStatusHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);

	// 已读回执相关逻辑
	// 获取用户未读消息数
	void GetUnreadCountsHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);
	// 标记消息已读并通知发送方
	void MarkMsgReadHandler(std::shared_ptr<CSession> session, const short& msg_id, const string& msg_data);

	//�ж��ַ����Ƿ�Ϊ�����֣������жϿͻ�����������uid�������֣�
	bool isPureDigit(const std::string& str);
	//��ȡ�û���Ϣ�ĺ������ֱ����uid�����ֻ�ȡ
	void GetUserByUid(std::string uid_str, Json::Value& rtvalue);
	void GetUserByName(std::string name, Json::Value& rtvalue);
	//��ȡ�û�������Ϣ�ĺ�������Ҫ��Ϊ�˻�ȡ�û���ͷ���ǳƵ���Ϣ���������ݿ����
	bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
	//��ȡ����������Ϣ�б�
	bool GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>>& list);
	//��ȡ�����б�
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> & user_list);
	//��ȡ�����߳�
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

