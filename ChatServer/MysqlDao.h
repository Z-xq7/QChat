#pragma once
#include <chrono>
#include "const.h"
#include <thread>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
#include "data.h"
#include <memory>
#include <queue>
#include <mutex>
#include "message.pb.h"
using message::AddFriendMsg;
using message::TextChatData;

class SqlConnection {
public:
	SqlConnection(sql::Connection* con, int64_t lasttime) :_con(con), _last_oper_time(lasttime) {}
	std::unique_ptr<sql::Connection> _con;
	int64_t _last_oper_time;
};

class MySqlPool {
public:
	MySqlPool(const std::string& url, const std::string& user, const std::string& pass, const std::string& schema, int poolSize)
		: url_(url), user_(user), pass_(pass), schema_(schema), poolSize_(poolSize), b_stop_(false), _fail_count(0) {
		try {
			for (int i = 0; i < poolSize_; ++i) {
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				auto* con = driver->connect(url_, user_, pass_);
				con->setSchema(schema_);
				// ��ȡ��ǰʱ���
				auto currentTime = std::chrono::system_clock::now().time_since_epoch();
				// ��ʱ���ת��Ϊ��
				long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
				pool_.push(std::make_unique<SqlConnection>(con, timestamp));
				std::cout << "--- Mysql Connection Init Success ---" << std::endl;
			}

			_check_thread = std::thread([this]() {
				int count = 0;
				while (!b_stop_) {
					if (count >= 60) {
						count = 0;
						checkConnectionPro();
					}
					std::this_thread::sleep_for(std::chrono::seconds(1));
					count++;
				}
				});

			_check_thread.detach();
		}
		catch (sql::SQLException& e) {
			// �����쳣
			std::cout << "[*** mysql pool init failed, error is " << e.what() << " ***]" << std::endl;
		}
	}

	void checkConnectionPro() {
		//�ȶ�ȡ��Ŀ�괦������
		size_t targetCount;
		{
			std::lock_guard<std::mutex> guard(mutex_);
			targetCount = pool_.size();
		}

		//��ǰ�Ѿ�����������
		size_t processed = 0;

		//ʱ���
		auto now = std::chrono::system_clock::now().time_since_epoch();
		long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

		while (processed < targetCount) {
			std::unique_ptr<SqlConnection> con;
			{
				std::lock_guard<std::mutex> guard(mutex_);
				if (pool_.empty()) {
					break;
				}
				con = std::move(pool_.front());
				pool_.pop();
			}

			bool healthy = true;
			//�����������/�����߼�
			if (timestamp - con->_last_oper_time >= 5) {
				try {
					std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
					stmt->executeQuery("SELECT 1");
					con->_last_oper_time = timestamp;
				}
				catch (sql::SQLException& e) {
					std::cout << "[*** Error keeping connection alive: " << e.what() << " ***]" << std::endl;
					healthy = false;
					_fail_count++;
				}
			}

			if (healthy)
			{
				std::lock_guard<std::mutex> guard(mutex_);
				pool_.push(std::move(con));
				cond_.notify_one();
			}
			++processed;
		}

		std::cout << "*** Mysql: Connection check completed, " << _fail_count << " connections failed ***" << std::endl;

		//��������mysql
		while (_fail_count > 0) {
			auto b_res = reconnect(timestamp);
			if (b_res) {
				_fail_count--;
			}
			else {
				break;
			}
		}
	}

	bool reconnect(long long timestamp) {
		try {
			sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
			auto* con = driver->connect(url_, user_, pass_);
			con->setSchema(schema_);

			auto newCon = std::make_unique<SqlConnection>(con, timestamp);
			{
				std::lock_guard<std::mutex> guard(mutex_);
				pool_.push(std::move(newCon));
			}
			std::cout << "[--- Mysql Connection Reconnect Success! ---]" << std::endl;
			return true;
		}
		catch (sql::SQLException& e) {
			std::cout << "[*** Mysql Reconnect Failed, error is " << e.what() << " ***]" << std::endl;
			return false;
		}
	}

	void checkConnection() {
		std::lock_guard<std::mutex> guard(mutex_);
		int poolsize = pool_.size();
		// ��ȡ��ǰʱ���
		auto currentTime = std::chrono::system_clock::now().time_since_epoch();
		// ��ʱ���ת��Ϊ��
		long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
		for (int i = 0; i < poolsize; i++) {
			auto con = std::move(pool_.front());
			pool_.pop();
			Defer defer([this, &con]() {
				pool_.push(std::move(con));
				});

			if (timestamp - con->_last_oper_time < 5) {
				continue;
			}

			try {
				std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
				stmt->executeQuery("SELECT 1");
				con->_last_oper_time = timestamp;
				//std::cout << "execute timer alive query , cur is " << timestamp << std::endl;
			}
			catch (sql::SQLException& e) {
				std::cout << "Error keeping connection alive: " << e.what() << std::endl;
				// ���´������Ӳ��滻�ɵ�����
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				auto* newcon = driver->connect(url_, user_, pass_);
				newcon->setSchema(schema_);
				con->_con.reset(newcon);
				con->_last_oper_time = timestamp;
			}
		}
	}

