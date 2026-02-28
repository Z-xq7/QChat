#pragma once
#include "const.h"


//CServer类，负责监听端口，接受连接，交给HttpConnection类管理连接
class CServer:public std::enable_shared_from_this<CServer>
{
public:
	CServer(boost::asio::io_context& ioc,unsigned short& port);
	//启动服务
	void Start();

private:
	tcp::acceptor _acceptor;
	net::io_context& _ioc;
};

