#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"
#include "Logger.h"

CServer::CServer(boost::asio::io_context& ioc, unsigned short& port):_ioc(ioc),
	_acceptor(ioc,tcp::endpoint(tcp::v4(), port))
{
}

void CServer::Start()
{
	auto self = shared_from_this();
	auto& io_context = AsioIOServicePool::GetInstance()->GetIOService();
	std::shared_ptr<HttpConnection> new_connection = std::make_shared<HttpConnection>(io_context);
	_acceptor.async_accept(new_connection->GetSocket(), [self, new_connection](beast::error_code ec)
	{
		try
		{
			//如果出现连接失败，继续监听下一个连接
			if (ec)
			{
				self->Start();
				return;
			}

			//处理连接，异步读取客户端请求
			new_connection->Start();
			LOG_INFO("new connection accepted");

			//继续监听
			self->Start();
		}
		catch (std::exception& e)
		{
			LOG_ERROR("CServer Start exception: " << e.what());
		}
	});
}
