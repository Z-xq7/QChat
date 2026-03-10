#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOServicePool.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include <boost/filesystem.hpp>

#include "Logger.h"
#include <direct.h>  // for _mkdir on Windows
#include <sys/stat.h>
#include <iomanip>

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

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
		std::string log_file = "log/resourceserver_" + std::string(time_str) + ".log";

		// 初始化 logger，设置最低日志级别为 DEBUG
		log_system::Logger::getInstance().init(log_file, log_system::LogLevel::DEBUG);
		log_system::Logger::getInstance().setConsoleOutput(true);
		
		LOG_INFO("Logger system initialized successfully, log file: " << log_file);
	}
	catch (std::exception& e) {
		// Logger 初始化失败时无法使用 LOG_ERROR，保留 cerr
		std::cerr << "Logger init failed: " << e.what() << std::endl;
		return -1;
	}

	auto& cfg = ConfigMgr::Inst();
	auto server_name = cfg["SelfServer"]["Name"];
	LOG_INFO("ResourceServer [" << server_name << "] starting...");

	std::shared_ptr<AsioIOServicePool> pool = nullptr;
	try {
		pool = AsioIOServicePool::GetInstance();

		boost::asio::io_context  io_context;
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&io_context, pool](auto, auto) {
			LOG_WARN("Received shutdown signal, stopping services...");
			io_context.stop();
			pool->Stop();
			});
		auto port_str = cfg["SelfServer"]["Port"];
		CServer s(io_context, atoi(port_str.c_str()));
		LOG_INFO("ResourceServer started successfully");
		io_context.run();
		LOG_INFO("ResourceServer shutdown completed");
	}
	catch (std::exception& e) {
		if (pool) {
			pool->Stop();
		}
		LOG_FATAL("ResourceServer exception: " << e.what());
	}

}