#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOServicePool.h"
#include "ChatServiceImpl.h"
#include "CServer.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"

#include "Logger.h"
#include <direct.h>  // for _mkdir on Windows
#include <sys/stat.h>
#include <iomanip>

using namespace std;
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
		std::string log_file = "log/chatserver_" + std::string(time_str) + ".log";
		
		// 初始化 logger，设置最低日志级别为 DEBUG
		log_system::Logger::getInstance().init(log_file, log_system::LogLevel::DEBUG);
		log_system::Logger::getInstance().setConsoleOutput(true);
	}
	catch (std::exception& e) {
		std::cerr << "Logger init failed: " << e.what() << std::endl;
		return -1;
	}

	LOG_INFO("========== ChatServer Starting ==========");
	
	auto& cfg = ConfigMgr::Inst();
	auto server_name = cfg["SelfServer"]["Name"];
	LOG_INFO("Server name: " << server_name);
	
	try {
		auto pool = AsioIOServicePool::GetInstance();
		//将登录数设置为0
		RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0");
		Defer derfer([server_name]() {
			RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
			RedisMgr::GetInstance()->Close();
			});

		boost::asio::io_context  io_context;
		auto port_str = cfg["SelfServer"]["Port"];
		//创建Cserver智能指针
		auto pointer_server = std::make_shared<CServer>(io_context, atoi(port_str.c_str()));

		//启动定时器
		pointer_server->StartTimer();

		//定义一个GrpcServer
		std::string server_address(cfg["SelfServer"]["Host"] + ":" + cfg["SelfServer"]["RPCPort"]);
		ChatServiceImpl service;
		grpc::ServerBuilder builder;
		// 监听端口和添加服务
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		builder.RegisterService(&service);
		service.RegisterServer(pointer_server);
		// 构建并启动gRPC服务器
		std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
		LOG_INFO("RPC Server listening on " << server_address);

		//单独启动一个线程处理grpc服务
		std::thread  grpc_server_thread([&server]() {
			server->Wait();
			});

		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&io_context, pool, &server](auto, auto) {
			LOG_INFO("Received shutdown signal, stopping server...");
			io_context.stop();
			pool->Stop();
			server->Shutdown();
			});

		//将Cserver注册给逻辑类方便以后清除连接
		LogicSystem::GetInstance()->SetServer(pointer_server);
		io_context.run();

		//等待grpc服务线程结束
		grpc_server_thread.join();
		//停止定时器
		pointer_server->StopTimer();
		LOG_INFO("========== ChatServer Stopped ==========");
		return 0;
    }
    catch (std::exception& e) {
        LOG_ERROR("Exception: " << e.what());
		return -1;
    }
}