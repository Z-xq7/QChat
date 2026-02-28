#include "HttpConnection.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc):_socket(ioc)
{
}

//char 转为16进制
unsigned char ToHex(unsigned char x)
{
	return  x > 9 ? x + 55 : x + 48;
}

//16进制转为十进制的char的方法
unsigned char FromHex(unsigned char x)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else assert(0);
	return y;
}

//url编码，处理特殊字符
std::string UrlEncode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//判断是否仅有数字和字母构成
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ') //为空字符
			strTemp += "+";
		else
		{
			//其他字符需要提前加%并且高四位和低四位分别转为16进制
			strTemp += '%';
			strTemp += ToHex((unsigned char)str[i] >> 4);
			strTemp += ToHex((unsigned char)str[i] & 0x0F);
		}
	}
	return strTemp;
}

//url解码，处理特殊字符
std::string UrlDecode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//还原+为空
		if (str[i] == '+') strTemp += ' ';
		//遇到%将后面的两个字符从16进制转为char再拼接
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = FromHex((unsigned char)str[++i]);
			unsigned char low = FromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}

//启动连接，异步读取客户端请求
void HttpConnection::Start()
{
	auto self = shared_from_this();
	//异步读取客户端请求
	http::async_read(_socket, _buffer, _request, [self](beast::error_code ec,std::size_t bytes_transferred)
		{
			try
			{
				if (ec)
				{
					SetColor(RED);
					std::cout << "*** http read err is: " << ec.message() << " ***" << std::endl;
					SetColor(RESET);
					return;
				}
				//http服务器不需要粘包处理，故可以忽略已发送字节数
				boost::ignore_unused(bytes_transferred);
				//打印请求内容
				SetColor(YELLOW);
				std::cout << "--- http request received ---\n" << self->_request << std::endl;
				SetColor(RESET);
				//处理客户端请求
				self->HandleReq();
				//启动超时检测
				self->CheckDeadLine();
			}
			catch (std::exception& e)
			{
				SetColor(RED);
				std::cerr << "*** HttpConnection Start Error: " << e.what() << " ***" << std::endl;
				SetColor(RESET);
			}
		}
	);
}

//定时器检测客户端超时连接（掉线）
void HttpConnection::CheckDeadLine()
{
	auto self = shared_from_this();
	deadline_.async_wait([self](beast::error_code ec)
		{
			//定时器出错
			if (ec)
			{
				SetColor(RED);
				std::cout << "*** http deadline err is: " << ec.message() << " ***" << std::endl;
				SetColor(RESET);
				return;
			}
			//没有出错，关闭客户端连接
			self->_socket.close(ec);
		}
	);
}

//预处理Get请求参数(解析url路径)
void HttpConnection::PreParseGetParam() {
	// 提取 URI  
	auto uri = _request.target();
	// 查找查询字符串的开始位置（即 '?' 的位置）  
	auto query_pos = uri.find('?');
	if (query_pos == std::string::npos) {
		_get_url = uri;
		return;
	}
	//截取不带参数的URL路径
	_get_url = uri.substr(0, query_pos);
	//截取路径参数字符串
	std::string query_string = uri.substr(query_pos + 1);
	std::string key;
	std::string value;
	size_t pos = 0;
	//循环处理每个参数对，直到没有更多的参数对（即没有 '&' 分隔符）
	while ((pos = query_string.find('&')) != std::string::npos) {
		auto pair = query_string.substr(0, pos);
		size_t eq_pos = pair.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码  
			value = UrlDecode(pair.substr(eq_pos + 1));
			_get_params[key] = value;
		}
		query_string.erase(0, pos + 1);
	}
	// 处理最后一个参数对（如果没有 & 分隔符）  
	if (!query_string.empty()) {
		size_t eq_pos = query_string.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(query_string.substr(0, eq_pos));
			value = UrlDecode(query_string.substr(eq_pos + 1));
			_get_params[key] = value;
		}
	}
}

//处理客户端请求（解析请求头、请求体）
void HttpConnection::HandleReq()
{
	//设置版本
	_response.version(_request.version());
	//设置短连接(http)
	_response.keep_alive(false);
	//根据请求方法处理请求
	if (_request.method() == http::verb::get)
	{
		//预处理Get请求参数(解析url路径)
		PreParseGetParam();
		//逻辑系统处理Get请求
		bool success = LogicSystem::GetInstance()->HandleGet(_get_url,shared_from_this());	//.target:获取url
		//处理失败
		if (!success)
		{
			//设置回应状态码（404）
			_response.result(http::status::not_found);
			//设置回应类型
			_response.set(http::field::content_type,"text/plain");
			//回应内容
			beast::ostream(_response.body()) << "404 Not Found\r\n";
			//发送回应
			WriteResponse();
			return;
		}
		//处理成功则在逻辑系统中已经设置好了回应内容，直接发送回应
		//设置回应状态码（200）
		_response.result(http::status::ok);
		//设置回应服务器类型
		_response.set(http::field::server,"GateServer");
		//发送回应
		WriteResponse();
		return;
	}
	else if (_request.method() == http::verb::post)
	{
		//逻辑系统处理Post请求
		bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
		//处理失败
		if (!success)
		{
			//设置回应状态码（404）
			_response.result(http::status::not_found);
			//设置回应类型
			_response.set(http::field::content_type, "text/plain");
			//回应内容
			beast::ostream(_response.body()) << "404 Not Found\r\n";
			//发送回应
			WriteResponse();
			return;
		}
		//处理成功则在逻辑系统中已经设置好了回应内容，直接发送回应
		//设置回应状态码（200）
		_response.result(http::status::ok);
		//设置回应服务器类型
		_response.set(http::field::server, "GateServer");
		//发送回应
		WriteResponse();
		return;
	}
}

//响应客户端（回发数据）
void HttpConnection::WriteResponse()
{
	auto self = shared_from_this();
	//设置回应内容长度
	_response.content_length(_response.body().size());
	SetColor(GREEN);
	std::cout << "--- http response to be sent ---\n" << _response << std::endl;
	SetColor(RESET);
	//异步回应客户端
	http::async_write(_socket,_response,[self](beast::error_code ec, std::size_t bytes_transferred)
		{
			//关闭客户端发送通道
			self->_socket.shutdown(tcp::socket::shutdown_send, ec);
			if (ec)
			{
				SetColor(RED);
				std::cout << "*** http write err is: " << ec.message() << " ***" << std::endl;
				SetColor(RESET);
			}
			//回应完成后取消定时器
			self->deadline_.cancel();
		}
	);
}

