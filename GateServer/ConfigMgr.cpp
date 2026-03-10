#include "ConfigMgr.h"
#include "Logger.h"

ConfigMgr::ConfigMgr()
{
	boost::filesystem::path current_path = boost::filesystem::current_path();
	boost::filesystem::path config_path = current_path / "config.ini";
	LOG_INFO("Config file path: " << config_path.string());
	//以解析的方式读取配置文件
	boost::property_tree::ptree pt;
	boost::property_tree::read_ini(config_path.string(), pt);
	//遍历读取到的配置数据存入_config_map
	for (const auto& section_pair : pt)
	{
		const std::string section_name = section_pair.first;
		const boost::property_tree::ptree& section_tree = section_pair.second;
		std::map<std::string, std::string> section_config;
		for (const auto& key_value_pair : section_tree)
		{
			const std::string& key = key_value_pair.first;
			const std::string& value = key_value_pair.second.get_value<std::string>();
			section_config[key] = value;
		}
		//SectionInfo section_info;
		//section_info._section_datas = section_config;
		//_config_map[section_name] = section_info;
		_config_map[section_name]._section_datas = section_config;
	}
	//打印读取到的配置信息
	for (const auto& section_pair : _config_map)
	{
		const std::string& section_name = section_pair.first;
		const SectionInfo& section_info = section_pair.second;
		LOG_DEBUG("[ Section: " << section_name << " ]");
		for (const auto& key_value_pair : section_info._section_datas)
		{
			LOG_DEBUG(key_value_pair.first << " = " << key_value_pair.second);
		}
	}
}

ConfigMgr::~ConfigMgr()
{
	_config_map.clear();
}

SectionInfo ConfigMgr::operator[](const std::string& section)
{
	if (_config_map.find(section) == _config_map.end())
	{
		return SectionInfo();
	}
	return _config_map[section];
}

//获取单例实例
ConfigMgr& ConfigMgr::Instance()
{
	static ConfigMgr instance;
	return instance;
}