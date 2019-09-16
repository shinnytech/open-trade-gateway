/////////////////////////////////////////////////////////////////////////
///@file ms_config.cpp
///@brief	配置文件读写
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "ms_config.h"
#include "log.h"
#include "utility.h"
#include "SerializerTradeBase.h"
#include "rapid_serialize.h"

#include <iostream>
#include <boost/filesystem.hpp>

using namespace std;

MasterConfig g_masterConfig;

SlaveNodeInfo::SlaveNodeInfo()
	:name("")
	,host("")
	,port("7788")
	,path("/")
	,userList()
{
}

MasterConfig::MasterConfig()
	:host("0.0.0.0")
	,port(5566)	
	,trading_day("")
	,slaveNodeList()
	,brokers()
	,broker_list_str()
	,users_slave_node_map()
{	
}

RtnBrokersMsg::RtnBrokersMsg()
	:aid("")
	,brokers()
{
}

bool LoadBrokerList()
{
	std::map<std::string, BrokerConfig> brokerConfigMap;
	std::vector< std::string> brokerNameList;
	std::string strFileName = "/etc/open-trade-gateway/broker_list.json";

	if (boost::filesystem::exists(strFileName))
	{
		MasterSerializerConfig ss_broker;
		if (!ss_broker.FromFile(strFileName.c_str()))
		{
			LogMs().WithField("fun","LoadBrokerList")
				.WithField("key","gatewayms")
				.WithField("fileName",strFileName).
				Log(LOG_WARNING,"load broker list json file fail");
		}
		else
		{
			std::vector<BrokerConfig> broker_list;
			ss_broker.ToVar(broker_list);
			for (auto b : broker_list)
			{
				brokerNameList.push_back(b.broker_name);
				brokerConfigMap[b.broker_name] = b;
			}
		}
	}

	std::vector<std::string> brokerFileList;
	boost::filesystem::path brokerPath = "/etc/open-trade-gateway/broker_list/";
	if (boost::filesystem::exists(brokerPath))
	{
		boost::filesystem::directory_iterator item_begin(brokerPath);
		boost::filesystem::directory_iterator item_end;
		for (; item_begin != item_end; item_begin++)
		{
			if (boost::filesystem::is_directory(*item_begin))
			{
				continue;
			}
			boost::filesystem::path brokerFilePath = item_begin->path();
			brokerFilePath = boost::filesystem::system_complete(brokerFilePath);

			std::string strExt = boost::filesystem::extension(brokerFilePath);

			if (strExt != ".json")
			{
				continue;
			}

			brokerFileList.push_back(brokerFilePath.string());
		}
	}

	for (auto fileName : brokerFileList)
	{
		MasterSerializerConfig ss_broker;
		if (!ss_broker.FromFile(fileName.c_str()))
		{
			LogMs().WithField("fun","LoadBrokerList")
				.WithField("key","gatewayms")
				.WithField("fileName",fileName).
				Log(LOG_WARNING,"load broker json file fail");
			continue;
		}

		BrokerConfig bc;
		ss_broker.ToVar(bc);

		brokerConfigMap[bc.broker_name] = bc;
		brokerNameList.push_back(bc.broker_name);
	}

	if (brokerNameList.empty())
	{
		LogMs().WithField("fun","LoadBrokerList")
			.WithField("key","gatewayms")
			.Log(LOG_FATAL,"broker name list is empty!");
		return false;
	}

	g_masterConfig.brokers = brokerConfigMap;

	SerializerTradeBase ss_broker_list_str;
	rapidjson::Pointer("/aid").Set(*ss_broker_list_str.m_doc,"rtn_brokers");
	for (int i = 0; i < brokerNameList.size(); i++)
	{
		rapidjson::Pointer("/brokers/" + std::to_string(i)).Set(*ss_broker_list_str.m_doc, brokerNameList[i]);
	}
	ss_broker_list_str.ToString(&g_masterConfig.broker_list_str);

	return true;
}

