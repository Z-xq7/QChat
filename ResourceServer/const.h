#pragma once
#include <functional>

//�����Ϣ����
#define MAX_LENGTH  1024*1024
//ͷ���ܳ���
#define HEAD_TOTAL_LEN 6
//ͷ��id����
#define HEAD_ID_LEN 2
//ͷ�����ݳ���
#define HEAD_DATA_LEN 4
#define MAX_RECVQUE  2000000
#define MAX_SENDQUE 2000000

//������ļ��Ĵ�С
#define MAX_FILE_LEN 1024*256

//�ֲ�ʽ���ĳ���ʱ��
#define LOCK_TIME_OUT 10
//�ֲ�ʽ��������ʱ��
#define ACQUIRE_TIME_OUT 5

//4���߼�������
#define LOGIC_WORKER_COUNT 4
//4���ļ�������
#define FILE_WORKER_COUNT 4
//4�����ع�����
#define DOWN_LOAD_WORKER_COUNT	4

enum MSG_IDS {
	ID_TEST_MSG_REQ = 1001,       //������Ϣ
	ID_TEST_MSG_RSP = 1002,       //������Ϣ�ذ�
	ID_UPLOAD_FILE_REQ = 1003,    //�����ļ�����
	ID_UPLOAD_FILE_RSP = 1004,    //�����ļ��ظ�
	ID_SYNC_FILE_REQ = 1005,      //ͬ���ļ���Ϣ����
	ID_SYNC_FILE_RSP = 1006,      //ͬ���ļ��ظ��ظ�
	ID_UPLOAD_HEAD_ICON_REQ = 1031,      //�ϴ�ͷ������
	ID_UPLOAD_HEAD_ICON_RSP = 1032,      //�ϴ�ͷ��ظ�
	ID_DOWN_LOAD_FILE_REQ = 1033,        //�����ļ�����
	ID_DOWN_LOAD_FILE_RSP = 1034,        //�����ļ��ظ�
	ID_IMG_CHAT_UPLOAD_REQ = 1037,        //�ϴ�����ͼƬ��Դ
	ID_IMG_CHAT_UPLOAD_RSP = 1038,        //�ϴ�����ͼƬ��Դ�ظ�
	ID_NOTIFY_IMG_CHAT_MSG_REQ = 1039, //֪ͨ�û�ͼƬ������Ϣ
	ID_FILE_INFO_SYNC_REQ = 1041,      //�ļ���Ϣͬ������
	ID_FILE_INFO_SYNC_RSP = 1042,       //�ļ���Ϣͬ���ظ�
	ID_IMG_CHAT_CONTINUE_UPLOAD_REQ = 1043,  //��������ͼƬ��Դ����
	ID_IMG_CHAT_CONTINUE_UPLOAD_RSP = 1044,  //��������ͼƬ��Դ�ظ�
	ID_IMG_CHAT_DOWN_INFO_SYNC_REQ = 1045,   //��ȡ����ͼƬ���ص�ͬ����Ϣ
	ID_IMG_CHAT_DOWN_INFO_SYNC_RSP = 1046,    //��ȡ����ͼƬ���ص�ͬ����Ϣ�ذ�
	ID_IMG_CHAT_DOWN_REQ = 1047,    //����ͼƬ��������
	ID_IMG_CHAT_DOWN_RSP = 1048,     //����ͼƬ���ػظ�
	ID_FILE_CHAT_DOWN_REQ = 1065,    //聊天文件下载请求
	ID_FILE_CHAT_DOWN_RSP = 1066     //聊天文件下载回复
};

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
	FileNotExists = 1012, //�ļ�������
	FileSaveRedisFailed = 1013, //�ļ��洢redisʧ��
	CreateFilePathFailed = 1014, //�ļ�·������ʧ��
	FileWritePermissionFailed = 1015,  //�ļ�дȨ�޲���
	FileReadPermissionFailed = 1016,  //�ļ���Ȩ�޲���
	FileSeqInvalid = 1017,     //�ļ���������
	FileOffsetInvalid = 1018,   //�ļ�ƫ��������
	FileReadFailed = 1019,      //�ļ���ȡʧ��
	RedisReadErr = 1020,        //redis��ȡʧ��
	ServerIpErr = 1021,          //server ip����
	MsgIdErr = 1022,             //��Ϣid����
};

enum MsgStatus {
	UN_READ = 0,  //�Է�δ��
	SEND_FAILED = 1,  //����ʧ��
	READED = 2,  //�Է��Ѷ�
	UN_UPLOAD = 3 //δ�ϴ����
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

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define NAME_INFO  "nameinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"