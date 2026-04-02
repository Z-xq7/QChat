#pragma once
#include <functional>


enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json��������
	RPCFailed = 1002,  //RPC�������
	VarifyExpired = 1003, //��֤�����
	VarifyCodeErr = 1004, //��֤�����
	UserExist = 1005,       //�û��Ѿ�����
	PasswdErr = 1006,    //�������
	EmailNotMatch = 1007,  //���䲻ƥ��
	PasswdUpFailed = 1008,  //��������ʧ��
	PasswdInvalid = 1009,   //�������ʧ��
	TokenInvalid = 1010,   //TokenʧЧ
	UidInvalid = 1011,  //uid��Ч
	CREATE_CHAT_FAILED = 1012, //���������߳�ʧ��
	LOAD_CHAT_MSG_FAILED = 1013, //����������Ϣʧ��
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


