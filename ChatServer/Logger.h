#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>

// 日志总开关：定义此宏可关闭所有日志输出
// #define DISABLE_LOGGING

namespace log_system {

    // 日志级别
    enum class LogLevel {
        DEBUG = 0,
        INFO,
        WARN,
        ERR,
        FATAL
    };

    // Logger 单例管理器
    class Logger {
    public:
        static Logger& getInstance();

        // 初始化日志系统（文件路径，最低输出级别）
        void init(const std::string& filepath, LogLevel minLevel = LogLevel::DEBUG);

        // 设置是否同时输出到控制台
        void setConsoleOutput(bool enable);

        // 设置全局最低日志级别
        void setLogLevel(LogLevel level);

        // 获取当前级别
        LogLevel getLogLevel() const;

        // 核心写日志函数（由 LogMessage 析构时调用）
        void write(LogLevel level, const std::string& formattedMsg);

    private:
        Logger() = default;
        ~Logger();

        // 禁止拷贝和赋值
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

    private:
        std::ofstream m_fileStream;
        std::mutex m_mutex;
        LogLevel m_minLevel = LogLevel::DEBUG;
        bool m_consoleOutput = true;
    };

    // 辅助类：用于捕获并格式化单条日志
    class LogMessage {
    public:
        LogMessage(LogLevel level, const char* file, int line, const char* func);
        ~LogMessage();

        // 暴露输出流以便接收数据
        std::ostream& stream() { return m_stream; }

    private:
        // 提取文件名的辅助函数（去路径）
        const char* getFileName(const char* path);

        // 获取当前时间的格式化字符串
        std::string getTimestamp();

        // 获取级别的字符串表示
        const char* getLevelString(LogLevel level);

        // 获取颜色的 ANSI 转义码
        const char* getColorCode(LogLevel level);

    private:
        LogLevel m_level;
        std::ostringstream m_stream;
        std::string m_prefix;
    };

} // namespace log_system

// ---------------------------------------------------------
// 宏定义：提供流式使用风格
// 示例: LOG_INFO("connect to " << ip << ":" << port);
// ---------------------------------------------------------

#ifdef DISABLE_LOGGING
#define LOG_DEBUG(msg) do {} while(0)
#define LOG_INFO(msg)  do {} while(0)
#define LOG_WARN(msg)  do {} while(0)
#define LOG_ERROR(msg) do {} while(0)
#define LOG_FATAL(msg) do {} while(0)
#else
#define LOG_MACRO_BODY(level, msg) \
        do { \
            if (level >= log_system::Logger::getInstance().getLogLevel()) { \
                log_system::LogMessage(level, __FILE__, __LINE__, __func__).stream() << msg; \
            } \
        } while(0)

#define LOG_DEBUG(msg) LOG_MACRO_BODY(log_system::LogLevel::DEBUG, msg)
#define LOG_INFO(msg)  LOG_MACRO_BODY(log_system::LogLevel::INFO,  msg)
#define LOG_WARN(msg)  LOG_MACRO_BODY(log_system::LogLevel::WARN,  msg)
#define LOG_ERROR(msg) LOG_MACRO_BODY(log_system::LogLevel::ERR, msg)
#define LOG_FATAL(msg) LOG_MACRO_BODY(log_system::LogLevel::FATAL, msg)
#endif