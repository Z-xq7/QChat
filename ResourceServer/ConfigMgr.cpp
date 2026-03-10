#include "ConfigMgr.h"
#include "Logger.h"

ConfigMgr::ConfigMgr() {
	// 获取当前工作目录  
	boost::filesystem::path current_path = boost::filesystem::current_path();
	// 构建config.ini文件的完整路径  
	boost::filesystem::path config_path = current_path / "config.ini";
	LOG_INFO("Config path: " << config_path.string());

	// 使用Boost.PropertyTree来读取INI文件  
	boost::property_tree::ptree pt;
	boost::property_tree::read_ini(config_path.string(), pt);


	// 遍历INI文件中的每个section  
	for (const auto& section_pair : pt) {
		const std::string& section_name = section_pair.first;
		const boost::property_tree::ptree& section_tree = section_pair.second;

		// 对于每个section，遍历所有的key-value对  
		std::map<std::string, std::string> section_config;
		for (const auto& key_value_pair : section_tree) {
			const std::string& key = key_value_pair.first;
			const std::string& value = key_value_pair.second.get_value<std::string>();
			section_config[key] = value;
		}
		SectionInfo sectionInfo;
		sectionInfo._section_datas = section_config;
		// 将section和key-value对保存到config_map中  
		_config_map[section_name] = sectionInfo;
	}

	// 打印所有的section和key-value对  
	for (const auto& section_entry : _config_map) {
		const std::string& section_name = section_entry.first;
		SectionInfo section_config = section_entry.second;
		LOG_DEBUG("[" << section_name << "]");
		for (const auto& key_value_pair : section_config._section_datas) {
			LOG_DEBUG(key_value_pair.first << "=" << key_value_pair.second);
		}
	}


	InitPath();
}

std::string ConfigMgr::GetValue(const std::string& section, const std::string& key) {
	if (_config_map.find(section) == _config_map.end()) {
		return "";
	}

	return _config_map[section].GetValue(key);
}

boost::filesystem::path ConfigMgr::GetFileOutPath()
{
	return _static_path;
}

void ConfigMgr::InitPath()
{
	// 获取当前工作目录  (./bin/static)
	boost::filesystem::path current_path = boost::filesystem::current_path();
	std::string bindir = _config_map["Output"].GetValue("Path");
	std::string staticdir = _config_map["Static"].GetValue("Path");
	_static_path = current_path / bindir / staticdir;
	_bin_path = current_path / bindir;


	// 检查路径是否存在
	if (!boost::filesystem::exists(_static_path)) {
		// 如果路径不存在，则创建
		if (boost::filesystem::create_directories(_static_path)) {
			LOG_INFO("Directory created successfully: " << _static_path.string());
		}
		else {
			LOG_ERROR("Failed to create directory: " << _static_path.string());
		}
	}
	else {
		LOG_INFO("Directory already exists: " << _static_path.string());
	}
}