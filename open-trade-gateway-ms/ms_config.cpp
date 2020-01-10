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

SlaveNodeInfo::SlaveNodeInfo()
	:name("")
	,host("")
	,port("7788")
	,path("/")
	,bids()
{
}

SlaveNodeUserInfo::SlaveNodeUserInfo()
	:name("")	
	,users()
{
}

TMsUsersConfig::TMsUsersConfig()
	:trading_day("")
	,slaveNodeUserInfoList()
{
}

MasterConfig::MasterConfig()
	:brokers()
	,broker_list_str("")
	,host("0.0.0.0")
	,port(5566)		
	,slaveNodeList()
	,usersConfig()
	,users_slave_node_map()
	,bids_slave_node_map()
{
}

MasterConfig g_masterConfig;

RtnBrokersMsg::RtnBrokersMsg()
	:aid("")
	,brokers()
{
}

void MasterSerializerConfig::DefineStruct(BrokerConfig& d)
{
	AddItem(d.broker_name,"name");
	AddItem(d.broker_type,"type");
	AddItem(d.is_fens,"is_fens");
	AddItem(d.ctp_broker_id,"broker_id");
	AddItem(d.trading_fronts,"trading_fronts");
	AddItem(d.product_info,"product_info");
	AddItem(d.auth_code,"auth_code");	
}

void MasterSerializerConfig::DefineStruct(SlaveNodeInfo& s)
{
	AddItem(s.name,"name");
	AddItem(s.host,"host");
	AddItem(s.port,"port");
	AddItem(s.path,"path");
	AddItem(s.bids,"bids");	
}

void MasterSerializerConfig::DefineStruct(SlaveNodeUserInfo& d)
{
	AddItem(d.name, "name");	
	AddItem(d.users, "users");	
}

void MasterSerializerConfig::DefineStruct(MasterConfig& c)
{
	AddItem(c.host, "host");
	AddItem(c.port, "port");	
	AddItem(c.slaveNodeList,"slaveNodeList");
}

void MasterSerializerConfig::DefineStruct(TMsUsersConfig& d)
{
	AddItem(d.trading_day,"trading_day");	
	AddItem(d.slaveNodeUserInfoList,"slaveNodeUserInfoList");
}

void MasterSerializerConfig::DefineStruct(RtnBrokersMsg& b)
{
	AddItem(b.aid, "aid");
	AddItem(b.brokers, "brokers");
}

