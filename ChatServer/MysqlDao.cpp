#include "MysqlDao.h"
#include "ConfigMgr.h"
#include "Logger.h"

MysqlDao::MysqlDao()
{
	auto& cfg = ConfigMgr::Inst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["Port"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];
	pool_.reset(new MySqlPool(host + ":" + port, user, pwd, schema, 5));
}

MysqlDao::~MysqlDao() {
	pool_->Close();
}

int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}
		// ׼�����ô洢����
		std::unique_ptr < sql::PreparedStatement > stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
		// �����������
		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);

		// ����PreparedStatement��ֱ��֧��ע�����������������Ҫʹ�ûỰ������������������ȡ���������ֵ

		  // ִ�д洢����
		stmt->execute();
		// ����洢���������˻Ự��������������ʽ��ȡ���������ֵ�������������ִ��SELECT��ѯ����ȡ����
	   // ���磬����洢����������һ���Ự����@result���洢������������������ȡ��
		std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
		if (res->next()) {
			int result = res->getInt("result");
			LOG_INFO("RegUser - name: " << name << ", result: " << result);
			pool_->returnConnection(std::move(con));
			return result;
		}
		pool_->returnConnection(std::move(con));
		return -1;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		LOG_ERROR("RegUser SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return -1;
	}
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// ׼����ѯ���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));

		// �󶨲���
		pstmt->setString(1, name);

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		// ���������
		while (res->next()) {
			LOG_DEBUG("CheckEmail - name: " << name << ", email: " << res->getString("email"));
			if (email != res->getString("email")) {
				pool_->returnConnection(std::move(con));
				return false;
			}
			pool_->returnConnection(std::move(con));
			return true;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		LOG_ERROR("CheckEmail SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// ׼����ѯ���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

		// �󶨲���
		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);

		// ִ�и���
		int updateCount = pstmt->executeUpdate();

		LOG_INFO("UpdatePwd - name: " << name << ", updated_rows: " << updateCount);
		pool_->returnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		LOG_ERROR("UpdatePwd SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::CheckPwd(const std::string& name, const std::string& pwd, UserInfo& userInfo) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name); // ��username�滻Ϊ��Ҫ��ѯ���û���

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::string origin_pwd = "";
		// ���������
		while (res->next()) {
			origin_pwd = res->getString("pwd");
			// �����ѯ��������
			LOG_DEBUG("CheckPwd - name: " << name);
			break;
		}

		if (pwd != origin_pwd) {
			return false;
		}
		userInfo.name = name;
		userInfo.email = res->getString("email");
		userInfo.uid = res->getInt("uid");
		userInfo.pwd = origin_pwd;
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("CheckPwd SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::AddFriendApply(const int& from, const int& to, const std::string& desc, const std::string& back_name)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"INSERT INTO friend_apply (from_uid, to_uid, descs, back_name) values (?,?,?,?) "
			"ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid, descs = ?, back_name = ?"));
		pstmt->setInt(1, from); // from id
		pstmt->setInt(2, to);
		pstmt->setString(3, desc);
		pstmt->setString(4, back_name);
		pstmt->setString(5, desc);
		pstmt->setString(6, back_name);
		// ִ�и���
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}
		LOG_DEBUG("AddFriendApply - from: " << from << ", to: " << to);
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("AddFriendApply SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
	return true;
}

//���ѷ�����
bool MysqlDao::AuthFriendApply(const int& from, const int& to) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE friend_apply SET status = 1 "
			"WHERE from_uid = ? AND to_uid = ?"));
		//������������ʱfrom����֤ʱto
		pstmt->setInt(1, to); // from id
		pstmt->setInt(2, from);
		// ִ�и���
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}
		LOG_DEBUG("AuthFriendApply - from: " << from << ", to: " << to);
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("AuthFriendApply SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}

	return true;
}

