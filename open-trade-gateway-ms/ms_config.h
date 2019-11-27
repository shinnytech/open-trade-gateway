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

struct SlaveNodeInfoBids
{
	SlaveNodeInfoBids();

	std::string name;
	
	std::vector<std::string> bidList;
};

//bid和slavenode的name之间的映射关系
typedef std::map<std::string,std::vector<std::string>> TBidSlaveNodeMap;

struct MasterConfig
{
	MasterConfig();
		
	std::map<std::string, BrokerConfig> brokers;

	std::string broker_list_str;

	//服务IP及端口号
	std::string host;

	int port;

	//当前交易日
	std::string trading_day;
	
	TSlaveNodeInfoList slaveNodeList;	

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

	void DefineStruct(SlaveNodeInfoBids& s);

	void DefineStruct(MasterConfig& c);	

	void DefineStruct(RtnBrokersMsg& b);	
};

bool LoadBrokerList();

bool LoadMasterConfig();