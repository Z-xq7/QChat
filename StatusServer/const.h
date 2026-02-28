#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <memory>
#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <hiredis.h>

#include "Singleton.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#define USERIPPREFIX  "uip_"
#define USERTOKENPREFIX  "utoken_"
#define IPCOUNTPREFIX  "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT  "logincount"
#define LOCK_COUNT "lockcount"

//分布式锁的持有时间
#define LOCK_TIME_OUT 10
//分布式锁的重试时间
#define ACQUIRE_TIME_OUT 5

enum ErrorCodes
{
	Success = 0,
	Error_Json = 1001, // JSON解析错误
	RPCFailed = 1002,   // RPC请求失败
	VarifyExpired = 1003, // 验证码过期
	VarifyCodeErr = 1004, // 验证码错误
	UserExist = 1005,     // 用户已存在
	PasswordErr = 1006,   // 密码错误
	EmailNotMatch = 1007,   // 邮箱不匹配
	PasswdUpFailed = 1008,   // 密码更新失败
	PasswdInvalid = 1009,   // 密码不合法
	UidInvalid = 1011,   // 用户ID无效
	TokenInvalid = 1012,   // Token无效
};

// 颜色枚举（方便调用）
enum ConsoleColor {
    BLACK = 30,
    RED = 31,
    GREEN = 32,
    YELLOW = 33,
    BLUE = 34,
    RESET = 0 // 重置
};

// 设置输出颜色的函数
 static void SetColor(ConsoleColor color) {
	std::cout << "\033[" << static_cast<int>(color) << "m";
}

 //Defer类实现类似Go语言中的defer功能，在对象生命周期结束时执行指定的函数
class Defer
 {
 public:
	 //构造函数接受一个函数对象，并在析构函数中调用它
	Defer(std::function<void()> func) : func_(func) {}
	~Defer() { func_(); }
 private:
	std::function<void()> func_;
};