//���Ż�����ɹ���Ϊ�����Ӻ��Ѳ����������̣߳�
bool MysqlDao::AddFriend(const int& from, const int& to, std::string back_name,
	std::vector<std::shared_ptr<AddFriendMsg>>& chat_datas) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	try {
		//��ʼ����
		con->_con->setAutoCommit(false);
		std::string reverse_back;
		std::string apply_desc;
		{
			// 1. ��������ȡ
			std::unique_ptr<sql::PreparedStatement> selStmt(con->_con->prepareStatement(
				"SELECT back_name, descs "
				"FROM friend_apply "
				"WHERE from_uid = ? AND to_uid = ? "
				"FOR UPDATE"
			));
			selStmt->setInt(1, to);
			selStmt->setInt(2, from);
			std::unique_ptr<sql::ResultSet> rsSel(selStmt->executeQuery());
			if (rsSel->next()) {
				reverse_back = rsSel->getString("back_name");
				apply_desc = rsSel->getString("descs");
			}
			else {
				// û�ж�Ӧ�������¼��ֱ�� rollback ������ʧ��
				con->_con->rollback();
				return false;
			}
		}
		{
			// 2. ִ�������ĸ���
			std::unique_ptr<sql::PreparedStatement> updStmt(con->_con->prepareStatement(
				"UPDATE friend_apply "
				"SET status = 1 "
				"WHERE from_uid = ? AND to_uid = ?"
			));
			updStmt->setInt(1, to);
			updStmt->setInt(2, from);
			if (updStmt->executeUpdate() != 1) {
				// �����������ԣ��ع�
				con->_con->rollback();
				return false;
			}
		}
		{
			// 3. ������ѹ�ϵ - �ؼ��Ľ������չ̶�˳������������
			// ȷ������˳��ʼ�հ��� uid ��С˳��
			int smaller_uid = std::min(from, to);
			int larger_uid = std::max(from, to);

			// ��һ�β��룺��С�� uid ��Ϊ self_id
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
				"INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?)"
			));

			if (from == smaller_uid) {
				pstmt->setInt(1, from);
				pstmt->setInt(2, to);
				pstmt->setString(3, back_name);
			}
			else {
				pstmt->setInt(1, to);
				pstmt->setInt(2, from);
				pstmt->setString(3, reverse_back);
			}

			// ִ�и���
			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				return false;
			}

			// �ڶ��β��룺�ϴ�� uid ��Ϊ self_id
			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement(
				"INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?)"
			));

			if (from == larger_uid) {
				pstmt2->setInt(1, from);
				pstmt2->setInt(2, to);
				pstmt2->setString(3, back_name);
			}
			else {
				pstmt2->setInt(1, to);
				pstmt2->setInt(2, from);
				pstmt2->setString(3, reverse_back);
			}

			// ִ�и���
			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0) {
				con->_con->rollback();
				return false;
			}
		}

		// 4. ���� chat_thread
		long long threadId = 0;
		{
			std::unique_ptr<sql::PreparedStatement> threadStmt(con->_con->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW())"
			));
			threadStmt->executeUpdate();
			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				threadId = rs->getInt64(1);
			}
			else {
				con->_con->rollback();
				return false;
			}
		}

		// 5. ���� private_chat
		{
			std::unique_ptr<sql::PreparedStatement> pcStmt(con->_con->prepareStatement(
				"INSERT INTO private_chat(thread_id, user1_id, user2_id) VALUES (?, ?, ?)"
			));
			pcStmt->setInt64(1, threadId);
			pcStmt->setInt(2, from);
			pcStmt->setInt(3, to);
			if (pcStmt->executeUpdate() < 0) {
				con->_con->rollback();
				return false;
			}
		}

		// 6. �����ʼ��Ϣ������������
		if (!apply_desc.empty())
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status, msg_type) "
				"VALUES (?, ?, ?, ?, NOW(), NOW(), ?, ?)"
			));
			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, to);
			msgStmt->setInt(3, from);
			msgStmt->setString(4, apply_desc);
			//������ֱ������Ϊ�Ѷ����������Ըĳ�δ��
			msgStmt->setInt(5, 2);
			msgStmt->setInt(6, 0);
			if (msgStmt->executeUpdate() < 0) {
				con->_con->rollback();
				return false;
			}

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(to);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent(apply_desc);
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				LOG_DEBUG("AddFriend - insert first message, thread_id: " << threadId);
				chat_datas.push_back(tx_data);
			}
			else {
				con->_con->rollback();
				return false;
			}
		}

		// 7. �����Ϊ���ѵ���Ϣ
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status, msg_type) "
				"VALUES (?, ?, ?, ?, NOW(), NOW(), ?, ?)"
			));
			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, from);
			msgStmt->setInt(3, to);
			msgStmt->setString(4, "We are friends now!");
			//������ֱ������Ϊ�Ѷ����������Ըĳ�δ��
			msgStmt->setInt(5, 2);
			msgStmt->setInt(6, 0);
			if (msgStmt->executeUpdate() < 0) {
				con->_con->rollback();
				return false;
			}

			std::unique_ptr<sql::Statement> stmt(con->_con->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);

			if (rs->next()) {
				auto messageId = rs->getInt64(1);
				auto tx_data = std::make_shared<AddFriendMsg>();
				tx_data->set_sender_id(from);
				tx_data->set_msg_id(messageId);
				tx_data->set_msgcontent("We are friends now!");
				tx_data->set_thread_id(threadId);
				tx_data->set_unique_id("");
				chat_datas.push_back(tx_data);
			}
			else {
				con->_con->rollback();
				return false;
			}
		}
		// �ύ����
		con->_con->commit();
		LOG_INFO("AddFriend success - from: " << from << ", to: " << to << ", thread_id: " << threadId);
		return true;
	}
	catch (sql::SQLException& e) {
		// ����������󣬻ع�����
		if (con) {
			con->_con->rollback();
		}
		LOG_ERROR("AddFriend SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());

		// �������������1213�������Կ�������
		if (e.getErrorCode() == 1213) {
			LOG_ERROR("Deadlock detected, consider retry");
		}

		return false;
	}
	return true;
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid); // ��uid�滻Ϊ��Ҫ��ѯ��uid

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// ���������
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->icon = res->getString("icon");
			user_ptr->uid = uid;
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetUser SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return nullptr;
	}
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(std::string name)
{
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name); // ��uid�滻Ϊ��Ҫ��ѯ��uid

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// ���������
		while (res->next()) {
			user_ptr.reset(new UserInfo);
			user_ptr->pwd = res->getString("pwd");
			user_ptr->email = res->getString("email");
			user_ptr->name = res->getString("name");
			user_ptr->nick = res->getString("nick");
			user_ptr->desc = res->getString("desc");
			user_ptr->sex = res->getInt("sex");
			user_ptr->uid = res->getInt("uid");
			user_ptr->icon = res->getString("icon");
			break;
		}
		return user_ptr;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetUser SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return nullptr;
	}
}

