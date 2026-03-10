#include "MysqlDao.h"
#include "ConfigMgr.h"
#include "Logger.h"

MysqlDao::MysqlDao()
{
	auto & cfg = ConfigMgr::Inst();
	const auto& host = cfg["Mysql"]["Host"];
	const auto& port = cfg["Mysql"]["Port"];
	const auto& pwd = cfg["Mysql"]["Passwd"];
	const auto& schema = cfg["Mysql"]["Schema"];
	const auto& user = cfg["Mysql"]["User"];
	pool_.reset(new MySqlPool(host+":"+port, user, pwd,schema, 5));
}

MysqlDao::~MysqlDao(){
	pool_->Close();
}

int MysqlDao::RegUser(const std::string& name, const std::string& email, const std::string& pwd)
{
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}
		// 准备好调用存储过程
		std::unique_ptr < sql::PreparedStatement > stmt(con->_con->prepareStatement("CALL reg_user(?,?,?,@result)"));
		// 设置输入参数
		stmt->setString(1, name);
		stmt->setString(2, email);
		stmt->setString(3, pwd);

		// 因为PreparedStatement不直接支持注册输出参数，我们需要使用会话变量的方式来获取输出参数值

		  // 执行存储过程
		stmt->execute();
		// 因为存储过程内用了会话变量来返回结果，我们以查询的方式获取输出参数值，后面紧接着执行SELECT查询来获取它
	   // 例如，在存储过程里返回了一个会话变量@result，存储过程执行后，我们可以获取它
	   std::unique_ptr<sql::Statement> stmtResult(con->_con->createStatement());
	  std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
	  if (res->next()) {
	       int result = res->getInt("result");
		  LOG_DEBUG("Register user result: " << result);
		  pool_->returnConnection(std::move(con));
		  return result;
	  }
	  pool_->returnConnection(std::move(con));
		return -1;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
		return -1;
	}
}

bool MysqlDao::CheckEmail(const std::string& name, const std::string& email) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT email FROM user WHERE name = ?"));

		// 绑定参数
		pstmt->setString(1, name);

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		// 处理结果集
		while (res->next()) {
			LOG_DEBUG("Check Email: " << res->getString("email"));
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
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
		return false;
	}
}

bool MysqlDao::UpdatePwd(const std::string& name, const std::string& newpwd) {
	auto con = pool_->getConnection();
	try {
		if (con == nullptr) {
			return false;
		}

		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));

		// 绑定参数
		pstmt->setString(2, name);
		pstmt->setString(1, newpwd);

		// 执行更新
		int updateCount = pstmt->executeUpdate();

		LOG_DEBUG("Updated rows: " << updateCount);
		pool_->returnConnection(std::move(con));
		return true;
	}
	catch (sql::SQLException& e) {
		pool_->returnConnection(std::move(con));
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 准备SQL语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name); // 把username替换为需要查询的用户名

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::string origin_pwd = "";
		// 处理结果集
		while (res->next()) {
			origin_pwd = res->getString("pwd");
			// 输出查询到的信息
			LOG_DEBUG("Check password for user: " << name);
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
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 准备SQL语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement(
			"INSERT INTO friend_apply (from_uid, to_uid, descs, back_name) values (?,?,?,?) "
			"ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid, descs = ?, back_name = ?"));
		pstmt->setInt(1, from); // from id
		pstmt->setInt(2, to);
		pstmt->setString(3, desc);
		pstmt->setString(4, back_name);
		pstmt->setString(5, desc);
		pstmt->setString(6, back_name);
		// 执行更新
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
		return false;
	}
	return true;
}

//认证好友申请
bool MysqlDao::AuthFriendApply(const int& from, const int& to) {
	auto con = pool_->getConnection();
	if (con == nullptr) {
		return false;
	}

	Defer defer([this, &con]() {
		pool_->returnConnection(std::move(con));
		});

	try {
		// 准备SQL语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("UPDATE friend_apply SET status = 1 "
			"WHERE from_uid = ? AND to_uid = ?"));
		//这里申请的时候from，认证时to
		pstmt->setInt(1, to); // from id
		pstmt->setInt(2, from);
		// 执行更新
		int rowAffected = pstmt->executeUpdate();
		if (rowAffected < 0) {
			return false;
		}
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
		return false;
	}

	return true;
}

