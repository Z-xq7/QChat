#pragma once
#include "const.h"
#include "LogicSystem.h"

//HttpConnection类，负责管理客户端连接，处理客户端请求，回发数据给客户端
class HttpConnection:public std::enable_shared_from_this<HttpConnection>
{
	friend class LogicSystem;
public:
	HttpConnection(boost::asio::io_context& ioc);
	//启动连接，异步读取客户端请求
	void Start();
	//获取套接字引用，供CServer类使用
	tcp::socket& GetSocket() { return _socket; }
private:
	//定时器检测客户端超时连接（掉线）
	void CheckDeadLine();
	//预处理Get请求参数(解析url路径)
	void PreParseGetParam();
	//处理客户端请求（解析请求头、请求体）
	void HandleReq();
	//响应客户端（回发数据）
	void WriteResponse();

	//套接字
	tcp::socket _socket;
	//接受数据缓冲区
	beast::flat_buffer _buffer{8192};
	//用来解析请求
	http::request<http::dynamic_body> _request;
	//用来回应客户端
	http::response<http::dynamic_body> _response;
	//用来做定时器判断请求是否超时
	net::steady_timer deadline_{
		_socket.get_executor(),std::chrono::seconds(60)
	};
	//Get请求的url路径
	std::string _get_url;
	//Get请求的参数
	std::unordered_map<std::string, std::string> _get_params;
};