bool MysqlDao::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>>& applyList, int begin, int limit) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���, ������ʼid���������������б�
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select apply.from_uid, apply.status, user.name, "
			"user.nick, user.sex,user.icon from friend_apply as apply join user on apply.from_uid = user.uid where apply.to_uid = ? "
			"and apply.id > ? order by apply.id ASC LIMIT ? "));

		pstmt->setInt(1, touid); // ��uid�滻Ϊ��Ҫ��ѯ��uid
		pstmt->setInt(2, begin); // ��ʼid
		pstmt->setInt(3, limit); //ƫ����
		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// ���������
		while (res->next()) {
			auto name = res->getString("name");
			auto uid = res->getInt("from_uid");
			auto status = res->getInt("status");
			auto nick = res->getString("nick");
			auto sex = res->getInt("sex");
			auto icon = res->getString("icon");
			auto apply_ptr = std::make_shared<ApplyInfo>(uid, name, "", icon, nick, sex, status);
			applyList.push_back(apply_ptr);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetApplyList SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo> >& user_info_list) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// ׼��SQL���, ������ʼid���������������б�
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select * from friend where self_id = ? "));

		pstmt->setInt(1, self_id); // ��uid�滻Ϊ��Ҫ��ѯ��uid

		// ִ�в�ѯ
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// ���������
		while (res->next()) {
			auto friend_id = res->getInt("friend_id");
			auto back = res->getString("back");
			//��һ�β�ѯfriend_id��Ӧ����Ϣ
			auto user_info = GetUser(friend_id);
			if (user_info == nullptr) {
				continue;
			}

			user_info->back = user_info->name;
			user_info_list.push_back(user_info);
		}
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetFriendList SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
	return true;
}

