#include "Logger.h"
#include <iomanip>
#include <cstring>

namespace log_system {

    // ---------------------------------------------------------
    // Logger 单例实现
    // ---------------------------------------------------------
    Logger& Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    Logger::~Logger() {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
    }

    void Logger::init(const std::string& filepath, LogLevel minLevel) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = minLevel;

        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }

        // 以追加模式打开日志文件
        if (!filepath.empty()) {
            m_fileStream.open(filepath, std::ios::app);
            if (!m_fileStream.is_open()) {
                std::cerr << "Failed to open log file: " << filepath << std::endl;
            }
        }
    }

    void Logger::setConsoleOutput(bool enable) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_consoleOutput = enable;
    }

    void Logger::setLogLevel(LogLevel level) {
        m_minLevel = level; // atomic-like enough for basic types, but strictly can be locked
    }

    LogLevel Logger::getLogLevel() const {
        return m_minLevel;
    }

    void Logger::write(LogLevel level, const std::string& formattedMsg) {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 输出到控制台 (带颜色)
        if (m_consoleOutput) {
            // 重置颜色的 ANSI 码是 \033[0m
            std::cout << formattedMsg << "\033[0m\n";
        }

        // 输出到文件 (去除 ANSI 颜色码，避免文件乱码)
        if (m_fileStream.is_open()) {
            // 查找颜色码结束的位置
            size_t colorEndPos = formattedMsg.find('m');
            if (colorEndPos != std::string::npos && colorEndPos < 10) {
                m_fileStream << formattedMsg.substr(colorEndPos + 1) << "\n";
            }
            else {
                m_fileStream << formattedMsg << "\n";
            }
            m_fileStream.flush();
        }
    }

    // ---------------------------------------------------------
    // LogMessage 辅助类实现
    // ---------------------------------------------------------
    LogMessage::LogMessage(LogLevel level, const char* file, int line, const char* func)
        : m_level(level)
    {
        std::ostringstream prefixStream;

        // 构造日志头部信息
        prefixStream << getColorCode(level)
            << "[" << getTimestamp() << "] "
            << "[" << getLevelString(level) << "] "
            << "[" << getFileName(file) << ":" << line << "] "
            << "[" << func << "] "
            << "[thread:" << std::this_thread::get_id() << "] ";

        m_prefix = prefixStream.str();
    }

    LogMessage::~LogMessage() {
        // 析构时，整条日志已收集完毕，一次性发送给单例写入
        std::string fullMsg = m_prefix + m_stream.str();
        Logger::getInstance().write(m_level, fullMsg);

        // 如果是 FATAL 级别，可以选择直接退出程序
        if (m_level == LogLevel::FATAL) {
            std::abort();
        }
    }

    const char* LogMessage::getFileName(const char* path) {
        const char* file = strrchr(path, '/');
        if (!file) {
            file = strrchr(path, '\\'); // Windows 路径支持
        }
        return file ? (file + 1) : path;
    }

    std::string LogMessage::getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;

#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &now_c); // Windows 安全版本
#else
        localtime_r(&now_c, &tm_buf); // Linux/POSIX 安全版本
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    const char* LogMessage::getLevelString(LogLevel level) {
        switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKWN";
        }
    }

    const char* LogMessage::getColorCode(LogLevel level) {
        // 使用标准 ANSI 转义序列实现终端色彩
        switch (level) {
        case LogLevel::DEBUG: return "\033[36m"; // 青色
        case LogLevel::INFO:  return "\033[32m"; // 绿色
        case LogLevel::WARN:  return "\033[33m"; // 黄色
        case LogLevel::ERR: return "\033[31m"; // 红色
        case LogLevel::FATAL: return "\033[1;31m"; // 亮红色
        default:              return "\033[0m";  // 默认重置
        }
    }

} // namespace log_system