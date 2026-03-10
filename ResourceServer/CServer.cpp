#include "CServer.h"
#include "Logger.h"
#include "AsioIOServicePool.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"

using namespace std;

CServer::CServer(boost::asio::io_context& io_context, short port):_io_context(io_context), _port(port),
	_acceptor(io_context, tcp::endpoint(tcp::v4(),port)), _timer(_io_context, std::chrono::seconds(60))
{
	LOG_INFO("Server start success, listen on port: " << _port);
	StartAccept();
}

CServer::~CServer() {
	LOG_INFO("Server destruct, listen on port: " << _port);
}

void CServer::HandleAccept(shared_ptr<CSession> new_session, const boost::system::error_code& error){
	if (!error) {
		new_session->Start();
		lock_guard<mutex> lock(_mutex);
		_sessions.insert(make_pair(new_session->GetSessionId(), new_session));
	}
	else {
		LOG_ERROR("Session accept failed, error: " << error.what());
	}

	StartAccept();
}

void CServer::StartAccept() {
	auto &io_context = AsioIOServicePool::GetInstance()->GetIOService();
	shared_ptr<CSession> new_session = make_shared<CSession>(io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this, new_session, placeholders::_1));
}

//根据session 的id删除session，并且移除用户和session的关联
void CServer::ClearSession(std::string session_id) {
	
	std::lock_guard<mutex> lock(_mutex);
	if (_sessions.find(session_id) != _sessions.end()) {
		auto uid = _sessions[session_id]->GetUserId();

		//移除用户和session的关联
		UserMgr::GetInstance()->RmvUserSession(uid, session_id);
	}

	_sessions.erase(session_id);
}