bool MysqlDao::GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& loadMore, int& nextLastId)
{
	// ��ʼ״̬
	loadMore = false;
	nextLastId = lastId;
	threads.clear();

	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;
	try {
		// ׼����ҳ��ѯ��CTE + UNION ALL + ORDER + LIMIT N+1
		// ����Ⱥ�ģ�ͨ�� JOIN group_chat ����ȡȺ������
		// �ͨ�� LEFT JOIN chat_message ��ȡÿ�����е����Ϣʱ��
		std::string sql =
			"WITH all_threads AS ( "
			"  SELECT thread_id, 'private' AS type, user1_id, user2_id, '' AS group_name "
			"    FROM private_chat "
			"   WHERE (user1_id = ? OR user2_id = ?) "
			"     AND thread_id > ? "
			"  UNION ALL "
			"  SELECT gcm.thread_id, 'group' AS type, 0 AS user1_id, 0 AS user2_id, gc.name AS group_name "
			"    FROM group_chat_member gcm "
			"    JOIN group_chat gc ON gcm.thread_id = gc.thread_id "
			"   WHERE gcm.user_id = ? "
			"     AND gcm.thread_id > ? "
			") "
			"SELECT t.thread_id, t.type, t.user1_id, t.user2_id, t.group_name, "
			"       COALESCE(lm.updated_at, t.thread_id) AS last_msg_time "
			"  FROM all_threads t "
			"  LEFT JOIN ("
			"    SELECT thread_id, MAX(updated_at) AS updated_at "
			"    FROM chat_message "
			"    GROUP BY thread_id"
			"  ) lm ON t.thread_id = lm.thread_id "
			" ORDER BY COALESCE(lm.updated_at, 0) DESC, t.thread_id DESC "
			" LIMIT ?;";
		std::unique_ptr<sql::PreparedStatement> pstmt(
			conn->prepareStatement(sql));
		// �󶨲�����? ��Ӧ (userId, userId, lastId, userId, lastId, pageSize+1)
		int idx = 1;
		pstmt->setInt64(idx++, userId);              // private.user1_id
		pstmt->setInt64(idx++, userId);              // private.user2_id
		pstmt->setInt64(idx++, lastId);              // private.thread_id > lastId
		pstmt->setInt64(idx++, userId);              // group.user_id
		pstmt->setInt64(idx++, lastId);              // group.thread_id > lastId
		pstmt->setInt(idx++, pageSize + 1);          // LIMIT pageSize+1
		// ִ��
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// �Ȱ������ж�����ʱ����
		std::vector<std::shared_ptr<ChatThreadInfo>> tmp;
		while (res->next()) {
			auto cti = std::make_shared<ChatThreadInfo>();
			cti->_thread_id = res->getInt64("thread_id");
			cti->_type = res->getString("type");
			cti->_user1_id = res->getInt64("user1_id");
			cti->_user2_id = res->getInt64("user2_id");
			cti->_group_name = res->getString("group_name");
			// ��ȡ������Ϣʱ�䣨ת��Ϊ�ַ���
			cti->_last_msg_time = res->getString("last_msg_time").asStdString();
			tmp.push_back(cti);
		}
		// �ж��Ƿ��ȡ��һ��
		if ((int)tmp.size() > pageSize) {
			loadMore = true;
			tmp.pop_back();  // ������ pageSize+1 ��
		}
		// ����������ݣ����� nextLastId Ϊ���һ���� thread_id
		if (!tmp.empty()) {
			nextLastId = tmp.back()->_thread_id;
		}
		// �����������
		threads = std::move(tmp);
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetUserThreads SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
	return true;
}

bool MysqlDao::CreatePrivateChat(int user1_id, int user2_id, int& thread_id)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;
	int uid1 = std::min(user1_id, user2_id);
	int uid2 = std::max(user1_id, user2_id);
	try {
		// ��������
		conn->setAutoCommit(false);
		// 1. �ȳ��Բ�ѯ�Ѵ��ڵļ�¼(����)
		std::string check_sql =
			"SELECT thread_id FROM private_chat "
			"WHERE user1_id = ? AND user2_id = ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(check_sql));
		pstmt->setInt64(1, uid1);
		pstmt->setInt64(2, uid2);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		if (res->next()) {
			// ����Ѵ��ڣ����ظ� thread_id
			thread_id = res->getInt("thread_id");
			conn->commit();  // �ύ����
			return true;
		}

		// 2. ���δ�ҵ��������µ� chat_thread �� private_chat ��¼
		// �� chat_thread �������¼�¼
		std::string insert_chat_thread_sql =
			"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());";

		std::unique_ptr<sql::PreparedStatement> pstmt_insert_thread(conn->prepareStatement(insert_chat_thread_sql));
		pstmt_insert_thread->executeUpdate();

		// ��ȡ�²���� thread_id
		std::string get_last_insert_id_sql = "SELECT LAST_INSERT_ID();";
		std::unique_ptr<sql::PreparedStatement> pstmt_last_insert_id(conn->prepareStatement(get_last_insert_id_sql));
		std::unique_ptr<sql::ResultSet> res_last_id(pstmt_last_insert_id->executeQuery());
		res_last_id->next();
		thread_id = res_last_id->getInt(1);

		// 3. �� private_chat �������¼�¼
		std::string insert_private_chat_sql =
			"INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
			"VALUES (?, ?, ?, NOW());";


		std::unique_ptr<sql::PreparedStatement> pstmt_insert_private(conn->prepareStatement(insert_private_chat_sql));
		pstmt_insert_private->setInt64(1, thread_id);
		pstmt_insert_private->setInt64(2, uid1);
		pstmt_insert_private->setInt64(3, uid2);
		pstmt_insert_private->executeUpdate();

		// �ύ����
		conn->commit();
		LOG_INFO("CreatePrivateChat success - thread_id: " << thread_id);
		return true;
	}
	catch (sql::SQLException& e) {
		conn->rollback();

		// ����Ƿ���Ψһ����ͻ (MySQL error code 1062)
		if (e.getErrorCode() == 1062) {
			// ���²�ѯ�Ѵ��ڵļ�¼
			try {
				conn->setAutoCommit(true);
				std::string retry_sql =
					"SELECT thread_id FROM private_chat "
					"WHERE user1_id = ? AND user2_id = ?;";
				std::unique_ptr<sql::PreparedStatement> pstmt_retry(
					conn->prepareStatement(retry_sql));
				pstmt_retry->setInt64(1, uid1);
				pstmt_retry->setInt64(2, uid2);
				std::unique_ptr<sql::ResultSet> res_retry(pstmt_retry->executeQuery());

				if (res_retry->next()) {
					thread_id = res_retry->getInt("thread_id");
					return true;
				}
			}
			catch (...) {
				return false;
			}
		}

		LOG_ERROR("CreatePrivateChat SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode());
		return false;
	}
	return false;
}

std::shared_ptr<PageResult> MysqlDao::LoadChatMsg(int thread_id, int last_message_id, int page_size)
{
	auto con = pool_->getConnection();
	if (!con) {
		return nullptr;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	auto& conn = con->_con;
	try {
		auto page_res = std::make_shared<PageResult>();
		page_res->load_more = false;

		// SQL����ȡһ���������ж��Ƿ��и���
		const std::string sql = R"(
        SELECT message_id, thread_id, sender_id, recv_id, content,
               created_at, updated_at, status, msg_type
        FROM chat_message
        WHERE thread_id = ?
          AND message_id > ?
        ORDER BY message_id ASC
        LIMIT ? 
		)";
		uint32_t fetch_limit = page_size + 1;
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(sql)
		);
		pstmt->setInt(1, thread_id);
		pstmt->setInt(2, last_message_id);
		pstmt->setInt(3, fetch_limit);
		auto rs = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
		// ��ȡ fetch_limit ����¼
		while (rs->next()) {
			ChatMessage msg;
			msg.message_id = rs->getUInt64("message_id");
			msg.thread_id = rs->getUInt64("thread_id");
			msg.sender_id = rs->getUInt64("sender_id");
			msg.recv_id = rs->getUInt64("recv_id");
			msg.content = rs->getString("content");
			msg.chat_time = rs->getString("created_at");
			msg.status = rs->getInt("status");
			msg.msg_type = rs->getInt("msg_type");
			page_res->messages.push_back(std::move(msg));
		}
		if (page_res->messages.empty()) {
			// 没有更多消息，直接返回
			return page_res;
		}
		if (page_res->messages.size() > page_size) {
			page_res->messages.pop_back();
			page_res->load_more = true;
		}
		page_res->next_cursor = page_res->messages.back().message_id;

		return page_res;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("LoadChatMsg SQLException - error: " << e.what());
		conn->rollback();
		return nullptr;
	}
	return nullptr;
}

