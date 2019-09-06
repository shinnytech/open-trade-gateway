/////////////////////////////////////////////////////////////////////////
///@file ms_config.h
///@brief	配置文件读写
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"
#include "config.h"

#include <string>
#include <vector>
#include <map>

struct SlaveNodeInfo
{
	SlaveNodeInfo();

	std::string name;

	std::string host;

	std::string port;

	std::string path;

	std::vector<std::string> userList;
};

typedef std::vector<SlaveNodeInfo> TSlaveNodeInfoList;

typedef std::map<std::string,SlaveNodeInfo> TUserSlaveNodeMap;

struct MasterConfig
{
	MasterConfig();

	//服务IP及端口号
	std::string host;

	int port;
	
	TSlaveNodeInfoList slaveNodeList;

	std::map<std::string, BrokerConfig> brokers;

	std::string broker_list_str;

	TUserSlaveNodeMap users_slave_node_map;
};

struct RtnBrokersMsg
{
	RtnBrokersMsg();

	std::string aid;

	std::vector<std::string> brokers;
};

extern MasterConfig g_masterConfig;

bool LoadBrokerList();

bool LoadMasterConfig();

class MasterSerializerConfig
	: public RapidSerialize::Serializer<MasterSerializerConfig>
{
public:
	using RapidSerialize::Serializer<MasterSerializerConfig>::Serializer;

	void DefineStruct(MasterConfig& c);

	void DefineStruct(SlaveNodeInfo& s);

	void DefineStruct(RtnBrokersMsg& b);

	void DefineStruct(BrokerConfig& d);
};