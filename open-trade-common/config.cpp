/////////////////////////////////////////////////////////////////////////
///@file config.cpp
///@brief	配置文件读写
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "log.h"
#include "utility.h"
#include "SerializerTradeBase.h"
#include "rapid_serialize.h"

#include <iostream>
#include <boost/filesystem.hpp>

using namespace std;

Config g_config;

class SerializerConfig
    : public RapidSerialize::Serializer<SerializerConfig>
{
public:
    using RapidSerialize::Serializer<SerializerConfig>::Serializer;

    void DefineStruct(Config& d)
    {
        AddItem(d.host, "host");
        AddItem(d.port, "port");
        AddItem(d.user_file_path, "user_file_path");
        AddItem(d.auto_confirm_settlement, "auto_confirm_settlement");
    }

    void DefineStruct(BrokerConfig& d)
    {
        AddItem(d.broker_name, "name");
        AddItem(d.broker_type, "type");
		AddItem(d.is_fens, "is_fens");
        AddItem(d.ctp_broker_id, "broker_id");
        AddItem(d.trading_fronts, "trading_fronts");
        AddItem(d.product_info, "product_info");
        AddItem(d.auth_code, "auth_code");
    }	
};

bool LoadConfig()
{
    g_config.trading_day = GuessTradingDay();

	Log(LOG_INFO,"msg=LoadConfig;trading_day=%s", g_config.trading_day.c_str());

    SerializerConfig ss;
    if (!ss.FromFile("/etc/open-trade-gateway/config.json"))
    {
        Log2(LOG_FATAL,"load /etc/open-trade-gateway/config.json file fail");
        return false;
    }
    ss.ToVar(g_config);
		
	std::map<std::string, BrokerConfig> brokerConfigMap;
	std::string strFileName = "/etc/open-trade-gateway/broker_list.json";
	
	if (boost::filesystem::exists(strFileName))
	{
		SerializerConfig ss_broker;
		if (!ss_broker.FromFile(strFileName.c_str()))
		{
			Log2(LOG_WARNING,"load %s file fail",strFileName.c_str());
		}
		else
		{
			std::vector<BrokerConfig> broker_list;
			ss_broker.ToVar(broker_list);
			for (auto b : broker_list)
			{
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
			brokerFilePath=boost::filesystem::system_complete(brokerFilePath);

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
		SerializerConfig ss_broker;
		if (!ss_broker.FromFile(fileName.c_str()))
		{
			Log2(LOG_WARNING, "load %s file fail", fileName.c_str());
			continue;
		}

		BrokerConfig bc;
		ss_broker.ToVar(bc);

		brokerConfigMap[bc.broker_name] = bc;
	}
   
	if (brokerConfigMap.empty())
	{
		Log2(LOG_FATAL,"load broker list fail!");
		return false;
	}

	g_config.brokers = brokerConfigMap;	
    boost::filesystem::path ufpath(g_config.user_file_path);
	std::vector<std::string> brokerList;
	for (auto it : g_config.brokers)
	{
		BrokerConfig& broker = it.second;
		if (!boost::filesystem::exists(ufpath/broker.broker_name))
		{
			boost::filesystem::create_directory(ufpath/broker.broker_name);
		}
		brokerList.push_back(broker.broker_name);
	}

	std::sort(brokerList.begin(),brokerList.end());
	SerializerTradeBase ss_broker_list_str;
	rapidjson::Pointer("/aid").Set(*ss_broker_list_str.m_doc,"rtn_brokers");
	for (int i = 0; i < brokerList.size();i++)
	{
		rapidjson::Pointer("/brokers/" + std::to_string(i)).Set(*ss_broker_list_str.m_doc,brokerList[i]);
	}
    ss_broker_list_str.ToString(&g_config.broker_list_str);

    return true;
}

Config::Config()
{
    //配置参数默认值
    port = 7788;
    auto_confirm_settlement = false;
}