//最好回调分开成功后为了加好友单独隔离，需要操作很多步骤
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
		//开启事务
		con->_con->setAutoCommit(false);
		std::string reverse_back;
		std::string apply_desc;
		{
			// 1. 先查询获取
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
				// 没有对应的申请记录，直接 rollback 并返回失败
				con->_con->rollback();
				return false;
			}
		}
		{
			// 2. 执行申请的更新
			std::unique_ptr<sql::PreparedStatement> updStmt(con->_con->prepareStatement(
				"UPDATE friend_apply "
				"SET status = 1 "
				"WHERE from_uid = ? AND to_uid = ?"
			));
			updStmt->setInt(1, to);
			updStmt->setInt(2, from);
			if (updStmt->executeUpdate() != 1) {
				// 更新不符合预期，回滚
				con->_con->rollback();
				return false;
			}
		}
		{
			// 3. 准备第一条SQL语句, 保证对方加自己
			std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//这里申请的时候from，认证时to
			pstmt->setInt(1, from); // from id
			pstmt->setInt(2, to);
			pstmt->setString(3, back_name);
			// 执行更新
			int rowAffected = pstmt->executeUpdate();
			if (rowAffected < 0) {
				con->_con->rollback();
				return false;
			}
			//准备第二条SQL语句，保证自己加对方数据
			std::unique_ptr<sql::PreparedStatement> pstmt2(con->_con->prepareStatement("INSERT IGNORE INTO friend(self_id, friend_id, back) "
				"VALUES (?, ?, ?) "
			));
			//这里申请的时候from，认证时to
			pstmt2->setInt(1, to); // from id
			pstmt2->setInt(2, from);
			pstmt2->setString(3, reverse_back);
			// 执行更新
			int rowAffected2 = pstmt2->executeUpdate();
			if (rowAffected2 < 0) {
				con->_con->rollback();
				return false;
			}
		}
		// 4. 创建 chat_thread
		long long threadId = 0;
		{
			std::unique_ptr<sql::PreparedStatement> threadStmt(con->_con->prepareStatement(
				"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());"
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
				return false;
			}
		}
		// 5. 创建 private_chat
		{
			std::unique_ptr<sql::PreparedStatement> pcStmt(con->_con->prepareStatement(
				"INSERT INTO private_chat(thread_id, user1_id, user2_id) VALUES (?, ?, ?)"
			));
			pcStmt->setInt64(1, threadId);
			pcStmt->setInt(2, from);
			pcStmt->setInt(3, to);
			if (pcStmt->executeUpdate() < 0) return false;
		}
		// 6. 可选：插入初始消息到 chat_message
		if (apply_desc.empty() == false)
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content,created_at, updated_at, status) VALUES (?, ?, ?, ?,NOW(),NOW(),?)"
			));
			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, to);
			msgStmt->setInt(3, from);
			msgStmt->setString(4, apply_desc);
			//这里都直接设置为已读因为都是添加的所以改成未读
			msgStmt->setInt(5, 2);
			if (msgStmt->executeUpdate() < 0) { return false; }
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
				LOG_DEBUG("AddFriend insert message success, message_id: " << messageId);
				chat_datas.push_back(tx_data);
			}
			else {
				return false;
			}
		}
		{
			std::unique_ptr<sql::PreparedStatement> msgStmt(con->_con->prepareStatement(
				"INSERT INTO chat_message(thread_id, sender_id, recv_id, content, created_at, updated_at, status) VALUES (?, ?, ?, ?,NOW(),NOW(),?)"
			));
			msgStmt->setInt64(1, threadId);
			msgStmt->setInt(2, from);
			msgStmt->setInt(3, to);
			msgStmt->setString(4, "We are friends now!");
			//这里都直接设置为已读因为都是添加的所以改成未读
			msgStmt->setInt(5, 2);
			if (msgStmt->executeUpdate() < 0) { return false; }
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
				return false;
			}
		}
		// 提交事务
		con->_con->commit();
		LOG_INFO("AddFriend insert friends success, thread_id: " << threadId);
		return true;
	}
	catch (sql::SQLException& e) {
		// 如果发生错误，回滚事务
		if (con) {
			con->_con->rollback();
		}
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 准备SQL语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE uid = ?"));
		pstmt->setInt(1, uid); // 把uid替换为需要查询的uid

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// 处理结果集
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
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 准备SQL语句
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("SELECT * FROM user WHERE name = ?"));
		pstmt->setString(1, name); // 把uid替换为需要查询的uid

		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		std::shared_ptr<UserInfo> user_ptr = nullptr;
		// 处理结果集
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
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 准备SQL语句, 用起始id和限制来分页查询列表
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select apply.from_uid, apply.status, user.name, "
			"user.nick, user.sex,user.icon from friend_apply as apply join user on apply.from_uid = user.uid where apply.to_uid = ? "
			"and apply.id > ? order by apply.id ASC LIMIT ? "));

		pstmt->setInt(1, touid); // 把uid替换为需要查询的uid
		pstmt->setInt(2, begin); // 起始id
		pstmt->setInt(3, limit); //偏移量
		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// 处理结果集
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
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 准备SQL语句, 用起始id和限制来分页查询列表
		std::unique_ptr<sql::PreparedStatement> pstmt(con->_con->prepareStatement("select * from friend where self_id = ? "));

		pstmt->setInt(1, self_id); // 把uid替换为需要查询的uid
	
		// 执行查询
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// 处理结果集
		while (res->next()) {		
			auto friend_id = res->getInt("friend_id");
			auto back = res->getString("back");
			//再一步查询friend_id对应的信息
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
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
		return false;
	}
	return true;
}