bool MysqlDao::AddChatMsg(std::vector<std::shared_ptr<ChatMessage>>& chat_datas)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		//�ر��Զ��ύ�����ֶ���������
		conn->setAutoCommit(false);
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"INSERT INTO chat_message "
				"(thread_id, sender_id, recv_id, content, created_at, updated_at, status, msg_type) "
				"VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
			)
		);

		for (auto& msg : chat_datas) {
			// ��ͨ�ֶ�
			pstmt->setUInt64(1, msg->thread_id);
			pstmt->setUInt64(2, msg->sender_id);
			pstmt->setUInt64(3, msg->recv_id);
			pstmt->setString(4, msg->content);
			pstmt->setString(5, msg->chat_time);  // created_at
			pstmt->setString(6, msg->chat_time);  // updated_at
			pstmt->setInt(7, msg->status);
			pstmt->setInt(8, msg->msg_type);

			pstmt->executeUpdate();

			// 2. ȡ LAST_INSERT_ID()
			std::unique_ptr<sql::Statement> keyStmt(
				conn->createStatement()
			);
			std::unique_ptr<sql::ResultSet> rs(
				keyStmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				msg->message_id = rs->getUInt64(1);
			}
			else {
				continue;
			}
		}

		conn->commit();
		LOG_DEBUG("AddChatMsg batch success - count: " << chat_datas.size());
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("AddChatMsg batch SQLException - error: " << e.what());
		conn->rollback();
		return false;
	}
	return true;
}

