#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include "const.h"
#include "MsgNode.h"
#include "message.grpc.pb.h"
#include "message.pb.h"
#include <grpcpp/grpcpp.h>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using message::NotifyChatImgReq;

class CServer;
class LogicSystem;

class CSession: public std::enable_shared_from_this<CSession>
{
public:
	CSession(boost::asio::io_context& io_context, CServer* server);
	~CSession();
	tcp::socket& GetSocket();
	std::string& GetSessionId();
	void SetUserId(int uid);
	int GetUserId();
	void Start();
	void Send(char* msg,  short max_length, short msgid);
	void Send(std::string msg, short msgid);
	void Close();
	std::shared_ptr<CSession> SharedSelf();
	void AsyncReadBody(int length);
	void AsyncReadHead(int total_len);
	//֪ͨ�û�����
	void NotifyOffline(int uid);
	//֪ͨ�û����նԷ�������ͼƬ��Ϣ
	void NotifyChatImgRecv(const ::message::NotifyChatImgReq* request);
	//֪ͨ�û����նԷ��������ļ���Ϣ
	void NotifyChatFileRecv(const ::message::NotifyChatFileReq* request);
	//�ж������Ƿ����
	bool IsHeartbeatExpired(std::time_t& now);
	//��������
	void UpdateHeartbeat();
	//�����쳣���ӣ���ص�¼����ʬ���ӣ�
	void DealExceptionSession();

private:
	void asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code& , std::size_t)> handler);
	void asyncReadLen(std::size_t  read_len, std::size_t total_len,
		std::function<void(const boost::system::error_code&, std::size_t)> handler);
	
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self);

	tcp::socket _socket;
	std::string _session_id;
	char _data[MAX_LENGTH];
	CServer* _server;
	bool _b_close;
	//����ʹ�ö������洢������Ϣ�����ã�1.������ٿ�����2.��֤��Ϣ����˳��3.���̰߳�ȫ
	std::queue<shared_ptr<SendNode> > _send_que;
	std::mutex _send_lock;
	//�յ�����Ϣ�ṹ
	std::shared_ptr<RecvNode> _recv_msg_node;
	bool _b_head_parse;
	//�յ���ͷ���ṹ
	std::shared_ptr<MsgNode> _recv_head_node;
	int _user_uid;
	//��¼�ϴν������ݵ�ʱ��
	std::atomic<time_t> _last_heartbeat;
	//session ��
	std::mutex _session_mtx;
};

class LogicNode {
	friend class LogicSystem;
public:
	LogicNode(shared_ptr<CSession>, shared_ptr<RecvNode>);
private:
	shared_ptr<CSession> _session;
	shared_ptr<RecvNode> _recvnode;
};
