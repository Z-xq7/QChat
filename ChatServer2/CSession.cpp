#include "CSession.h"
#include "CServer.h"
#include <iostream>
#include <sstream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "LogicSystem.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "Logger.h"
#include "MysqlMgr.h"

CSession::CSession(boost::asio::io_context& io_context, CServer* server):
	_socket(io_context), _server(server), _b_close(false),_b_head_parse(false), _user_uid(0){
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	_session_id = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);
	_last_heartbeat = std::time(nullptr);
}

CSession::~CSession() {
	LOG_DEBUG("Session destructed - session_id: " << _session_id);

	////********** 这里被优化了，先不再用 **********
	////减少服务器登录人数
	//auto& cfg = ConfigMgr::Inst();
	//auto self_name = cfg["SelfServer"]["Name"];
	//RedisMgr::GetInstance()->DecreaseCount(self_name);
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

void CSession::Start(){
	AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::Send(std::string msg, short msgid) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		LOG_WARN("Send queue full - session_id: " << _session_id << ", size: " << MAX_SENDQUE);
		return;
	}

	_send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
	if (send_que_size > 0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void CSession::Send(char* msg, short max_length, short msgid) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		LOG_WARN("Send queue full - session_id: " << _session_id << ", size: " << MAX_SENDQUE);
		return;
	}

	_send_que.push(make_shared<SendNode>(msg, max_length, msgid));
	if (send_que_size>0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len), 
		std::bind(&CSession::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void CSession::Close() {
	std::lock_guard<std::mutex> lock(_session_mtx);
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
				LOG_ERROR("Handle read body failed - session_id: " << _session_id << ", error: " << ec.what());
				Close();
				DealExceptionSession();
				return;
			}

			if (bytes_transfered < total_len) {
				LOG_WARN("Read length not match - session_id: " << _session_id 
					<< ", read: " << bytes_transfered << ", expected: " << total_len);
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			//判断连接是否有效
			if (!_server->CheckValid(_session_id)) {
				Close();
				return;
			}

			memcpy(_recv_msg_node->_data , _data , bytes_transfered);
			_recv_msg_node->_cur_len += bytes_transfered;
			_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
			LOG_DEBUG("Receive data - session_id: " << _session_id << ", data: " << _recv_msg_node->_data);

			//更新session心跳时间
			UpdateHeartbeat();
			//此处将消息投递到逻辑队列中
			LogicSystem::GetInstance()->PostMsgToQue(make_shared<LogicNode>(shared_from_this(), _recv_msg_node));
			//继续监听头部接收事件
			AsyncReadHead(HEAD_TOTAL_LEN);
		}
		catch (std::exception& e) {
			LOG_ERROR("AsyncReadBody exception - session_id: " << _session_id << ", error: " << e.what());
		}
		});
}

void CSession::AsyncReadHead(int total_len)
{
	auto self = shared_from_this();
	asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {
		try {
			if (ec) {
				LOG_ERROR("Handle read head failed - session_id: " << _session_id << ", error: " << ec.what());
				Close();
				DealExceptionSession();
				return;
			}

			if (bytes_transfered < HEAD_TOTAL_LEN) {
				LOG_WARN("Read length not match - session_id: " << _session_id 
					<< ", read: " << bytes_transfered << ", expected: " << HEAD_TOTAL_LEN);
				Close();
				_server->ClearSession(_session_id);
				return;
			}

			//判断连接是否有效
			if (!_server->CheckValid(_session_id)) {
				Close();
				return;
			}

			_recv_head_node->Clear();
			memcpy(_recv_head_node->_data, _data, bytes_transfered);

			//获取头部MSGID数据
			short msg_id = 0;
			memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
			//网络字节序转换为主机字节序
			msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
			LOG_DEBUG("Receive msg_id: " << msg_id << " - session_id: " << _session_id);
			//id非法
			if (msg_id > MAX_LENGTH) {
				LOG_WARN("Invalid msg_id: " << msg_id << " - session_id: " << _session_id);
				_server->ClearSession(_session_id);
				return;
			}
			short msg_len = 0;
			memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
			//网络字节序转换为主机字节序
			msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
			LOG_DEBUG("Receive msg_len: " << msg_len << " - session_id: " << _session_id);

			//id非法
			if (msg_len > MAX_LENGTH) {
				LOG_WARN("Invalid data length: " << msg_len << " - session_id: " << _session_id);
				_server->ClearSession(_session_id);
				return;
			}

			_recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);
			//继续监听消息体接收事件
			AsyncReadBody(msg_len);
			//更新session心跳时间
			UpdateHeartbeat();
		}
		catch (std::exception& e) {
			LOG_ERROR("AsyncReadHead exception - session_id: " << _session_id << ", error: " << e.what());
		}
		});
}

