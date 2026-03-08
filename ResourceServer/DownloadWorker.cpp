#include "DownloadWorker.h"
#include "CSession.h"
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
	// 解码
	auto file_path_str = task->_file_path;

	//std::cout << "file_path_str is " << file_path_str << std::endl;

	boost::filesystem::path file_path(file_path_str);

	// 获取完整文件名（包含扩展名）
	std::string filename = file_path.filename().string();
	Json::Value result;
	result["error"] = ErrorCodes::Success;

	if (!boost::filesystem::exists(file_path)) {
		std::cerr << "文件不存在: " << file_path_str << std::endl;
		result["error"] = ErrorCodes::FileNotExists;
		task->_callback(result);
		return;
	}

	std::ifstream infile(file_path_str, std::ios::binary);
	if (!infile) {
		std::cerr << "无法打开文件进行读取。" << std::endl;
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
		//如果为空，则创建FileInfo 构造数据存储
		file_info = std::make_shared<FileInfo>();
		file_info->_file_path_str = file_path_str;
		file_info->_name = filename;
		file_info->_seq = 1;

		file_info->_total_size = file_size;
		file_info->_trans_size = 0;
		// 立即保存到 Redis，覆盖旧数据，设置过期时间
		RedisMgr::GetInstance()->SetDownLoadInfo(filename, file_info);
		std::cout << "[新下载] 文件: " << filename
			<< ", 大小: " << file_size << " 字节" << std::endl;
	}
	else {
		//断点续传，从 Redis 获取历史信息
		file_info = RedisMgr::GetInstance()->GetDownloadInfo(filename);
		if (file_info == nullptr) {
			// Redis 中没有信息（可能过期了）
			std::cerr << "断点续传失败，Redis 中无下载信息: " << filename << std::endl;
			result["error"] = ErrorCodes::RedisReadErr;
			task->_callback(result);
			infile.close();
			return;
		}

		// 验证序列号是否匹配
		if (task->_seq != file_info->_seq) {
			std::cerr << "序列号不匹配，期望: " << file_info->_seq
				<< ", 实际: " << task->_seq << std::endl;
			result["error"] = ErrorCodes::FileSeqInvalid;
			task->_callback(result);
			infile.close();
			return;
		}

		std::cout << "[续传] 文件: " << filename
			<< ", seq: " << task->_seq
			<< ", 进度: " << file_info->_trans_size
			<< "/" << file_info->_total_size << std::endl;
	}

	// 计算当前偏移量
	std::streamsize offset = ((std::streamsize)task->_seq - 1) * MAX_FILE_LEN;
	if (offset >= file_info->_total_size) {
		std::cerr << "偏移量超出文件大小。" << std::endl;
		result["error"] = ErrorCodes::FileOffsetInvalid;
		task->_callback(result);
		infile.close();
		return;
	}

	// 定位到指定偏移量
	infile.seekg(offset);

	// 读取最多2048字节
	char buffer[MAX_FILE_LEN];
	infile.read(buffer, MAX_FILE_LEN);
	//获取read实际读取多少字节
	std::streamsize bytes_read = infile.gcount();

	if (bytes_read <= 0) {
		std::cerr << "读取文件失败。" << std::endl;
		result["error"] = ErrorCodes::FileReadFailed;
		task->_callback(result);
		infile.close();
		return;
	}

	// 将读取的数据进行base64编码
	std::string data_to_encode(buffer, bytes_read);
	std::string encoded_data = base64_encode(data_to_encode);

	// 检查是否是最后一个包
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
		std::cout << "文件读取完成: " << file_path_str << std::endl;
		RedisMgr::GetInstance()->DelDownLoadInfo(filename);
	}
	else {
		//更新信息
		file_info->_seq++;
		file_info->_trans_size = offset + bytes_read;
		//更新redis
		RedisMgr::GetInstance()->SetDownLoadInfo(filename, file_info);
	}

	if (task->_callback) {
		task->_callback(result);
	}
}