#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VarifygRPCClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusgRPCClient.h"

LogicSystem::LogicSystem()
{
	//注册Get请求处理函数
	RegGet("/get_test",[](std::shared_ptr<HttpConnection> connection){
		beast::ostream(connection->_response.body()) << "receive get_test request";
		//参数显示
		int i = 0;
		for (const auto& param : connection->_get_params)
		{
			++i;
			beast::ostream(connection->_response.body()) << "\nparam " << i << ": " << param.first << " = " << param.second;
		}
	});

	//注册Post请求处理函数（获取验证码）
	RegPost("/get_code", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		std::cout << "--- Received POST /get_code with body: " << body_str << " ---" <<std::endl;
		//设置回应类型
		connection->_response.set(http::field::content_type, "text/json");
		//Json解析
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		//解析失败则回应错误信息
		if (!parse_success)
		{
			SetColor(RED);
			std::cout << "*** Failed to parse JSON data! ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//判断email参数是否存在
		if (!src_root.isMember("email"))
		{
			SetColor(RED);
			std::cout << "*** Missing email parameter! ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//提取email参数
		std::string email = src_root["email"].asString();
		std::cout << "--- Email: " << email << " ---" << std::endl;
		//调用gRPC客户端获取验证码
		GetVarifyRsp rsp = VarifygRPCClient::GetInstance()->GetVarifyCode(email);
		root["error"] = rsp.error();
		root["email"] = src_root["email"];
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
		}
	);

	//注册Post请求处理函数（用户注册）
	RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		std::cout << "--- receive body is " << body_str << " ---" << std::endl;
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			SetColor(RED);
			std::cout << "*** Failed to parse JSON data! ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto name = src_root["user"].asString();
		auto mail = src_root["email"].asString();
		auto pwd = src_root["passwd"].asString();
		auto confirm = src_root["confirm"].asString();

		if (pwd != confirm)
		{
			std::cout << "*** password and confirm password do not match ***" << std::endl;
			root["error"] = ErrorCodes::PasswordErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//先查找redis中email对应的验证码是否合理
		std::string  varify_code;
		bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + mail, varify_code);
		//如果没有找到或者过期了则回应错误信息
		if (!b_get_varify) {
			std::cout << "*** get varify code expired ***" << std::endl;
			root["error"] = ErrorCodes::VarifyExpired;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//如果验证码不匹配则回应错误信息
		if (varify_code != src_root["varifycode"].asString()) {
			std::cout << "*** varify code error ***" << std::endl;
			root["error"] = ErrorCodes::VarifyCodeErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//查找数据库判断用户是否存在
		int uid = MysqlMgr::GetInstance()->RegUser(name, mail, pwd);
		if (uid == 0 || uid == -1)
		{
			std::cout << "*** user or email exist ***" << std::endl;
			root["error"] = ErrorCodes::UserExist;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		root["error"] = 0;
		root["email"] = mail;
		root["user"] = name;
		root["passwd"] = pwd;
		root["confirm"] = confirm;
		root["varifycode"] = src_root["varifycode"].asString();
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
		}
	);

	//注册Post请求处理函数（重置密码）
	RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		SetColor(YELLOW);
		std::cout << "receive body is " << body_str << std::endl;
		SetColor(RESET);
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			SetColor(RED);
			std::cout << "*** Failed to parse JSON data! ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto name = src_root["user"].asString();
		auto pwd = src_root["passwd"].asString();

		//先查找redis中email对应的验证码是否合理
		std::string  varify_code;
		bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), varify_code);
		if (!b_get_varify) {
			SetColor(RED);
			std::cout << "*** get varify code expired ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::VarifyExpired;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		if (varify_code != src_root["varifycode"].asString()) {
			SetColor(RED);
			std::cout << "*** varify code error ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::VarifyCodeErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//查询数据库判断用户名和邮箱是否匹配
		bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
		if (!email_valid) {
			SetColor(RED);
			std::cout << "*** user email not match ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::EmailNotMatch;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//更新密码为最新密码
		bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
		if (!b_up) {
			SetColor(RED);
			std::cout << "*** update pwd failed ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::PasswdUpFailed;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		SetColor(GREEN);
		std::cout << "--- succeed to update password ---" << pwd << std::endl;
		SetColor(RESET);
		root["error"] = 0;
		root["email"] = email;
		root["user"] = name;
		root["passwd"] = pwd;
		root["varifycode"] = src_root["varifycode"].asString();
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
		}
	);

	//用户登录逻辑
	RegPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		SetColor(YELLOW);
		std::cout << "receive body is " << body_str << std::endl;
		SetColor(RESET);
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			SetColor(RED);
			std::cout << "*** Failed to parse JSON data! ***" << std::endl;
			SetColor(RESET);
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto pwd = src_root["passwd"].asString();
		UserInfo userInfo;
		//查询数据库判断用户名和密码是否匹配
		bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
		if (!pwd_valid) {
			std::cout << "*** user pwd not match ***" << std::endl;
			root["error"] = ErrorCodes::PasswdInvalid;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//查询StatusServer找到合适的连接
		auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
		if (reply.error()) {
			std::cout << "*** grpc get chat server failed, error is " << reply.error() << " ***" << std::endl;
			root["error"] = ErrorCodes::RPCFailed;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		SetColor(GREEN);
		std::cout << "--- succeed to load userinfo uid is " << userInfo.uid << " ---" << std::endl;
		std::cout << "chat server info: [ host: " << reply.host() << " ]" << std::endl << 
			"[ port: " << reply.port() << " ]" << std::endl << "[ token: " << reply.token() << " ]" << std::endl;
		SetColor(RESET);
		root["error"] = 0;
		root["email"] = email;
		root["uid"] = userInfo.uid;
		root["token"] = reply.token();
		root["host"] = reply.host();
		root["port"] = reply.port();
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
		}
	);
}

//注册Get请求处理函数到map表
void LogicSystem::RegGet(std::string url, HttpHandler handler)
{
	_get_handlers.insert(std::make_pair(url,handler));
}

//处理Get请求
bool LogicSystem::HandleGet(std::string path, std::shared_ptr<HttpConnection> conn)
{
	//没有找到对应处理函数
	if (_get_handlers.find(path) == _get_handlers.end())
	{
		return false;
	}
	//找到了->调用对应的处理函数
	_get_handlers[path](conn);
	return true;
}

//注册Post请求处理函数到map表
void LogicSystem::RegPost(std::string url, HttpHandler handler)
{
	_post_handlers.insert(std::make_pair(url, handler));
}

//处理Post请求
bool LogicSystem::HandlePost(std::string path, std::shared_ptr<HttpConnection> conn)
{
	//没有找到对应处理函数
	if (_post_handlers.find(path) == _post_handlers.end())
	{
		return false;
	}
	//找到了->调用对应的处理函数
	_post_handlers[path](conn);
	return true;
}