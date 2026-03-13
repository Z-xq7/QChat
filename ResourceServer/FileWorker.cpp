#include "FileWorker.h"
#include "CSession.h"
#include "Logger.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "base64.h"
#include "ConfigMgr.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "ChatServerGrpcClient.h"

FileWorker::FileWorker() :_b_stop(false)
{
	RegisterHandlers();

	_work_thread = std::thread([this]() {
		while (!_b_stop) {
			std::unique_lock<std::mutex> lock(_mtx);
			_cv.wait(lock, [this]() {
				if (_b_stop) {
					return true;
				}

				if (_task_que.empty()) {
					return false;
				}
				return true;
			});

			if (_b_stop) {
				break;
			}

			auto task_call = _task_que.front();
			_task_que.pop();
			task_call();
		}
	});
}

FileWorker::~FileWorker()
{
	_b_stop = true;
	_cv.notify_one();
	_work_thread.join();
}

void FileWorker::PostTask(std::shared_ptr<FileTask> task)
{
	{
		std::lock_guard<std::mutex> lock(_mtx);
		//借鉴python万物皆对象思想，构造伪闭包将函数对象扔到队列中
		_task_que.push([task, this]()
		{
			task_callback(task);
		});
	}
	_cv.notify_one();
}

void FileWorker::task_callback(std::shared_ptr<FileTask> task)
{
	auto iter = _handlers.find(task->_msg_id);
	if (iter == _handlers.end()) {
		return;
	}

	iter->second(task);
}

