/////////////////////////////////////////////////////////////////////////
///@file config.cpp
///@brief	配置文件读写
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "config.h"

#include <experimental/filesystem>
#include "log.h"
#include "utility.h"
#include "rapid_serialize.h"

class SerializerConfig
    : public RapidSerialize::Serializer<SerializerConfig>
{
public:
    using RapidSerialize::Serializer<SerializerConfig>::Serializer;

    void DefineStruct(Config& d)
    {
        AddItem(d.host, "host");
        AddItem(d.port, "port");
        AddItem(d.ca_file, "ca_file");
        AddItem(d.user_file_path, "user_file_path");
        AddItem(d.brokers, "brokers");
    }

    void DefineStruct(BrokerConfig& d)
    {
        AddItem(d.broker_type, "type");
        AddItem(d.ctp_broker_id, "broker_id");
        AddItem(d.trading_fronts, "trading_fronts");
        AddItem(d.product_info, "product_info");
        AddItem(d.auth_code, "auth_code");
    }
};

Config g_config;

bool LoadConfig()
{
    g_config.trading_day = GuessTradingDay();
    SerializerConfig ss;
    if (!ss.FromFile("/etc/open-trade-gateway/config.json")){
        Log(LOG_FATAL, NULL, "load /etc/open-trade-gateway/config.json file fail");
        return false;
    }
    ss.ToVar(g_config);
    SerializerConfig ss_broker;
    if (!ss_broker.FromFile("/etc/open-trade-gateway/brokers.json")){
        Log(LOG_FATAL, NULL, "load /etc/open-trade-gateway/brokers.json file fail");
        return false;
    }
    ss_broker.ToVar(g_config.brokers);
    std::experimental::filesystem::v1::path ufpath(g_config.user_file_path);
    for (auto it = g_config.brokers.begin(); it != g_config.brokers.end(); ++it) {
        std::string bid = it->first;
        std::experimental::filesystem::v1::create_directories(ufpath / bid);
    }
    return true;
}

Config::Config()
{
    //配置参数默认值
    port = 7788;
}
