#pragma once
#include "Singleton.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class CSession;
class UserMgr : public Singleton<UserMgr>
{
	friend class Singleton<UserMgr>;
public:
	~UserMgr();
	//根据uid获取session
	std::shared_ptr<CSession> GetSession(int uid);
	//根据session_id获取session
	void SetUserSession(int uid, std::shared_ptr<CSession> session);
	//根据uid和session_id删除session
	void RmvUserSession(int uid, std::string session_id);

private:
	UserMgr();
	std::mutex _session_mtx;
	std::unordered_map<int, std::shared_ptr<CSession>> _uid_to_session;
};