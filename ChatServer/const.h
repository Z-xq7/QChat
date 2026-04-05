#pragma once
#include <functional>

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误
	RPCFailed = 1002,  //RPC调用失败
	VarifyExpired = 1003, //验证码过期
	VarifyCodeErr = 1004, //验证码错误
	UserExist = 1005,       //用户已经存在
	PasswdErr = 1006,    //密码错误
	EmailNotMatch = 1007,  //邮箱不匹配
	PasswdUpFailed = 1008,  //密码更新失败
	PasswdInvalid = 1009,   //密码无效
	TokenInvalid = 1010,   //Token失效
	UidInvalid = 1011,  //uid无效
	CREATE_CHAT_FAILED = 1012, //创建聊天线程失败
	LOAD_CHAT_MSG_FAILED = 1013, //加载聊天消息失败
};


// Defer��
class Defer {
public:
	// ����һ��lambda����ʽ���ߺ���ָ��
	Defer(std::function<void()> func) : func_(func) {}

	// ����������ִ�д���ĺ���
	~Defer() {
		func_();
	}

private:
	std::function<void()> func_;
};

#define MAX_LENGTH  1024*2
//ͷ���ܳ���
#define HEAD_TOTAL_LEN 4
//ͷ��id����
#define HEAD_ID_LEN 2
//ͷ�����ݳ���
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000


enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005,				//�û���½
	MSG_CHAT_LOGIN_RSP = 1006,			//�û���½�ذ�
	ID_SEARCH_USER_REQ = 1007,			//�û���������
	ID_SEARCH_USER_RSP = 1008,			//�����û��ذ�
	ID_ADD_FRIEND_REQ = 1009,			//�������Ӻ�������
	ID_ADD_FRIEND_RSP  = 1010,			//�������Ӻ��ѻظ�
	ID_NOTIFY_ADD_FRIEND_REQ = 1011,	//֪ͨ�û����Ӻ�������
	ID_AUTH_FRIEND_REQ = 1013,			//��֤��������
	ID_AUTH_FRIEND_RSP = 1014,			//��֤���ѻظ�
	ID_NOTIFY_AUTH_FRIEND_REQ = 1015,	//֪ͨ�û���֤��������
	ID_TEXT_CHAT_MSG_REQ = 1017,		//�ı�������Ϣ����
	ID_TEXT_CHAT_MSG_RSP = 1018,		//�ı�������Ϣ�ظ�
	ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //֪ͨ�û��ı�������Ϣ
	ID_NOTIFY_OFF_LINE_REQ = 1021,		//֪ͨ�û�����
	ID_HEART_BEAT_REQ = 1023,			//��������
	ID_HEART_BEAT_RSP = 1024,			//�����ظ�
	ID_LOAD_CHAT_THREAD_REQ = 1025,		//���������߳�����
	ID_LOAD_CHAT_THREAD_RSP = 1026,		//���������̻߳ظ�
	ID_CREATE_PRIVATE_CHAT_REQ = 1027,	//����˽���߳�����
	ID_CREATE_PRIVATE_CHAT_RSP = 1028,	//����˽���̻߳ظ�
	ID_LOAD_CHAT_MSG_REQ = 1029,		//����������Ϣ
	ID_LOAD_CHAT_MSG_RSP = 1030,		//����������Ϣ

	ID_IMG_CHAT_MSG_REQ = 1035,			//ͼƬ������Ϣ����
	ID_IMG_CHAT_MSG_RSP = 1036,			//ͼƬ������Ϣ�ظ�
	ID_NOTIFY_IMG_CHAT_MSG_REQ = 1039,	//֪ͨ�û�ͼƬ������Ϣ
	ID_FILE_INFO_SYNC_REQ = 1041,		//�ļ���Ϣͬ������
	ID_FILE_INFO_SYNC_RSP = 1042,		//�ļ���Ϣͬ���ظ�

	// ��Ƶͨ�����Э��ID
	ID_CALL_INVITE_REQ = 1050,        // ��Ƶͨ����������
	ID_CALL_INVITE_RSP = 1051,        // ��Ƶͨ��������Ӧ
	ID_CALL_INCOMING_NOTIFY = 1052,   // ����֪ͨ
	ID_CALL_ACCEPT_REQ = 1053,        // ����ͨ������
	ID_CALL_ACCEPT_RSP = 1054,        // ����ͨ����Ӧ
	ID_CALL_ACCEPT_NOTIFY = 1055,     // ����ͨ��֪ͨ
	ID_CALL_REJECT_REQ = 1056,        // �ܾ�ͨ������
	ID_CALL_REJECT_RSP = 1057,        // �ܾ�ͨ����Ӧ
	ID_CALL_REJECT_NOTIFY = 1058,     // �ܾ�ͨ��֪ͨ
	ID_CALL_HANGUP_REQ = 1059,        // �Ҷ�ͨ������
	ID_CALL_HANGUP_RSP = 1060,        // �Ҷ�ͨ����Ӧ
	ID_CALL_HANGUP_NOTIFY = 1061,      // �Ҷ�ͨ��֪ͨ

	ID_FILE_CHAT_MSG_REQ = 1062,
	ID_FILE_CHAT_MSG_RSP = 1063,
	ID_NOTIFY_FILE_CHAT_MSG_REQ = 1064,

	// 群聊相关消息ID
	ID_CREATE_GROUP_CHAT_REQ = 1070,	// 创建群聊请求
	ID_CREATE_GROUP_CHAT_RSP = 1071,	// 创建群聊响应
	ID_NOTIFY_GROUP_CHAT_CREATED = 1072,	// 通知用户被加入群聊
	ID_GET_GROUP_MEMBERS_REQ = 1073,	// 获取群成员列表请求
	ID_GET_GROUP_MEMBERS_RSP = 1074,	// 获取群成员列表响应
	ID_UPDATE_GROUP_NOTICE_REQ = 1075,	// 更新群公告请求
	ID_UPDATE_GROUP_NOTICE_RSP = 1076,	// 更新群公告响应
	ID_NOTIFY_GROUP_NOTICE_UPDATE = 1077,	// 通知群成员群公告更新

	ID_GET_FRIEND_ONLINE_STATUS_REQ = 1080,	// 查询好友在线状态请求
	ID_GET_FRIEND_ONLINE_STATUS_RSP = 1081,	// 查询好友在线状态响应
	ID_NOTIFY_USER_ONLINE = 1082,				// 通知好友用户上线
	ID_NOTIFY_USER_OFFLINE = 1083,			// 通知好友用户下线

	// 已读回执相关消息ID
	ID_GET_UNREAD_COUNTS_REQ = 1090,			// 获取未读消息数请求
	ID_GET_UNREAD_COUNTS_RSP = 1091,			// 获取未读消息数响应
	ID_MARK_MSG_READ_REQ = 1092,				// 标记消息已读请求
	ID_MARK_MSG_READ_RSP = 1093,				// 标记消息已读响应
	ID_NOTIFY_MSG_READ = 1094,				// 通知发送方消息已读
};

//��Ϣ״̬
enum MsgStatus {
	UN_READ = 0,  //�Է�δ��
	SEND_FAILED = 1,  //����ʧ��
	READED = 2,  //�Է��Ѷ�
	UN_UPLOAD = 3 //δ�ϴ����
};

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define NAME_INFO  "nameinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"

//�ֲ�ʽ���ĳ���ʱ��
#define LOCK_TIME_OUT 10
//�ֲ�ʽ��������ʱ��
#define ACQUIRE_TIME_OUT 5
//��������ʱ��
#define HEARTBEAT_EXPIRE_TIME 20