bool MysqlDao::AddChatMsg(std::shared_ptr<ChatMessage>& chat_data)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}
	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});
	auto& conn = con->_con;

	try {
		//�ر��Զ��ύ�����ֶ���������
		conn->setAutoCommit(false);
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"INSERT INTO chat_message "
				"(thread_id, sender_id, recv_id, content, created_at, updated_at, status, msg_type) "
				"VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
			)
		);

		// �󶨲���
		pstmt->setUInt64(1, chat_data->thread_id);
		pstmt->setUInt64(2, chat_data->sender_id);
		pstmt->setUInt64(3, chat_data->recv_id);
		pstmt->setString(4, chat_data->content);
		pstmt->setString(5, chat_data->chat_time);  // created_at
		pstmt->setString(6, chat_data->chat_time);  // updated_at
		pstmt->setInt(7, chat_data->status);
		pstmt->setInt(8, chat_data->msg_type);

		pstmt->executeUpdate();

		// ��ȡ��������
		std::unique_ptr<sql::Statement> keyStmt(
			conn->createStatement()
		);
		std::unique_ptr<sql::ResultSet> rs(
			keyStmt->executeQuery("SELECT LAST_INSERT_ID()")
		);
		if (rs->next()) {
			chat_data->message_id = rs->getUInt64(1);
		}

		conn->commit();
		LOG_DEBUG("AddChatMsg single success - message_id: " << chat_data->message_id);
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("AddChatMsg single SQLException - error: " << e.what());
		conn->rollback();
		return false;
	}
}

std::shared_ptr<ChatMessage> MysqlDao::GetChatMsg(int message_id)
{
	auto con = pool_->getConnection();
	if (!con) {
		return nullptr;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"SELECT message_id, thread_id, sender_id, recv_id, "
				"content, created_at, updated_at, status, msg_type "
				"FROM chat_message WHERE message_id = ?"
			)
		);

		pstmt->setUInt64(1, message_id);
		auto rs = std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());

		if (rs->next()) {
			auto msg = std::make_shared<ChatMessage>();
			msg->message_id = rs->getUInt64("message_id");
			msg->thread_id = rs->getUInt64("thread_id");
			msg->sender_id = rs->getUInt64("sender_id");
			msg->recv_id = rs->getUInt64("recv_id");
			msg->content = rs->getString("content");
			msg->chat_time = rs->getString("created_at");
			msg->status = rs->getInt("status");
			msg->msg_type = rs->getInt("msg_type");

			return msg;
		}

		return nullptr;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetChatMessageById SQLException: " << e.what());
		return nullptr;
	}
}

