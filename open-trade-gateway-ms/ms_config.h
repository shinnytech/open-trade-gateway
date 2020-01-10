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

	std::vector<std::string> bids;
};

typedef std::vector<SlaveNodeInfo> TSlaveNodeInfoList;

//user和slavenode之间的映射关系
typedef std::map<std::string, SlaveNodeInfo> TUserSlaveNodeMap;

struct SlaveNodeUserInfo
{
	SlaveNodeUserInfo();

	std::string name;
	
	std::vector<std::string> users;
};

typedef std::vector<SlaveNodeUserInfo> TSlaveNodeUserInfoList;

struct TMsUsersConfig
{
	TMsUsersConfig();

	std::string trading_day;

	TSlaveNodeUserInfoList slaveNodeUserInfoList;
};

//bid和slavenode的name之间的映射关系
typedef std::map<std::string, std::vector<std::string>> TBidSlaveNodeMap;

struct MasterConfig
{
	MasterConfig();

	std::map<std::string, BrokerConfig> brokers;

	std::string broker_list_str;

	//服务IP及端口号
	std::string host;

	int port;
	
	TSlaveNodeInfoList slaveNodeList;	

	TMsUsersConfig usersConfig;

	TUserSlaveNodeMap users_slave_node_map;

	TBidSlaveNodeMap bids_slave_node_map;
};

extern MasterConfig g_masterConfig;

struct RtnBrokersMsg
{
	RtnBrokersMsg();

	std::string aid;

	std::vector<std::string> brokers;
};

class MasterSerializerConfig
	: public RapidSerialize::Serializer<MasterSerializerConfig>
{
public:
	using RapidSerialize::Serializer<MasterSerializerConfig>::Serializer;

	void DefineStruct(BrokerConfig& d);

	void DefineStruct(SlaveNodeInfo& s);

	void DefineStruct(SlaveNodeUserInfo& d);

	void DefineStruct(TMsUsersConfig& d);
	
	void DefineStruct(MasterConfig& c);

	void DefineStruct(RtnBrokersMsg& b);
};

bool LoadBrokerList();

bool LoadMasterConfig();