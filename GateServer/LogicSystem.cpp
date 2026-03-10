#include "LogicSystem.h"
#include "HttpConnection.h"
#include "VarifygRPCClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "StatusgRPCClient.h"
#include "Logger.h"

LogicSystem::LogicSystem()
{
	//注册Get请求处理器
	RegGet("/get_test",[](std::shared_ptr<HttpConnection> connection){
		beast::ostream(connection->_response.body()) << "receive get_test request";
		//参数展示
		int i = 0;
		for (const auto& param : connection->_get_params)
		{
			++i;
			beast::ostream(connection->_response.body()) << "\nparam " << i << ": " << param.first << " = " << param.second;
		}
	});

	//注册Post请求处理器（获取验证码）
	RegPost("/get_code", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		LOG_INFO("Received POST /get_code with body: " << body_str);
		//设置回应类型
		connection->_response.set(http::field::content_type, "text/json");
		//Json处理
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		//解析失败，响应错误信息
		if (!parse_success)
		{
			LOG_ERROR("Failed to parse JSON data");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//判断email键名是否存在
		if (!src_root.isMember("email"))
		{
			LOG_ERROR("Missing email parameter");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//获取email参数
		std::string email = src_root["email"].asString();
		LOG_INFO("Email: " << email);
		//调用gRPC客户端获取验证码
		GetVarifyRsp rsp = VarifygRPCClient::GetInstance()->GetVarifyCode(email);
		root["error"] = rsp.error();
		root["email"] = src_root["email"];
		std::string jsonstr = root.toStyledString();
		beast::ostream(connection->_response.body()) << jsonstr;
		return true;
		}
	);

	//注册Post请求处理器（用户注册）
	RegPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		LOG_INFO("receive body: " << body_str);
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			LOG_ERROR("Failed to parse JSON data");
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
			LOG_WARN("password and confirm password do not match");
			root["error"] = ErrorCodes::PasswordErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//先查询redis中email对应的验证码是否合理
		std::string  varify_code;
		bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + mail, varify_code);
		//如果没有找到或者过期，响应错误信息
		if (!b_get_varify) {
			LOG_WARN("get varify code expired");
			root["error"] = ErrorCodes::VarifyExpired;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//如果验证码不匹配，响应错误信息
		if (varify_code != src_root["varifycode"].asString()) {
			LOG_WARN("varify code error");
			root["error"] = ErrorCodes::VarifyCodeErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//查询数据库中对用户是否存在
		int uid = MysqlMgr::GetInstance()->RegUser(name, mail, pwd);
		if (uid == 0 || uid == -1)
		{
			LOG_WARN("user or email exist");
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

	//注册Post请求处理器（重置密码）
	RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection) {
		auto body_str = boost::beast::buffers_to_string(connection->_request.body().data());
		LOG_INFO("receive body: " << body_str);
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			LOG_ERROR("Failed to parse JSON data");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto name = src_root["user"].asString();
		auto pwd = src_root["passwd"].asString();

		//先查询redis中email对应的验证码是否合理
		std::string  varify_code;
		bool b_get_varify = RedisMgr::GetInstance()->Get(CODEPREFIX + src_root["email"].asString(), varify_code);
		if (!b_get_varify) {
			LOG_WARN("get varify code expired");
			root["error"] = ErrorCodes::VarifyExpired;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		if (varify_code != src_root["varifycode"].asString()) {
			LOG_WARN("varify code error");
			root["error"] = ErrorCodes::VarifyCodeErr;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}
		//查询数据库中对用户名和邮箱是否匹配
		bool email_valid = MysqlMgr::GetInstance()->CheckEmail(name, email);
		if (!email_valid) {
			LOG_WARN("user email not match");
			root["error"] = ErrorCodes::EmailNotMatch;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//更新密码为新密码
		bool b_up = MysqlMgr::GetInstance()->UpdatePwd(name, pwd);
		if (!b_up) {
			LOG_ERROR("update pwd failed");
			root["error"] = ErrorCodes::PasswdUpFailed;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		LOG_INFO("succeed to update password: " << pwd);
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
		LOG_INFO("receive body: " << body_str);
		connection->_response.set(http::field::content_type, "text/json");
		Json::Value root;
		Json::Reader reader;
		Json::Value src_root;
		bool parse_success = reader.parse(body_str, src_root);
		if (!parse_success) {
			LOG_ERROR("Failed to parse JSON data");
			root["error"] = ErrorCodes::Error_Json;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		auto email = src_root["email"].asString();
		auto pwd = src_root["passwd"].asString();
		UserInfo userInfo;
		//查询数据库中对用户名和密码是否匹配
		bool pwd_valid = MysqlMgr::GetInstance()->CheckPwd(email, pwd, userInfo);
		if (!pwd_valid) {
			LOG_WARN("user pwd not match");
			root["error"] = ErrorCodes::PasswdInvalid;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		//查询StatusServer找到合适的聊天服务器
		auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
		if (reply.error()) {
			LOG_ERROR("grpc get chat server failed, error is " << reply.error());
			root["error"] = ErrorCodes::RPCFailed;
			std::string jsonstr = root.toStyledString();
			beast::ostream(connection->_response.body()) << jsonstr;
			return true;
		}

		LOG_INFO("succeed to load userinfo uid is " << userInfo.uid);
		LOG_INFO("chat server info: host=" << reply.host() << ", port=" << reply.port() << ", token=" << reply.token());
		root["error"] = 0;
		root["email"] = email;
		root["uid"] = userInfo.uid;
		root["token"] = reply.token();
		root["chathost"] = reply.host();
		root["chatport"] = reply.port();
		auto& gCfgMgr = ConfigMgr::Instance();
		std::string res_port = gCfgMgr["ResServer"]["port"];
		std::string res_host = gCfgMgr["ResServer"]["host"];
		root["reshost"] = res_host;
		root["resport"] = res_port;
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