void CSession::HandleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> shared_self) {
	//此处捕获异常
	try {
		auto self = shared_from_this();
		if (!error) {
			std::lock_guard<std::mutex> lock(_send_lock);
			_send_que.pop();
			if (!_send_que.empty()) {
				auto& msgnode = _send_que.front();
				boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
					std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_self));
			}
		}
		else {
			LOG_ERROR("Handle write failed - session_id: " << _session_id << ", error: " << error.what());
			Close();
			DealExceptionSession();
		}
	}
	catch (std::exception& e) {
		LOG_ERROR("HandleWrite exception - error: " << e.what());
	}
	
}

//读取完整长度
void CSession::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler )
{
	::memset(_data, 0, MAX_LENGTH);
	asyncReadLen(0, maxLength, handler);
}

//读取指定字节数
void CSession::asyncReadLen(std::size_t read_len, std::size_t total_len, 
	std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	auto self = shared_from_this();
	_socket.async_read_some(boost::asio::buffer(_data + read_len, total_len-read_len),
		[read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t  bytesTransfered) {
			if (ec) {
				// 出现错误，调用回调函数
				handler(ec, read_len + bytesTransfered);
				return;
			}

			if (read_len + bytesTransfered >= total_len) {
				//长度够了就调用回调函数
				handler(ec, read_len + bytesTransfered);
				return;
			}

			// 没有错误，且长度不足则继续读取
			self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
	});
}

void CSession::NotifyOffline(int uid) {
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["uid"] = uid;

	std::string return_str = rtvalue.toStyledString();

	Send(return_str, ID_NOTIFY_OFF_LINE_REQ);
	return;
}

void CSession::NotifyChatImgRecv(const message::NotifyChatImgReq* request)
{
	Json::Value  rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	rtvalue["message_id"] = request->message_id();
	rtvalue["sender_id"] = request->from_uid();
	rtvalue["receiver_id"] = request->to_uid();
	rtvalue["img_name"] = request->file_name();
	rtvalue["total_size"] = std::to_string(request->total_size());
	rtvalue["thread_id"] = request->thread_id();

	std::string return_str = rtvalue.toStyledString();
	Send(return_str, ID_NOTIFY_IMG_CHAT_MSG_REQ);
	return;
}

bool CSession::IsHeartbeatExpired(std::time_t& now) {
	double diff_sec = std::difftime(now, _last_heartbeat);
	if (diff_sec > HEARTBEAT_EXPIRE_TIME) {
		LOG_WARN("Heartbeat expired - session_id: " << _session_id << ", diff_sec: " << diff_sec);
		return true;
	}

	return false;
}

void CSession::UpdateHeartbeat()
{
	time_t now = std::time(nullptr);
	_last_heartbeat = now;
}

void CSession::DealExceptionSession()
{
	auto self = shared_from_this();
	//加锁清除session
	auto uid_str = std::to_string(_user_uid);
	auto lock_key = LOCK_PREFIX + uid_str;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
	Defer defer([identifier, lock_key, self, this]() {
		_server->ClearSession(_session_id);
		RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
		});

	if (identifier.empty()) {
		return;
	}
	std::string redis_session_id = "";
	auto bsuccess = RedisMgr::GetInstance()->Get(USER_SESSION_PREFIX + uid_str, redis_session_id);
	if (!bsuccess) {
		return;
	}

	if (redis_session_id != _session_id) {
		//说明有客户在其他服务器异地登录了
		return;
	}

	RedisMgr::GetInstance()->Del(USER_SESSION_PREFIX + uid_str);
	//清除用户登录信息
	RedisMgr::GetInstance()->Del(USERIPPREFIX + uid_str);
	//清除用户token信息
	RedisMgr::GetInstance()->Del(USERTOKENPREFIX + uid_str);
}

LogicNode::LogicNode(shared_ptr<CSession>  session,
	shared_ptr<RecvNode> recvnode) :_session(session), _recvnode(recvnode) {

}

