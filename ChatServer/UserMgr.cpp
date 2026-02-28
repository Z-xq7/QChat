#include "UserMgr.h"
#include "CSession.h"
#include "RedisMgr.h"

UserMgr:: ~UserMgr() {
	_uid_to_session.clear();
}

std::shared_ptr<CSession> UserMgr::GetSession(int uid)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	auto iter = _uid_to_session.find(uid);
	if (iter == _uid_to_session.end()) {
		return nullptr;
	}

	return iter->second;
}

void UserMgr::SetUserSession(int uid, std::shared_ptr<CSession> session)
{
	std::lock_guard<std::mutex> lock(_session_mtx);
	_uid_to_session[uid] = session;
}

void UserMgr::RmvUserSession(int uid, std::string session_id)
{
	{
		std::lock_guard<std::mutex> lock(_session_mtx);
		auto iter = _uid_to_session.find(uid);
		if (iter == _uid_to_session.end()) {
			return;
		}

		auto session_id_ = iter->second->GetSessionId();
		//不相等说明是其他地方登录了
		if (session_id_ != session_id) {
			return;
		}
		_uid_to_session.erase(uid);
	}
}

//void UserMgr::RmvUserSession(int uid, std::string session_id)
//{
//	auto uid_str = std::to_string(uid);
//	//因为再次登录可能是在其他服务器登录，会造成本服务器删除key，其他服务器注册key的情况
//	{
//		std::lock_guard<std::mutex> lock(_session_mtx);
//		_uid_to_session.erase(uid);
//	}
//}

UserMgr::UserMgr()
{
}
