#include "CSession.h"
#include "CServer.h"
#include "Logger.h"
#include <sstream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "LogicSystem.h"


CSession::CSession(boost::asio::io_context& io_context, CServer* server) :
	_socket(io_context), _session_id(), _data(MAX_LENGTH), _server(server), _b_close(false), _b_head_parse(false), _user_uid(0) {
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	_session_id = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);
}
CSession::~CSession() {
	LOG_DEBUG("Session [" << _session_id << "] destruct");
}

tcp::socket& CSession::GetSocket() {
	return _socket;
}

std::string& CSession::GetSessionId() {
	return _session_id;
}

void CSession::SetUserId(int uid)
{
	_user_uid = uid;
}

int CSession::GetUserId()
{
	return _user_uid;
}

void CSession::Start() {
	LOG_INFO("Session [" << _session_id << "] started to read");
	AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::Send(std::string msg, short msgid)
{
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		LOG_WARN("Session [" << _session_id << "] send queue full, size: " << MAX_SENDQUE);
		return;
	}

	// SendNode 构造函数已经包含了头部（ID + 长度）的封装逻辑
	auto send_node = std::make_shared<SendNode>(msg.c_str(), msg.length(), msgid);
	_send_que.push(send_node);
	
	// 如果队列之前不为空，说明已经有 async_write 在执行，直接返回
	if (send_que_size > 0) {
		return;
	}

	auto self = shared_from_this();
	// 注意：发送整个缓冲区的数据，包括头部和体部
	boost::asio::async_write(_socket, boost::asio::buffer(send_node->_data.data(), send_node->total_len()),
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, self));
}

void CSession::Send(char* msg, short max_length, short msgid) {
	// 统一调用 string 版本，确保逻辑一致性和原子性
	this->Send(std::string(msg, max_length), msgid);
}

void CSession::Close() {
	_socket.close();
	_b_close = true;
}

std::shared_ptr<CSession>CSession::SharedSelf() {
	return shared_from_this();
}

void CSession::AsyncReadBody(int total_len)
{
	auto self = shared_from_this();
	asyncReadFull(total_len, [self, this, total_len](const boost::system::error_code& ec, std::size_t bytes_transfered) {
		try {
			if (ec) {
				LOG_ERROR("Handle read failed, error: " << ec.what());
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			if (bytes_transfered < static_cast<std::size_t>(total_len)) {
				LOG_ERROR("Read length not match, read [" << bytes_transfered << "], total [" << total_len << "]");
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			if (!_recv_msg_node || bytes_transfered > _recv_msg_node->total_len()) {
				LOG_ERROR("Body overflow: bytes_transfered=" << bytes_transfered
					<< ", buf_total_len=" << (_recv_msg_node ? _recv_msg_node->total_len() : 0));
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			memcpy(_recv_msg_node->_data.data(), _data.data(), bytes_transfered);
			_recv_msg_node->_cur_len += bytes_transfered;
			if (!_recv_msg_node->_data.empty()) {
				_recv_msg_node->_data[_recv_msg_node->total_len()] = '\0';
			}
			// ʹ�� std::hash ���ַ������й�ϣ
			std::hash<std::string> hash_fn;
			size_t hash_value = hash_fn(_session_id); // ���ɹ�ϣֵ
			int index = static_cast<int>(hash_value % LOGIC_WORKER_COUNT);
			//�˴�����ϢͶ�ݵ��߼������߳�
			LogicSystem::GetInstance()->PostMsgToQue(make_shared<LogicNode>(shared_from_this(), _recv_msg_node), index);
			//�ټ�������ͷ����ȡ�¼�
			AsyncReadHead(HEAD_TOTAL_LEN);
		}
		catch (std::exception& e) {
			LOG_ERROR("Exception: " << e.what());
		}
		});
}

void CSession::AsyncReadHead(int total_len)
{
	auto self = shared_from_this();
	asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {
		try {
			if (ec) {
				LOG_ERROR("Handle read failed, error: " << ec.what());
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			if (bytes_transfered < HEAD_TOTAL_LEN) {
				LOG_ERROR("Read length not match, read [" << bytes_transfered << "], total [" << HEAD_TOTAL_LEN << "]");
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			_recv_head_node->Clear();
			memcpy(_recv_head_node->_data.data(), _data.data(), bytes_transfered);

			//��ȡͷ��MSGID����
			short msg_id = 0;
			memcpy(&msg_id, _recv_head_node->_data.data(), HEAD_ID_LEN);
			//�����ֽ���ת��Ϊ�����ֽ���
			msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
			LOG_DEBUG("Received msg_id: " << msg_id);
			//id�Ƿ�
			if (msg_id > MAX_LENGTH) {
				LOG_ERROR("Invalid msg_id: " << msg_id);
				_server->ClearSession(_session_id);
				return;
			}
			int msg_len = 0;
			memcpy(&msg_len, _recv_head_node->_data.data() + HEAD_ID_LEN, HEAD_DATA_LEN);
			//�����ֽ���ת��Ϊ�����ֽ���
			msg_len = boost::asio::detail::socket_ops::network_to_host_long(msg_len);
			LOG_DEBUG("Received msg_len: " << msg_len);

			//���ȷǷ�
			if (msg_len > MAX_LENGTH) {
				LOG_ERROR("Invalid data length: " << msg_len);
				_server->ClearSession(_session_id);
				return;
			}

			_recv_msg_node = make_shared<RecvNode>(static_cast<std::size_t>(msg_len), msg_id);
			AsyncReadBody(msg_len);
		}
		catch (std::exception& e) {
			LOG_ERROR("Exception: " << e.what());
		}
		});
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self) {
	//�˴������쳣����
	try {
		if (!error) {
			std::lock_guard<std::mutex> lock(_send_lock);
			_send_que.pop();
			if (!_send_que.empty()) {
				auto& msgnode = _send_que.front();
				boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data.data(), msgnode->total_len()),
					std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_self));
			}
		}
		else {
			LOG_ERROR("Handle write failed, error: " << error.what());
			Close();
			_server->ClearSession(_session_id);
		}
	}
	catch (std::exception& e) {
		LOG_ERROR("Exception: " << e.what());
	}

}

//��ȡ��������
void CSession::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	::memset(_data.data(), 0, _data.size());
	asyncReadLen(0, maxLength, handler);
}

//��ȡָ���ֽ���
void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len,
	std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	auto self = shared_from_this();
	_socket.async_read_some(boost::asio::buffer(_data.data() + read_len, total_len - read_len),
		[read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t  bytesTransfered) {
			if (ec) {
				// ���ִ��󣬵��ûص�����
				handler(ec, read_len + bytesTransfered);
				return;
			}

			if (read_len + bytesTransfered >= total_len) {
				//���ȹ��˾͵��ûص�����
				handler(ec, read_len + bytesTransfered);
				return;
			}

			// û�д����ҳ��Ȳ�����������ȡ
			self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
		});
}

LogicNode::LogicNode(shared_ptr<CSession>  session,
	shared_ptr<RecvNode> recvnode) :_session(session), _recvnode(recvnode) {

}