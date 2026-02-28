#pragma once
#include "const.h"
#include <boost/asio/io_context.hpp>

//AsioIOServicePool类，负责管理多个boost::asio::io_context对象，提供接口获取io_context对象，停止服务
class AsioIOServicePool:public Singleton<AsioIOServicePool>
{
	friend Singleton<AsioIOServicePool>;
public:
	using IOService = boost::asio::io_context;
	//Work类是boost::asio库中的一个工具类，用于保持io_context对象的运行状态，防止io_context.run()函数返回
	using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
	using WorkPtr = std::unique_ptr<Work>;

	~AsioIOServicePool();
	AsioIOServicePool(const AsioIOServicePool&) = delete;
	AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

	// 使用 round-robin 的方式返回一个 io_service
	boost::asio::io_context& GetIOService();
	void Stop();

private:
	//构造函数，默认创建与CPU核心数一半相等的io_context对象
	AsioIOServicePool(std::size_t size = (std::thread::hardware_concurrency() / 2));

	std::vector<IOService> _ioServices;
	std::vector<WorkPtr> _works;
	std::vector<std::thread> _threads;
	std::size_t _nextIOService;
};

