#include <iostream>
#include "CServer.h"
#include "ConfigMgr.h"

#include "Logger.h"
#include <direct.h>  // for _mkdir on Windows
#include <sys/stat.h>
#include <iomanip>

int main()
{
	// 初始化日志系统
	try {
		// 创建 log 目录 (使用 C++14 兼容方式)
#if defined(_WIN32) || defined(_WIN64)
		_mkdir("log");
#else
		mkdir("log", 0755);
#endif

		// 获取当前时间作为日志文件名的一部分
		auto now = std::chrono::system_clock::now();
		auto time_t_now = std::chrono::system_clock::to_time_t(now);
		std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&tm_buf, &time_t_now);
#else
		localtime_r(&time_t_now, &tm_buf);
#endif
		char time_str[128];
		strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", &tm_buf);
		std::string log_file = "log/gateserver_" + std::string(time_str) + ".log";

		// 初始化 logger，设置最低日志级别为 DEBUG
		log_system::Logger::getInstance().init(log_file, log_system::LogLevel::DEBUG);
		log_system::Logger::getInstance().setConsoleOutput(true);
	}
	catch (std::exception& e) {
		std::cerr << "Logger init failed: " << e.what() << std::endl;
		return -1;
	}

	//读取配置文件，获取服务器监听端口
	auto& gCfgMgr = ConfigMgr::Instance();
	std::string gate_port_str = gCfgMgr["GateServer"]["port"];
	if (gate_port_str.empty()) {
		LOG_ERROR("config error: [GateServer].port is missing or empty");
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
		LOG_INFO("GateServer is running on port " << gate_port);
		ioc.run();
	}
	catch (std::exception& e)
	{
		LOG_FATAL("main exception: " << e.what());
		return EXIT_FAILURE;
	}
}
