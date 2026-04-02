#pragma once
#include <string>
#include "const.h"
#include <iostream>
#include <boost/asio.hpp>
#include <vector>
using namespace std;
using boost::asio::ip::tcp;
class LogicSystem;
class MsgNode
{
public:
	MsgNode(std::size_t max_len)
		: _data(max_len + 1, 0), _cur_len(0) {}

	~MsgNode() {
		std::cout << "destruct MsgNode" << endl;
	}

	void Clear() {
		std::fill(_data.begin(), _data.end(), 0);
		_cur_len = 0;
	}

	std::size_t _cur_len;
	std::vector<char> _data;

	std::size_t total_len() const { return _data.empty() ? 0 : _data.size() - 1; }
};

class RecvNode :public MsgNode {
	friend class LogicSystem;
public:
	RecvNode(std::size_t max_len, short msg_id);
	short _msg_id;
};

class SendNode:public MsgNode {
	friend class LogicSystem;
public:
	SendNode(const char* msg,std::size_t max_len, short msg_id);
	short _msg_id;
};

