#pragma once
#include <string>
#include <vector>

struct UserInfo {
	UserInfo() :name(""), pwd(""), uid(0), email(""), nick(""), desc(""), sex(0), icon(""), back("") {}
	std::string name;
	std::string pwd;
	int uid;
	std::string email;
	std::string nick;
	std::string desc;
	int sex;
	std::string icon;
	std::string back;
};

struct ApplyInfo {
	ApplyInfo(int uid, std::string name, std::string desc,
		std::string icon, std::string nick, int sex, int status)
		:_uid(uid),_name(name),_desc(desc),
		_icon(icon),_nick(nick),_sex(sex),_status(status){}

	int _uid;
	std::string _name;
	std::string _desc;
	std::string _icon;
	std::string _nick;
	int _sex;
	int _status;
};

//聊天线程信息
struct ChatThreadInfo
{
	int _thread_id;
	std::string _type;
	int _user1_id;
	int _user2_id;
};

enum class ChatMsgType {
	TEXT = 0,
	PIC = 1,
	VIDEO = 2,
	FILE = 3
};

//聊天消息信息
struct ChatMessage
{
	int message_id;
	int thread_id;
	int sender_id;
	int recv_id;
	std::string unique_id;
	std::string content;
	std::string chat_time;
	int status;
	int msg_type;
};

//聊天消息查询结果结果
struct PageResult
{
	std::vector<ChatMessage> messages;
	int next_cursor;	//本页最后一条消息的id，下一页查询时以此为起点
	bool load_more;
};