// ����Ⱥ��
bool MysqlDao::CreateGroupChat(int creator_uid, const std::string& group_name,
	const std::vector<int>& member_uids, int& thread_id)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		// ��������
		conn->setAutoCommit(false);

		// 1. �� chat_thread ���в���Ⱥ�ļ�¼
		{
			std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('group', NOW())"
			));
			pstmt->executeUpdate();
		}

		// 2. ��ȡ�²���� thread_id
		{
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			std::unique_ptr<sql::ResultSet> rs(
				stmt->executeQuery("SELECT LAST_INSERT_ID()")
			);
			if (rs->next()) {
				thread_id = rs->getInt(1);
			}
			else {
				conn->rollback();
				return false;
			}
		}

		// 3. �� group_chat ���в���Ⱥ����Ϣ
		{
			std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
				"INSERT INTO group_chat (thread_id, name, created_at) VALUES (?, ?, NOW())"
			));
			pstmt->setInt(1, thread_id);
			pstmt->setString(2, group_name);
			if (pstmt->executeUpdate() < 0) {
				conn->rollback();
				return false;
			}
		}

		// 4. �� group_chat_member ���в������г�Ա�����������ߣ�
		{
			// �Ȳ��봴���ߣ�role=2
			std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
				"INSERT INTO group_chat_member (thread_id, user_id, role, joined_at) VALUES (?, ?, 2, NOW())"
			));
			pstmt->setInt(1, thread_id);
			pstmt->setInt(2, creator_uid);
			if (pstmt->executeUpdate() < 0) {
				conn->rollback();
				return false;
			}

			// ����������Ա��role=0
			for (int member_uid : member_uids) {
				if (member_uid == creator_uid) continue;  // ����������

				std::unique_ptr<sql::PreparedStatement> pstmt_member(conn->prepareStatement(
					"INSERT INTO group_chat_member (thread_id, user_id, role, joined_at) VALUES (?, ?, 0, NOW())"
				));
				pstmt_member->setInt(1, thread_id);
				pstmt_member->setInt(2, member_uid);
				if (pstmt_member->executeUpdate() < 0) {
					conn->rollback();
					return false;
				}
			}
		}

		// �ύ����
		conn->commit();
		LOG_INFO("CreateGroupChat success - thread_id: " << thread_id << ", creator: " << creator_uid);
		return true;
	}
	catch (sql::SQLException& e) {
		if (con) {
			conn->rollback();
		}
		LOG_ERROR("CreateGroupChat SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

// ��ȡȺ��Ա�б�
bool MysqlDao::GetGroupMembers(int thread_id, std::vector<std::shared_ptr<GroupMemberInfo>>& members)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"SELECT gcm.user_id, gcm.role, u.name, u.icon "
			"FROM group_chat_member gcm "
			"JOIN user u ON gcm.user_id = u.uid "
			"WHERE gcm.thread_id = ?"
		));
		pstmt->setInt(1, thread_id);

		std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());

		while (rs->next()) {
			auto member = std::make_shared<GroupMemberInfo>();
			member->uid = rs->getInt("user_id");
			member->name = rs->getString("name");
			member->icon = rs->getString("icon");
			member->role = rs->getInt("role");
			members.push_back(member);
		}

		LOG_DEBUG("GetGroupMembers success - thread_id: " << thread_id << ", member_count: " << members.size());
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetGroupMembers SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

