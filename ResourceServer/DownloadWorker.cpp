#include "DownloadWorker.h"
#include "CSession.h"
#include "Logger.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "base64.h"
#include "ConfigMgr.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"

DownloadWorker::DownloadWorker() :_b_stop(false)
{
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

			auto task = _task_que.front();
			_task_que.pop();
			task_callback(task);
		}
		});
}

DownloadWorker::~DownloadWorker()
{
	_b_stop = true;
	_cv.notify_one();
	_work_thread.join();
}

void DownloadWorker::PostTask(std::shared_ptr<DownloadTask> task)
{
	{
		std::lock_guard<std::mutex> lock(_mtx);
		_task_que.push(task);
	}

	_cv.notify_one();
}

void DownloadWorker::task_callback(std::shared_ptr<DownloadTask> task)
{
	// 处理
	auto file_path_str = task->_file_path;

	boost::filesystem::path file_path(file_path_str);

	Json::Value result;
	result["error"] = ErrorCodes::Success;

	if (!boost::filesystem::exists(file_path)) {
		LOG_ERROR("File not exists: " << file_path_str);
		result["error"] = ErrorCodes::FileNotExists;
		task->_callback(result);
		return;
	}

	std::ifstream infile(file_path_str, std::ios::binary);
	if (!infile) {
		LOG_ERROR("Cannot open file for reading: " << file_path_str);
		result["error"] = ErrorCodes::FileReadPermissionFailed;
		task->_callback(result);
		return;
	}

	std::shared_ptr<FileInfo> file_info = nullptr;

	if (task->_seq == 1) {
		// 获取文件大小
		infile.seekg(0, std::ios::end);
		std::streamsize file_size = infile.tellg();
		infile.seekg(0, std::ios::beg);
		//如果为空，则创建FileInfo 对象存储
		file_info = std::make_shared<FileInfo>();
		file_info->_file_path_str = file_path_str;
		file_info->_name = task->_name;
		file_info->_seq = 1;

		file_info->_total_size = file_size;
		file_info->_trans_size = 0;
		// 将信息保存到 Redis（这是经常数据，用过即销毁）
		RedisMgr::GetInstance()->SetDownLoadInfo(task->_name, file_info);
		LOG_INFO("Download started - File: " << task->_name << ", Size: " << file_size << " bytes");
	}
	else {
		//续传则尝试从 Redis 获取历史信息
		file_info = RedisMgr::GetInstance()->GetDownloadInfo(task->_name);
		if (file_info == nullptr) {
			// Redis 里没有信息，可能过期了
			LOG_ERROR("Download failed - Redis info not found: " << task->_name);
			result["error"] = ErrorCodes::RedisReadErr;
			task->_callback(result);
			infile.close();
			return;
		}

		// 验证序列号是否匹配
		if (task->_seq != file_info->_seq) {
			LOG_ERROR("Sequence mismatch - Expected: " << file_info->_seq << ", Actual: " << task->_seq);
			result["error"] = ErrorCodes::FileSeqInvalid;
			task->_callback(result);
			infile.close();
			return;
		}

		LOG_DEBUG("Download continue - File: " << task->_name << ", Seq: " << task->_seq
			<< ", Progress: " << file_info->_trans_size << "/" << file_info->_total_size);
	}

	// 计算当前偏移量
	std::streamsize offset = ((std::streamsize)task->_seq - 1) * MAX_FILE_LEN;
	if (offset >= file_info->_total_size) {
		LOG_ERROR("Offset exceeds file size");
		result["error"] = ErrorCodes::FileOffsetInvalid;
		task->_callback(result);
		infile.close();
		return;
	}

	// 定位到指定偏移量
	infile.seekg(offset);

	// 读取最多2048字节
	//char buffer[MAX_FILE_LEN];
	std::vector<char> buffer(MAX_FILE_LEN);

	infile.read(buffer.data(), MAX_FILE_LEN);
	//让read实际读取了多少字节
	std::streamsize bytes_read = infile.gcount();

	if (bytes_read <= 0) {
		LOG_ERROR("Read file failed");
		result["error"] = ErrorCodes::FileReadFailed;
		task->_callback(result);
		infile.close();
		return;
	}

	// 将读取的数据进行base64编码
	std::string data_to_encode(buffer.data(), bytes_read);
	std::string encoded_data = base64_encode(data_to_encode);

	// 判断是否是最后一包
	std::streamsize current_pos = offset + bytes_read;
	bool is_last = (current_pos >= file_info->_total_size);

	// 设置返回结果
	result["data"] = encoded_data;
	result["seq"] = task->_seq;
	result["total_size"] = std::to_string(file_info->_total_size);
	result["current_size"] = std::to_string(current_pos);
	result["is_last"] = is_last;

	infile.close();

	if (is_last) {
		LOG_INFO("File download completed: " << file_path_str);
		RedisMgr::GetInstance()->DelDownLoadInfo(task->_name);
	}
	else {
		//更新信息
		file_info->_seq++;
		file_info->_trans_size = offset + bytes_read;
		//写入redis
		RedisMgr::GetInstance()->SetDownLoadInfo(task->_name, file_info);
	}

	if (task->_callback) {
		task->_callback(result);
	}
}