bool LoadMasterConfig()
{
	std::string trading_day=GuessTradingDay();
	std::string fn = "/etc/open-trade-gateway/config-ms.json";
	MasterSerializerConfig ss;
	if (!ss.FromFile(fn.c_str()))
	{
		LogMs().WithField("fun","LoadMasterConfig")
			.WithField("key","gatewayms")
			.WithField("fileName",fn.c_str())
			.Log(LOG_WARNING,"load gatewayms config file fail");
		return false;
	}

	ss.ToVar(g_masterConfig);

	//如果是一个新的交易日,需要清除所有游客	
	bool flag = false;
	if (trading_day != g_masterConfig.trading_day)
	{
		LogMs().WithField("fun","LoadMasterConfig")
			.WithField("key","gatewayms")
			.WithField("trading_day",trading_day)
			.Log(LOG_INFO, "load gatewayms config file in a new trading day");

		g_masterConfig.trading_day = trading_day;
		flag = true;

		for (SlaveNodeInfo & s : g_masterConfig.slaveNodeList)
		{
			std::vector<std::string>::iterator it = s.userList.begin();
			for (; it != s.userList.end();)
			{
				std::string& u = *it;
				if (u.find(u8"sim_快期模拟_游客_",0) != std::string::npos)
				{
					it = s.userList.erase(it);					
				}
				else
				{
					it++;
				}			
			}
		}
	}

	if (flag)
	{
		ss.FromVar(g_masterConfig);
		bool saveFile = ss.ToFile(fn.c_str());
		if (!saveFile)
		{
			LogMs().WithField("fun","LoadMasterConfig")
				.WithField("key","gatewayms")				
				.WithField("fileName",fn)
				.Log(LOG_INFO,"save ms config file failed!");
		}
	}
	
	if (g_masterConfig.slaveNodeList.size() == 0)
	{
		return false;
	}

	g_masterConfig.users_slave_node_map.clear();
	for (const SlaveNodeInfo & s : g_masterConfig.slaveNodeList)
	{
		SlaveNodeInfo tmpSlaveNode;
		tmpSlaveNode.name = s.name;
		tmpSlaveNode.host = s.host;
		tmpSlaveNode.path = s.path;
		tmpSlaveNode.port = s.port;
		tmpSlaveNode.userList.clear();
		for (const std::string& u : s.userList)
		{			
			TUserSlaveNodeMap::iterator it = g_masterConfig.users_slave_node_map.find(u);
			if (it == g_masterConfig.users_slave_node_map.end())
			{
				g_masterConfig.users_slave_node_map.insert(TUserSlaveNodeMap::value_type(
					u,tmpSlaveNode));
			}
		}
	}

	return true;
}

void MasterSerializerConfig::DefineStruct(MasterConfig& c)
{
	AddItem(c.host,"host");
	AddItem(c.port,"port");
	AddItem(c.trading_day,"trading_day");
	AddItem(c.slaveNodeList,"slaveNodeList");
}

void MasterSerializerConfig::DefineStruct(SlaveNodeInfo& s)
{
	AddItem(s.name, "name");
	AddItem(s.host, "host");
	AddItem(s.port, "port");
	AddItem(s.path, "path");
	AddItem(s.userList, "users");
}

void MasterSerializerConfig::DefineStruct(RtnBrokersMsg& b)
{
	AddItem(b.aid, "aid");
	AddItem(b.brokers, "brokers");
}

void MasterSerializerConfig::DefineStruct(BrokerConfig& d)
{
	AddItem(d.broker_name, "name");
	AddItem(d.broker_type, "type");
	AddItem(d.is_fens, "is_fens");
	AddItem(d.ctp_broker_id, "broker_id");
	AddItem(d.trading_fronts, "trading_fronts");
	AddItem(d.product_info, "product_info");
	AddItem(d.auth_code, "auth_code");
}