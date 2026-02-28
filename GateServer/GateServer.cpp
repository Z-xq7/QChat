#include <iostream>
#include "CServer.h"
#include "ConfigMgr.h"

int main()
{
	//读取配置文件，获取服务器监听端口
	auto& gCfgMgr = ConfigMgr::Instance();
	std::string gate_port_str = gCfgMgr["GateServer"]["port"];
	if (gate_port_str.empty()) {
		SetColor(RED);
		std::cerr << "*** config error: [GateServer].port is missing or empty ***" << std::endl;
		SetColor(RESET);
		return EXIT_FAILURE;
	}
	unsigned short gate_port = atoi(gate_port_str.c_str());

	try
	{
		//创建一个io_context对象，提供异步操作的核心I/O功能
		net::io_context ioc{1};
		//创建信号集，用于处理终止信号
		boost::asio::signal_set signals(ioc,SIGINT,SIGTERM);
		signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number)
		{
			//处理信号
			if (ec)
			{
				return;
			}
			ioc.stop();
		});
		//创建服务器对象，监听指定端口，并启动服务器
		std::make_shared<CServer>(ioc, gate_port)->Start();
		SetColor(GREEN);
		std::cout << "--- GateServer is running on port " << gate_port << " ---" << std::endl;
		SetColor(RESET);
		ioc.run();
	}
	catch (std::exception& e)
	{
		SetColor(RED);
		std::cout << "*** main err is: " << e.what() << " ***" << std::endl;
		SetColor(RESET);
		return EXIT_FAILURE;
	}
}