bool LoadBrokerList()
{
	std::map<std::string, BrokerConfig> brokerConfigMap;
	std::vector< std::string> brokerNameList;
	std::string fn = "/etc/open-trade-gateway/broker_list.json";

	if (boost::filesystem::exists(fn))
	{
		MasterSerializerConfig ss_broker;
		if (!ss_broker.FromFile(fn.c_str()))
		{
			LogMs().WithField("fun","LoadBrokerList")
				.WithField("key","gatewayms")
				.WithField("fileName",fn)
				.Log(LOG_WARNING,"load broker list json file fail");
		}
		else
		{
			std::vector<BrokerConfig> broker_list;
			ss_broker.ToVar(broker_list);
			for (const BrokerConfig& b : broker_list)
			{
				std::map<std::string, BrokerConfig>::iterator it = brokerConfigMap.find(b.broker_name);
				if (it == brokerConfigMap.end())
				{
					brokerNameList.push_back(b.broker_name);
					brokerConfigMap.insert(std::map<std::string,BrokerConfig>::value_type(b.broker_name,b));					
				}				
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

	for (const std::string& fileName : brokerFileList)
	{
		MasterSerializerConfig ss_broker;
		if (!ss_broker.FromFile(fileName.c_str()))
		{
			LogMs().WithField("fun","LoadBrokerList")
				.WithField("key","gatewayms")
				.WithField("fileName",fileName)
				.Log(LOG_WARNING,"load broker json file fail");
			continue;
		}

		BrokerConfig bc;
		ss_broker.ToVar(bc);

		std::map<std::string, BrokerConfig>::iterator it = brokerConfigMap.find(bc.broker_name);
		if (it == brokerConfigMap.end())
		{
			brokerNameList.push_back(bc.broker_name);
			brokerConfigMap.insert(std::map<std::string,BrokerConfig>::value_type(bc.broker_name,bc));
		}

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
	MasterSerializerConfig ss;

	//加载用户配置文件
	std::string fn = "/etc/open-trade-gateway/config-ms.json";
	if (!ss.FromFile(fn.c_str()))
	{
		LogMs().WithField("fun","LoadMasterConfig")
			.WithField("key","gatewayms")
			.WithField("fileName",fn.c_str())
			.Log(LOG_WARNING,"load gatewayms config file config-ms.json fail");
		return false;
	}

	ss.ToVar(g_masterConfig);
	if (g_masterConfig.slaveNodeList.size() == 0)
	{
		LogMs().WithField("fun","LoadMasterConfig")
			.WithField("key","gatewayms")
			.WithField("fileName",fn.c_str())
			.Log(LOG_WARNING,"slaveNodeList is empty");
		return false;
	}

	//生成bid和slavenode的name之间的映射关系
	g_masterConfig.bids_slave_node_map.clear();
	for (auto& a : g_masterConfig.slaveNodeList)
	{
		for (auto& b : a.bids)
		{
			TBidSlaveNodeMap::iterator it = g_masterConfig.bids_slave_node_map.find(b);
			if (it == g_masterConfig.bids_slave_node_map.end())
			{
				std::vector<std::string> nodeList;
				nodeList.push_back(a.name);
				g_masterConfig.bids_slave_node_map.insert(TBidSlaveNodeMap::value_type(b,nodeList));
			}
			else
			{
				std::vector<std::string>& nodeList = it->second;
				bool flag = false;
				for (auto& n : nodeList)
				{
					if (n == a.name)
					{
						flag = true;
						break;
					}
				}
				if (!flag)
				{
					nodeList.push_back(a.name);
				}
			}
		}
	}
	
	//加载ms-users.json配置文件
	g_masterConfig.usersConfig.trading_day = "";
	g_masterConfig.usersConfig.slaveNodeUserInfoList.clear();
	std::string fn_users = "/etc/open-trade-gateway/ms-users.json";
	if (ss.FromFile(fn_users.c_str()))
	{
		ss.ToVar(g_masterConfig.usersConfig);
		//如果是一个新的交易日,需要清除所有游客	
		std::string trading_day = GuessTradingDay();
		bool flag = false;
		if (trading_day != g_masterConfig.usersConfig.trading_day)
		{
			LogMs().WithField("fun","LoadMasterConfig")
				.WithField("key","gatewayms")
				.WithField("trading_day",trading_day)
				.WithField("old_trading_day", g_masterConfig.usersConfig.trading_day)
				.Log(LOG_INFO, "load ms-users.json in a new trading day");

			g_masterConfig.usersConfig.trading_day = trading_day;
			flag = true;

			//清除游客数据
			for (auto & s : g_masterConfig.usersConfig.slaveNodeUserInfoList)
			{
				std::vector<std::string>::iterator it = s.users.begin();
				for (; it != s.users.end();)
				{
					std::string& u = *it;
					if (u.find(u8"sim_快期模拟_游客_", 0) != std::string::npos)
					{
						it = s.users.erase(it);
					}
					else
					{
						it++;
					}
				}
			}
		}

		//保存用户配置文件
		if (flag)
		{
			ss.FromVar(g_masterConfig.usersConfig);
			bool saveFile = ss.ToFile(fn_users.c_str());
			if (!saveFile)
			{
				LogMs().WithField("fun", "LoadMasterConfig")
					.WithField("key", "gatewayms")
					.WithField("fileName", fn_users)
					.Log(LOG_WARNING, "save ms-users.json file failed!");
			}
		}


	}
	//原来不存在ms-users.json配置文件,生成一个空的
	else
	{
		std::string trading_day = GuessTradingDay();
		g_masterConfig.usersConfig.trading_day = trading_day;
		for (auto& a : g_masterConfig.slaveNodeList)
		{
			SlaveNodeUserInfo slaveNodeUserInfo;
			slaveNodeUserInfo.name = a.name;
			slaveNodeUserInfo.users.clear();
			g_masterConfig.usersConfig.slaveNodeUserInfoList.push_back(slaveNodeUserInfo);
		}
	}

	//生成user和和slavenode之间的映射关系
	g_masterConfig.users_slave_node_map.clear();
	for (auto& u : g_masterConfig.usersConfig.slaveNodeUserInfoList)
	{
		std::string nodeName = u.name;
		SlaveNodeInfo tmpSlaveNode;
		for (const SlaveNodeInfo & s : g_masterConfig.slaveNodeList)
		{
			if (s.name == nodeName)
			{
				tmpSlaveNode.name = s.name;
				tmpSlaveNode.host = s.host;
				tmpSlaveNode.path = s.path;
				tmpSlaveNode.port = s.port;
				break;
			}
		}

		if (!tmpSlaveNode.name.empty())
		{
			for (auto & uk : u.users)
			{
				TUserSlaveNodeMap::iterator it = g_masterConfig.users_slave_node_map.find(uk);
				if (it == g_masterConfig.users_slave_node_map.end())
				{
					g_masterConfig.users_slave_node_map.insert(TUserSlaveNodeMap::value_type(
						uk, tmpSlaveNode));
				}
			}
		}

	}
	return true;
}
