#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"

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
			//出错则放弃连接，继续监听其他连接
			if (ec)
			{
				self->Start();
				return;
			}

			//启动连接，异步读取客户端请求
			new_connection->Start();
			SetColor(GREEN);
			std::cout << "--- new connection accepted ---" << std::endl;
			SetColor(RESET);

			//继续监听
			self->Start();
		}
		catch (std::exception& e)
		{
			SetColor(RED);
			std::cerr << "*** CServer Start Error: " << e.what() << " ***" << std::endl;
			SetColor(RESET);
		}
	});
}
