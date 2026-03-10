#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "const.h"
#include "ConfigMgr.h"
#include "hiredis.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"
#include "AsioIOServicePool.h"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include "message.grpc.pb.h"
#include "StatusServiceImpl.h"

#include "Logger.h"
#include <direct.h>  // for _mkdir on Windows
#include <sys/stat.h>
#include <iomanip>

void RunServer() {
    auto& cfg = ConfigMgr::Instance();

    std::string server_address(cfg["StatusServer"]["host"] + ":" + cfg["StatusServer"]["port"]);
    StatusServiceImpl service;

	// 创建并配置gRPC服务器
    grpc::ServerBuilder builder;
    // 监听端口和添加服务
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    // 构建并启动gRPC服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    LOG_INFO("Server listening on " << server_address);

    // 创建Boost.Asio的io_context
    boost::asio::io_context io_context;
    // 创建signal_set用于捕获SIGINT
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);

    // 设置异步等待SIGINT信号
    signals.async_wait([&server](const boost::system::error_code& error, int signal_number) {
        if (!error) {
            LOG_WARN("Shutting down server...");
            server->Shutdown(); // 优雅地关闭服务器
        }
        });

    // 在单独的线程中运行io_context
    std::thread([&io_context]() { io_context.run(); }).detach();

    // 等待服务器关闭
    server->Wait();
    io_context.stop(); // 停止io_context
    LOG_INFO("Server stopped successfully");
}

int main(int argc, char** argv) {
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
        std::string log_file = "log/statusserver_" + std::string(time_str) + ".log";

        // 初始化 logger，设置最低日志级别为 DEBUG
        log_system::Logger::getInstance().init(log_file, log_system::LogLevel::DEBUG);
        log_system::Logger::getInstance().setConsoleOutput(true);
        
        LOG_INFO("Logger initialized successfully, log file: " << log_file);
    }
    catch (std::exception& e) {
        std::cerr << "Logger init failed: " << e.what() << std::endl;
        return -1;
    }

    try {
        RunServer();
    }
    catch (std::exception const& e) {
        LOG_FATAL("Server error: " << e.what());
        return EXIT_FAILURE;
    }

    return 0;
}