void FileWorker::RegisterHandlers()
{
	_handlers[ID_UPLOAD_FILE_REQ] = [this](std::shared_ptr<FileTask> task) {
		// 解码
		std::string decoded = base64_decode(task->_file_data);

		auto file_path_str = task->_path;
		auto last = task->_last;
		//std::cout << "file_path_str is " << file_path_str << std::endl;

		boost::filesystem::path file_path(file_path_str);
		boost::filesystem::path dir_path = file_path.parent_path();
		// 获取完整文件名（包含扩展名）
		std::string filename = file_path.filename().string();
		Json::Value result;
		result["error"] = ErrorCodes::Success;

		// Check if directory exists, if not, create it
		if (!boost::filesystem::exists(dir_path)) {
			if (!boost::filesystem::create_directories(dir_path)) {
				LOG_ERROR("Failed to create directory: " << dir_path.string());
				result["error"] = ErrorCodes::FileNotExists;
				task->_callback(result);
				return;
			}
		}

		std::ofstream outfile;
		//第一个包
		if (task->_seq == 1) {
			// 打开文件，如果存在则清空，不存在则创建
			outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
		}
		else {
			// 保存为文件
			outfile.open(file_path_str, std::ios::binary | std::ios::app);
		}

		if (!outfile) {
			LOG_ERROR("无法打开文件进行写入：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.write(decoded.data(), decoded.size());
		if (!outfile) {
			LOG_ERROR("写入文件失败：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.close();
		if (last) {
			LOG_INFO("文件已成功保存为: " << task->_name);
		}
		if (task->_callback) {
			task->_callback(result);
		}
	};

	//处理头像上传
	_handlers[ID_UPLOAD_HEAD_ICON_REQ] = [this](std::shared_ptr<FileTask> task) {
		// 解码
		std::string decoded = base64_decode(task->_file_data);

		auto file_path_str = task->_path;
		auto last = task->_last;
		//std::cout << "file_path_str is " << file_path_str << std::endl;

		boost::filesystem::path file_path(file_path_str);
		boost::filesystem::path dir_path = file_path.parent_path();
		// 获取完整文件名（包含扩展名）
		std::string filename = file_path.filename().string();
		Json::Value result;
		result["error"] = ErrorCodes::Success;

		// Check if directory exists, if not, create it
		if (!boost::filesystem::exists(dir_path)) {
			if (!boost::filesystem::create_directories(dir_path)) {
				LOG_ERROR("Failed to create directory: " << dir_path.string());
				result["error"] = ErrorCodes::FileNotExists;
				task->_callback(result);
				return;
			}
		}

		std::ofstream outfile;
		//第一个包
		if (task->_seq == 1) {
			// 打开文件，如果存在则清空，不存在则创建
			outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
		}
		else {
			// 保存为文件
			outfile.open(file_path_str, std::ios::binary | std::ios::app);
		}

		if (!outfile) {
			LOG_ERROR("无法打开文件进行写入：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.write(decoded.data(), decoded.size());
		if (!outfile) {
			LOG_ERROR("写入文件失败：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.close();
		if (last) {
			LOG_INFO("文件已成功保存为: " << task->_name);
			//更新头像
			MysqlMgr::GetInstance()->UpdateUserIcon(task->_uid, filename);
			//获取用户信息
			auto user_info = MysqlMgr::GetInstance()->GetUser(task->_uid);
			if (user_info == nullptr) {
				return;
			}

			//将数据库内容写入redis缓存
			Json::Value redis_root;
			redis_root["uid"] = task->_uid;
			redis_root["pwd"] = user_info->pwd;
			redis_root["name"] = user_info->name;
			redis_root["email"] = user_info->email;
			redis_root["nick"] = user_info->nick;
			redis_root["desc"] = user_info->desc;
			redis_root["sex"] = user_info->sex;
			redis_root["icon"] = user_info->icon;
			std::string base_key = USER_BASE_INFO + std::to_string(task->_uid);
			RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
		}

		if (task->_callback) {
			task->_callback(result);
		}
	};

	//处理聊天图片上传
	_handlers[ID_IMG_CHAT_UPLOAD_REQ] = [this](std::shared_ptr<FileTask> task) {
		// 解码
		std::string decoded = base64_decode(task->_file_data);

		auto file_path_str = task->_path;
		auto last = task->_last;
		//std::cout << "file_path_str is " << file_path_str << std::endl;

		boost::filesystem::path file_path(file_path_str);
		boost::filesystem::path dir_path = file_path.parent_path();
		// 获取完整文件名（包含扩展名）
		std::string filename = file_path.filename().string();
		Json::Value result;
		result["error"] = ErrorCodes::Success;

		// Check if directory exists, if not, create it
		if (!boost::filesystem::exists(dir_path)) {
			if (!boost::filesystem::create_directories(dir_path)) {
				LOG_ERROR("Failed to create directory: " << dir_path.string());
				result["error"] = ErrorCodes::FileNotExists;
				task->_callback(result);
				return;
			}
		}

		std::ofstream outfile;
		//第一个包
		if (task->_seq == 1) {
			// 打开文件，如果存在则清空，不存在则创建
			outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
		}
		else {
			// 保存为文件
			outfile.open(file_path_str, std::ios::binary | std::ios::app);
		}

		if (!outfile) {
			LOG_ERROR("无法打开文件进行写入：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.write(decoded.data(), decoded.size());
		if (!outfile) {
			LOG_ERROR("写入文件失败：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.close();
		if (last) {
			LOG_INFO("文件已成功保存为: " << task->_name);
			//更新数据库聊天图像上传状态
			MysqlMgr::GetInstance()->UpdateUploadStatus(task->_chat_msg_id);

			std::string uid_ip_value = "";
			auto receiver_str = std::to_string(task->_receiver);
			auto uid_ip_key = USERIPPREFIX + receiver_str;
			bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
			//如果接收者未登录，则直接返回
			if (!b_ip) {
				if (task->_callback) {
					task->_callback(result);
				}

				return;
			}

			if (task->_callback) {
				task->_callback(result);
			}

			//通过grpc通知ChatServer
			ChatServerGrpcClient::GetInstance()->NotifyChatImgMsg(task->_chat_msg_id, uid_ip_value);
			return;
		}

		if (task->_callback) {
			task->_callback(result);
		}
	};

	//处理文件信息同步请求
	_handlers[ID_FILE_INFO_SYNC_REQ] = [this](std::shared_ptr<FileTask> task) {
		// 解码
		std::string decoded = base64_decode(task->_file_data);

		auto file_path_str = task->_path;
		auto last = task->_last;
		//std::cout << "file_path_str is " << file_path_str << std::endl;

		boost::filesystem::path file_path(file_path_str);
		boost::filesystem::path dir_path = file_path.parent_path();
		// 获取完整文件名（包含扩展名）
		std::string filename = file_path.filename().string();
		Json::Value result;
		result["error"] = ErrorCodes::Success;

		// Check if directory exists, if not, create it
		if (!boost::filesystem::exists(dir_path)) {
			if (!boost::filesystem::create_directories(dir_path)) {
				LOG_ERROR("Failed to create directory: " << dir_path.string());
				result["error"] = ErrorCodes::FileNotExists;
				task->_callback(result);
				return;
			}
		}

		std::ofstream outfile;
		//第一个包
		if (task->_seq == 1) {
			// 打开文件，如果存在则清空，不存在则创建
			outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
		}
		else {
			// 保存为文件
			outfile.open(file_path_str, std::ios::binary | std::ios::app);
		}

		if (!outfile) {
			LOG_ERROR("无法打开文件进行写入：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.write(decoded.data(), decoded.size());
		if (!outfile) {
			LOG_ERROR("写入文件失败：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.close();
		if (last) {
			LOG_INFO("文件已成功保存为: " << task->_name);
			//更新数据库聊天图像上传状态
			MysqlMgr::GetInstance()->UpdateUploadStatus(task->_chat_msg_id);
			std::string uid_ip_value = "";
			auto receiver_str = std::to_string(task->_receiver);
			auto uid_ip_key = USERIPPREFIX + receiver_str;
			bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
			//如果接收者未登录，则直接返回
			if (!b_ip) {
				if (task->_callback) {
					task->_callback(result);
				}

				return;
			}

			if (task->_callback) {
				task->_callback(result);
			}

			//通过grpc通知ChatServer
			ChatServerGrpcClient::GetInstance()->NotifyChatImgMsg(task->_chat_msg_id, uid_ip_value);
			return;
		}

		if (task->_callback) {
			task->_callback(result);
		}
		};

	//处理续传图片请求
	_handlers[ID_IMG_CHAT_CONTINUE_UPLOAD_REQ] = [this](std::shared_ptr<FileTask> task) {
		// 解码
		std::string decoded = base64_decode(task->_file_data);

		auto file_path_str = task->_path;
		auto last = task->_last;
		//std::cout << "file_path_str is " << file_path_str << std::endl;

		boost::filesystem::path file_path(file_path_str);
		boost::filesystem::path dir_path = file_path.parent_path();
		// 获取完整文件名（包含扩展名）
		std::string filename = file_path.filename().string();
		Json::Value result;
		result["error"] = ErrorCodes::Success;

		// Check if directory exists, if not, create it
		if (!boost::filesystem::exists(dir_path)) {
			if (!boost::filesystem::create_directories(dir_path)) {
				LOG_ERROR("Failed to create directory: " << dir_path.string());
				result["error"] = ErrorCodes::FileNotExists;
				task->_callback(result);
				return;
			}
		}

		std::ofstream outfile;
		//第一个包
		if (task->_seq == 1) {
			// 打开文件，如果存在则清空，不存在则创建
			outfile.open(file_path_str, std::ios::binary | std::ios::trunc);
		}
		else {
			// 保存为文件
			outfile.open(file_path_str, std::ios::binary | std::ios::app);
		}

		if (!outfile) {
			LOG_ERROR("无法打开文件进行写入：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.write(decoded.data(), decoded.size());
		if (!outfile) {
			LOG_ERROR("写入文件失败：" << file_path_str);
			result["error"] = ErrorCodes::FileWritePermissionFailed;
			task->_callback(result);
			return;
		}

		outfile.close();
		if (last) {
			LOG_INFO("文件已成功保存为: " << task->_name);
			//更新数据库聊天图像上传状态
			MysqlMgr::GetInstance()->UpdateUploadStatus(task->_chat_msg_id);

			std::string uid_ip_value = "";
			auto receiver_str = std::to_string(task->_receiver);
			auto uid_ip_key = USERIPPREFIX + receiver_str;
			bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
			//如果接收者未登录，则直接返回
			if (!b_ip) {
				if (task->_callback) {
					task->_callback(result);
				}

				return;
			}

			//通过grpc通知ChatServer
			ChatServerGrpcClient::GetInstance()->NotifyChatImgMsg(task->_chat_msg_id, uid_ip_value);
			if (task->_callback) {
				task->_callback(result);
			}

			return;
		}

		if (task->_callback) {
			task->_callback(result);
		}
	};
}