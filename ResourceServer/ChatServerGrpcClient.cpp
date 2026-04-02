#include "ChatServerGrpcClient.h"

#include "Logger.h"
#include "MysqlMgr.h"

NotifyChatImgRsp  ChatServerGrpcClient::NotifyChatImgMsg(int message_id, std::string chatserver)
{
	ClientContext context;
	NotifyChatImgRsp reply;
	NotifyChatImgReq request;

	request.set_message_id(message_id);
	if (_hash_pools.find(chatserver) == _hash_pools.end()) {
		reply.set_error(ErrorCodes::ServerIpErr);
		return reply;
	}

	auto chat_msg = MysqlMgr::GetInstance()->GetChatMsgById(message_id);
	request.set_file_name(chat_msg->content);
	request.set_from_uid(chat_msg->sender_id);
	request.set_to_uid(chat_msg->recv_id);
	request.set_thread_id(chat_msg->thread_id);
	// ��Դ�ļ�·��
	auto file_dir = ConfigMgr::Inst().GetFileOutPath();
	//消息是接收端从发送端获取资源,所以资源存储在发送者的文件夹下
	auto uid_str = std::to_string(chat_msg->sender_id);
	auto file_path = (file_dir / uid_str / chat_msg->content);
	boost::uintmax_t file_size = 0;
	try {
		if (boost::filesystem::exists(file_path)) {
			file_size = boost::filesystem::file_size(file_path);
		}
		else {
			LOG_ERROR("NotifyChatImgMsg Image not found: " << file_path.string());
			reply.set_error(ErrorCodes::FileNotExists);
			return reply;
		}
	}
	catch (const boost::filesystem::filesystem_error& e) {
		LOG_ERROR("NotifyChatImgMsg filesystem error: " << e.what());
		reply.set_error(ErrorCodes::FileReadPermissionFailed);
		return reply;
	}
	request.set_total_size(file_size);

	auto& pool_ = _hash_pools[chatserver];
	auto stub = pool_->getConnection();
	Status status = stub->NotifyChatImgMsg(&context, request, &reply);
	Defer defer([&stub, &pool_, this]() {
		pool_->returnConnection(std::move(stub));
		});

	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

message::NotifyChatFileRsp ChatServerGrpcClient::NotifyChatFileMsg(int message_id, std::string chatserver)
{
	ClientContext context;
	message::NotifyChatFileRsp reply;
	message::NotifyChatFileReq request;

	request.set_message_id(message_id);
	if (_hash_pools.find(chatserver) == _hash_pools.end()) {
		reply.set_error(ErrorCodes::ServerIpErr);
		return reply;
	}

	auto chat_msg = MysqlMgr::GetInstance()->GetChatMsgById(message_id);
	request.set_file_name(chat_msg->content);
	request.set_from_uid(chat_msg->sender_id);
	request.set_to_uid(chat_msg->recv_id);
	request.set_thread_id(chat_msg->thread_id);

	auto file_dir = ConfigMgr::Inst().GetFileOutPath();
	auto uid_str = std::to_string(chat_msg->sender_id);
	
	// 文件内容可能包含 unique_name|origin_name
	std::string unique_name = chat_msg->content;
	size_t pos = unique_name.find('|');
	if (pos != std::string::npos) {
		unique_name = unique_name.substr(0, pos);
	}

	auto file_path = (file_dir / uid_str / unique_name);
	boost::uintmax_t file_size = 0;
	try {
		if (boost::filesystem::exists(file_path)) {
			file_size = boost::filesystem::file_size(file_path);
		}
		else {
			LOG_ERROR("File not found: " << file_path.string());
			reply.set_error(ErrorCodes::FileNotExists);
			return reply;
		}
	}
	catch (const boost::filesystem::filesystem_error& e) {
		LOG_ERROR("NotifyChatFileMsg filesystem error: " << e.what());
		reply.set_error(ErrorCodes::FileReadPermissionFailed);
		return reply;
	}
	request.set_total_size(file_size);

	auto& pool_ = _hash_pools[chatserver];
	auto stub = pool_->getConnection();
	Status status = stub->NotifyChatFileMsg(&context, request, &reply);

	if (stub) {
		pool_->returnConnection(std::move(stub));
	}

	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}

ChatServerGrpcClient::ChatServerGrpcClient()
{
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host1 = gCfgMgr["chatserver1"]["Host"];
	std::string port1 = gCfgMgr["chatserver1"]["Port"];
	_hash_pools["chatserver1"] = std::make_unique<ChatServerConPool>(5, host1, port1);

	std::string host2 = gCfgMgr["chatserver2"]["Host"];
	std::string port2 = gCfgMgr["chatserver2"]["Port"];
	_hash_pools["chatserver2"] = std::make_unique<ChatServerConPool>(5, host2, port2);
}