	std::unique_ptr<SqlConnection> getConnection() {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this] {
			if (b_stop_) {
				return true;
			}
			return !pool_.empty(); });
		if (b_stop_) {
			return nullptr;
		}
		std::unique_ptr<SqlConnection> con(std::move(pool_.front()));
		pool_.pop();
		return con;
	}

	void returnConnection(std::unique_ptr<SqlConnection> con) {
		if (con && con->_con) {
			try {
				// 确保归还的连接 autocommit=true，避免下一个复用者受残留事务影响
				if (!con->_con->getAutoCommit()) {
					con->_con->rollback();
					con->_con->setAutoCommit(true);
				}
			}
			catch (sql::SQLException&) {
				// 连接已损坏，丢弃不归还
				return;
			}
		}
		std::unique_lock<std::mutex> lock(mutex_);
		if (b_stop_) {
			return;
		}
		pool_.push(std::move(con));
		cond_.notify_one();
	}

	void Close() {
		b_stop_ = true;
		cond_.notify_all();
	}

	~MySqlPool() {
		std::unique_lock<std::mutex> lock(mutex_);
		while (!pool_.empty()) {
			pool_.pop();
		}
	}

private:
	std::string url_;
	std::string user_;
	std::string pass_;
	std::string schema_;
	int poolSize_;
	std::queue<std::unique_ptr<SqlConnection>> pool_;
	std::mutex mutex_;
	std::condition_variable cond_;
	std::atomic<bool> b_stop_;
	std::thread _check_thread;
	std::atomic<int> _fail_count;
};

class MysqlDao
{
public:
	MysqlDao();
	~MysqlDao();
	int RegUser(const std::string& name, const std::string& email, const std::string& pwd);
	bool CheckEmail(const std::string& name, const std::string& email);
	bool UpdatePwd(const std::string& name, const std::string& newpwd);
	bool CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo);
	bool AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& bakname);
	//(�Ż����ѷ���������״̬ͳһ��AddFriend����ɼ���)
	bool AuthFriendApply(const int& from, const int& to);
	bool AddFriend(const int& from, const int& to, std::string back_name, std::vector<std::shared_ptr<AddFriendMsg>>& msg_list);
	std::shared_ptr<UserInfo> GetUser(int uid);
	std::shared_ptr<UserInfo> GetUser(std::string name);
	bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int offset, int limit);
	bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info);
	bool GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
		std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& loadMore, int& nextLastId);
	bool CreatePrivateChat(int user1_id, int user2_id, int& thread_id);
	std::shared_ptr<PageResult> LoadChatMsg(int threadId, int lastId, int pageSize);
	// 反向分页：加载 message_id < cursor 的更早消息（ORDER BY message_id DESC）
	std::shared_ptr<PageResult> LoadChatHistory(int threadId, int cursor, int pageSize);
	bool AddChatMsg(std::vector<std::shared_ptr<ChatMessage>>& chat_datas);
	bool AddChatMsg(std::shared_ptr<ChatMessage>& chat_data);
	std::shared_ptr<ChatMessage> GetChatMsg(int message_id);

	// Ⱥ����ز���
	// ����Ⱥ�ģ����� thread_id
	bool CreateGroupChat(int creator_uid, const std::string& group_name,
		const std::vector<int>& member_uids, int& thread_id);
	// ��ȡȺ��Ա�б�// 获取群聊成员列表
	bool GetGroupMembers(int thread_id, std::vector<std::shared_ptr<GroupMemberInfo>>& members);
	// 获取用户加入的所有群聊ID
	bool GetUserGroupChats(int user_id, std::vector<int>& thread_ids);
	// 获取单个群聊基本信息
	bool GetGroupInfo(int thread_id, GroupInfo& group_info);
	// 更新群公告
	bool UpdateGroupNotice(int thread_id, const std::string& notice);
	// 更新用户在群内的个性化设置 (昵称、免打扰、置顶等)
	bool UpdateGroupMemberSetting(int thread_id, int user_id, const std::string& group_nick, int role, int is_disturb, int is_top);

	// 获取用户所有私聊会话的未读消息数
	bool GetUnreadCounts(int user_id, std::vector<std::pair<int, int>>& unread_counts);
	// 标记指定会话的消息为已读
	bool MarkMsgRead(int thread_id, int reader_uid, std::string update_time);
	// 根据 thread_id 查询私聊对方 uid（返回 true 表示私聊，peer_uid 为对方 uid）
	bool GetPrivateChatPeer(int thread_id, int self_uid, int& peer_uid);

private:
	std::unique_ptr<MySqlPool> pool_;
};