// ��ȡ�û����������Ⱥ��
bool MysqlDao::GetUserGroupChats(int user_id, std::vector<int>& thread_ids)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"SELECT thread_id FROM group_chat_member WHERE user_id = ?"
		));
		pstmt->setInt(1, user_id);

		std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());

		while (rs->next()) {
			thread_ids.push_back(rs->getInt("thread_id"));
		}

		LOG_DEBUG("GetUserGroupChats success - user_id: " << user_id << ", group_count: " << thread_ids.size());
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetUserGroupChats SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::GetGroupInfo(int thread_id, GroupInfo& group_info)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"SELECT name, icon, notice, `desc`, owner_id FROM group_chat WHERE thread_id = ?"
		));
		pstmt->setInt(1, thread_id);

		std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());

		if (rs->next()) {
			group_info.thread_id = thread_id;
			group_info.name = rs->getString("name");
			group_info.icon = rs->getString("icon");
			group_info.notice = rs->getString("notice");
			group_info.desc = rs->getString("desc");
			group_info.owner_id = rs->getInt("owner_id");
			
			// 也可以顺便查一下成员总数
			std::unique_ptr<sql::PreparedStatement> count_pstmt(conn->prepareStatement(
				"SELECT COUNT(*) as count FROM group_chat_member WHERE thread_id = ?"
			));
			count_pstmt->setInt(1, thread_id);
			std::unique_ptr<sql::ResultSet> count_rs(count_pstmt->executeQuery());
			if (count_rs->next()) {
				group_info.member_count = count_rs->getInt("count");
			}
			
			return true;
		}
		return false;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetGroupInfo SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::UpdateGroupNotice(int thread_id, const std::string& notice)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"UPDATE group_chat SET notice = ? WHERE thread_id = ?"
		));
		pstmt->setString(1, notice);
		pstmt->setInt(2, thread_id);

		int update_count = pstmt->executeUpdate();
		return update_count > 0;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("UpdateGroupNotice SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::UpdateGroupMemberSetting(int thread_id, int user_id, const std::string& group_nick, int role, int is_disturb, int is_top)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	auto& conn = con->_con;

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"UPDATE group_chat_member SET group_nick = ?, role = ?, is_disturb = ?, is_top = ? "
			"WHERE thread_id = ? AND user_id = ?"
		));
		pstmt->setString(1, group_nick);
		pstmt->setInt(2, role);
		pstmt->setInt(3, is_disturb);
		pstmt->setInt(4, is_top);
		pstmt->setInt(5, thread_id);
		pstmt->setInt(6, user_id);

		int update_count = pstmt->executeUpdate();
		return update_count > 0;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("UpdateGroupMemberSetting SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::GetUnreadCounts(int user_id, std::vector<std::pair<int, int>>& unread_counts)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});

	auto& conn = con->_con;

	try {
		// 查询用户所有私聊会话中 recv_id = user_id 且 status = 0(UN_READ) 的消息数
		// 通过 private_chat 表确认哪些会话是用户参与的
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"SELECT cm.thread_id, COUNT(*) AS unread_count "
			"FROM chat_message cm "
			"JOIN private_chat pc ON cm.thread_id = pc.thread_id "
			"WHERE cm.recv_id = ? AND cm.status = 0 "
			"AND (pc.user1_id = ? OR pc.user2_id = ?) "
			"GROUP BY cm.thread_id"
		));
		pstmt->setInt(1, user_id);
		pstmt->setInt(2, user_id);
		pstmt->setInt(3, user_id);

		std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());

		unread_counts.clear();
		while (rs->next()) {
			int thread_id = rs->getInt("thread_id");
			int count = rs->getInt("unread_count");
			unread_counts.emplace_back(thread_id, count);
		}

		LOG_DEBUG("GetUnreadCounts success - user_id: " << user_id << ", thread_count: " << unread_counts.size());
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetUnreadCounts SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::MarkMsgRead(int thread_id, int reader_uid, std::string update_time)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});

	auto& conn = con->_con;

	try {
		// 将指定会话中 recv_id = reader_uid 且 status = 0(UN_READ) 的消息标记为已读(status = 2 READED)
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"UPDATE chat_message SET status = 2, updated_at = ? "
			"WHERE thread_id = ? AND recv_id = ? AND status = 0"
		));
		pstmt->setString(1, update_time);
		pstmt->setInt(2, thread_id);
		pstmt->setInt(3, reader_uid);

		int update_count = pstmt->executeUpdate();
		LOG_INFO("MarkMsgRead - thread_id: " << thread_id << ", reader_uid: " << reader_uid
			<< ", updated_count: " << update_count);
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("MarkMsgRead SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}

bool MysqlDao::GetPrivateChatPeer(int thread_id, int self_uid, int& peer_uid)
{
	auto con = pool_->getConnection();
	if (!con) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
	});

	auto& conn = con->_con;

	try {
		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
			"SELECT user1_id, user2_id FROM private_chat WHERE thread_id = ? LIMIT 1"
		));
		pstmt->setInt(1, thread_id);

		std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
		if (rs->next()) {
			int user1 = rs->getInt("user1_id");
			int user2 = rs->getInt("user2_id");
			peer_uid = (user1 == self_uid) ? user2 : user1;
			return true;
		}
		return false;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("GetPrivateChatPeer SQLException - error: " << e.what()
			<< ", code: " << e.getErrorCode() << ", state: " << e.getSQLState());
		return false;
	}
}