bool MysqlDao::GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
	std::vector<std::shared_ptr<ChatThreadInfo>>& threads, bool& loadMore, int& nextLastId)
{
	// 初始状态
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
		// 准备分页查询：CTE + UNION ALL + ORDER + LIMIT N+1
		std::string sql =
			"WITH all_threads AS ( "
			"  SELECT thread_id, 'private' AS type, user1_id, user2_id "
			"    FROM private_chat "
			"   WHERE (user1_id = ? OR user2_id = ?) "
			"     AND thread_id > ? "
			"  UNION ALL "
			"  SELECT thread_id, 'group'   AS type, 0 AS user1_id, 0 AS user2_id "
			"    FROM group_chat_member "
			"   WHERE user_id   = ? "
			"     AND thread_id > ? "
			") "
			"SELECT thread_id, type, user1_id, user2_id "
			"  FROM all_threads "
			" ORDER BY thread_id "
			" LIMIT ?;";
		std::unique_ptr<sql::PreparedStatement> pstmt(
			conn->prepareStatement(sql));
		// 绑定参数，? 对应 (userId, userId, lastId, userId, lastId, pageSize+1)
		int idx = 1;
		pstmt->setInt64(idx++, userId);              // private.user1_id
		pstmt->setInt64(idx++, userId);              // private.user2_id
		pstmt->setInt64(idx++, lastId);              // private.thread_id > lastId
		pstmt->setInt64(idx++, userId);              // group.user_id
		pstmt->setInt64(idx++, lastId);              // group.thread_id > lastId
		pstmt->setInt(idx++, pageSize + 1);          // LIMIT pageSize+1
		// 执行
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
		// 先把结果读到临时容器
		std::vector<std::shared_ptr<ChatThreadInfo>> tmp;
		while (res->next()) {
			auto cti = std::make_shared<ChatThreadInfo>();
			cti->_thread_id = res->getInt64("thread_id");
			cti->_type = res->getString("type");
			cti->_user1_id = res->getInt64("user1_id");
			cti->_user2_id = res->getInt64("user2_id");
			tmp.push_back(cti);
		}
		// 判断是否获取到一条
		if ((int)tmp.size() > pageSize) {
			loadMore = true;
			tmp.pop_back();  // 只返回 pageSize+1 条
		}
		// 返回最后一条，并让 nextLastId 为最后一条的 thread_id
		if (!tmp.empty()) {
			nextLastId = tmp.back()->_thread_id;
		}
		// 赋值给输出
		threads = std::move(tmp);
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("SQLException: " << e.what() << " (MySQL error code: " << e.getErrorCode() 
			<< ", SQLState: " << e.getSQLState() << ")");
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
		// 开启事务
		conn->setAutoCommit(false);
		// 1. 先尝试查询已存在的记录(锁行)
		std::string check_sql =
			"SELECT thread_id FROM private_chat "
			"WHERE user1_id = ? AND user2_id = ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(check_sql));
		pstmt->setInt64(1, uid1);
		pstmt->setInt64(2, uid2);
		std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

		if (res->next()) {
			// 如果已存在，返回复用 thread_id
			thread_id = res->getInt("thread_id");
			conn->commit();  // 提交事务
			return true;
		}

		// 2. 如果未找到，则创建新的 chat_thread 和 private_chat 记录
		// 在 chat_thread 表插入记录
		std::string insert_chat_thread_sql =
			"INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW());";

		std::unique_ptr<sql::PreparedStatement> pstmt_insert_thread(conn->prepareStatement(insert_chat_thread_sql));
		pstmt_insert_thread->executeUpdate();

		// 获取新插入的 thread_id
		std::string get_last_insert_id_sql = "SELECT LAST_INSERT_ID();";
		std::unique_ptr<sql::PreparedStatement> pstmt_last_insert_id(conn->prepareStatement(get_last_insert_id_sql));
		std::unique_ptr<sql::ResultSet> res_last_id(pstmt_last_insert_id->executeQuery());
		res_last_id->next();
		thread_id = res_last_id->getInt(1);

		// 3. 在 private_chat 表插入记录
		std::string insert_private_chat_sql =
			"INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
			"VALUES (?, ?, ?, NOW());";


		std::unique_ptr<sql::PreparedStatement> pstmt_insert_private(conn->prepareStatement(insert_private_chat_sql));
		pstmt_insert_private->setInt64(1, thread_id);
		pstmt_insert_private->setInt64(2, uid1);
		pstmt_insert_private->setInt64(3, uid2);
		pstmt_insert_private->executeUpdate();

		// 提交事务
		conn->commit();
		return true;
	}
	catch (sql::SQLException& e) {
		conn->rollback();

		// 检查是否是唯一键冲突 (MySQL error code 1062)
		if (e.getErrorCode() == 1062) {
			// 再次查询已存在的记录
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

		LOG_ERROR("SQLException: " << e.what() << " (Code: " << e.getErrorCode() << ")");
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
		page_res->next_cursor = last_message_id;
		// SQL：获取一条额外的，判断是否有更多
		const std::string sql = R"(
        SELECT message_id, thread_id, sender_id, recv_id, content,
               created_at, updated_at, status
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
		// 获取 fetch_limit 条记录
		while (rs->next()) {
			ChatMessage msg;
			msg.message_id = rs->getUInt64("message_id");
			msg.thread_id = rs->getUInt64("thread_id");
			msg.sender_id = rs->getUInt64("sender_id");
			msg.recv_id = rs->getUInt64("recv_id");
			msg.content = rs->getString("content");
			msg.chat_time = rs->getString("created_at");
			msg.status = rs->getInt("status");
			page_res->messages.push_back(std::move(msg));
		}
		return page_res;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("SQLException: " << e.what());
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
		//关闭自动提交，手动开启事务
		conn->setAutoCommit(false);
		auto pstmt = std::unique_ptr<sql::PreparedStatement>(
			conn->prepareStatement(
				"INSERT INTO chat_message "
				"(thread_id, sender_id, recv_id, content, created_at, updated_at, status) "
				"VALUES (?, ?, ?, ?, ?, ?, ?)"
			)
		);

		for (auto& msg : chat_datas) {
			// 普通字段
			pstmt->setUInt64(1, msg->thread_id);
			pstmt->setUInt64(2, msg->sender_id);
			pstmt->setUInt64(3, msg->recv_id);
			pstmt->setString(4, msg->content);
			pstmt->setString(5, msg->chat_time);  // created_at
			pstmt->setString(6, msg->chat_time);  // updated_at
			pstmt->setInt(7, msg->status);

			pstmt->executeUpdate();

			// 2. 取 LAST_INSERT_ID()
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
		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("SQLException: " << e.what());
		conn->rollback();
		return false;
	}
	return true;
}

bool MysqlDao::UpdateHeadInfo(int uid, const std::string& icon)
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
		std::string update_sql = "UPDATE user SET icon = ? WHERE uid = ?;";

		std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(update_sql));
		pstmt->setString(1, icon);
		pstmt->setInt64(2, uid);

		int affected_rows = pstmt->executeUpdate();

		// 检查是否有被更新，可选的
		if (affected_rows == 0) {
			LOG_WARN("No user found with uid: " << uid);
			return false;
		}

		return true;
	}
	catch (sql::SQLException& e) {
		LOG_ERROR("SQLException in UpdateHeadInfo: " << e.what());
		return false;
	}
	return false;
}
