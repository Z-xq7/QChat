#pragma once
#include "const.h"

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

//LogicSystem类，负责处理客户端请求，维护Get和Post请求的处理函数映射表
class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	//~LogicSystem() {};
	//处理Get请求
	bool HandleGet(std::string,std::shared_ptr<HttpConnection>);
	//注册Get请求处理函数到map表
	void RegGet(std::string, HttpHandler);
	//处理Post请求
	bool HandlePost(std::string, std::shared_ptr<HttpConnection>);
	//注册Post请求处理函数到map表
	void RegPost(std::string, HttpHandler);

private:
	LogicSystem();
	//Get请求处理函数映射表，key为url，value为处理函数
	std::map<std::string, HttpHandler> _get_handlers;
	//Post请求处理函数映射表，key为url，value为处理函数
	std::map<std::string, HttpHandler> _post_handlers;

};

