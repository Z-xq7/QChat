#pragma once
#include "const.h"

//配置管理类，负责读取配置文件，提供接口获取配置信息
struct SectionInfo
{
	SectionInfo(){}
	~SectionInfo()
	{
		_section_datas.clear();
	}
	SectionInfo(const SectionInfo& other)
	{
		_section_datas = other._section_datas;
	}
	SectionInfo& operator=(const SectionInfo& other)
	{
		if (this != &other)
		{
			this->_section_datas = other._section_datas;
		}
		return *this;
	}
	std::string operator[](const std::string&key)
	{
		if (_section_datas.find(key) == _section_datas.end())
		{
			return "";
		}
		return _section_datas[key];
	}

	std::map<std::string, std::string> _section_datas;
};


class ConfigMgr
{
public:
	//析构
	~ConfigMgr();
	//[]运算符重载
	SectionInfo operator[](const std::string& section);
	//拷贝构造
	ConfigMgr(const ConfigMgr& other) = delete;
	//赋值运算符重载
	ConfigMgr& operator=(const ConfigMgr& other) = delete;
	//获取单例实例
	static ConfigMgr& Instance();

private:
	//构造
	ConfigMgr();

	std::map<std::string, SectionInfo> _config_map;
};

