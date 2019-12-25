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
#include "condition_order_type.h"
#include "condition_order_serializer.h"

#include <iostream>
#include <boost/filesystem.hpp>

using namespace std;

Config g_config;

condition_order_config g_condition_order_config;

class SerializerConfig
    : public RapidSerialize::Serializer<SerializerConfig>
{
public:
    using RapidSerialize::Serializer<SerializerConfig>::Serializer;

    void DefineStruct(Config& d)
    {
        AddItem(d.host,"host");
        AddItem(d.port,"port");
        AddItem(d.user_file_path,"user_file_path");
        AddItem(d.auto_confirm_settlement,"auto_confirm_settlement");
		AddItem(d.log_price_info,"log_price_info");
		AddItem(d.use_new_inst_service,"use_new_inst_service");
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

    Log().WithField("fun","LoadConfig")
        .WithField("trading_day",g_config.trading_day).
        Log(LOG_INFO, "try to load config");

    SerializerConfig ss;
    if (!ss.FromFile("/etc/open-trade-gateway/config.json"))
    {
                Log().WithField("fun", "LoadConfig")
                        .WithField("fileName","/etc/open-trade-gateway/config.json")
                        .Log(LOG_FATAL,"load config json file fail");
                return false;
    }

    ss.ToVar(g_config);
                
    std::map<std::string, BrokerConfig> brokerConfigMap;
    std::vector< std::string> brokerNameList;
	std::string strFileName = "/etc/open-trade-gateway/broker_list.json";
        
    if (boost::filesystem::exists(strFileName))
    {
            SerializerConfig ss_broker;
            if (!ss_broker.FromFile(strFileName.c_str()))
            {
                    Log().WithField("fun","LoadConfig")
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
                    Log().WithField("fun", "LoadConfig")
                            .WithField("fileName", fileName).
                            Log(LOG_WARNING, "load broker json file fail");
                    continue;
            }

            BrokerConfig bc;
            ss_broker.ToVar(bc);

            brokerConfigMap[bc.broker_name] = bc;
            brokerNameList.push_back(bc.broker_name);
    }
   
    if (brokerNameList.empty())
    {
            Log().WithField("fun","LoadConfig")
                    .Log(LOG_FATAL,"load broker list fail!");
            return false;
    }

	g_config.brokers = brokerConfigMap;        
	boost::filesystem::path ufpath(g_config.user_file_path);        
	for (auto it : g_config.brokers)
	{
			BrokerConfig& broker = it.second;
			if (!boost::filesystem::exists(ufpath/broker.broker_name))
			{
					boost::filesystem::create_directory(ufpath/broker.broker_name);
			}                
	}

	SerializerTradeBase ss_broker_list_str;
	rapidjson::Pointer("/aid").Set(*ss_broker_list_str.m_doc,"rtn_brokers");
	for (int i = 0; i < brokerNameList.size();i++)
	{
			rapidjson::Pointer("/brokers/" + std::to_string(i)).Set(*ss_broker_list_str.m_doc, brokerNameList[i]);
	}
	ss_broker_list_str.ToString(&g_config.broker_list_str);

	SerializerConditionOrderData ss2;
	if (!ss2.FromFile("/etc/open-trade-gateway/config-condition-order.json"))
	{
			Log().WithField("fun", "LoadConfig")                        
					.WithField("fileName", "/etc/open-trade-gateway/config-condition-order.json")
					.Log(LOG_INFO, "load condition order config file fail");
			return false;
	}
	ss2.ToVar(g_condition_order_config);
        
    return true;
}

Config::Config()
        :host("")
        ,port(7788)
        ,user_file_path("")
        ,auto_confirm_settlement(false)
		,log_price_info(true)
		,use_new_inst_service(false)
        ,brokers()
        ,broker_list_str("")
        ,trading_day("")
{
}
