/////////////////////////////////////////////////////////////////////////
///@file config.cpp
///@brief	配置文件读写
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "config.h"

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
    }
};

Config g_config;

bool LoadConfig()
{
    SerializerConfig ss;
    if (!ss.FromFile("config.json"))
        return false;
    ss.ToVar(g_config);
    SerializerConfig ss_broker;
    if (!ss_broker.FromFile("brokers.json"))
        return false;
    ss_broker.ToVar(g_config.brokers);
    return true;
}

Config::Config()
{
    //配置参数默认值
    port = 7777;

    //主程序exe所在路径
    char buffer[MAX_PATH + 1];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string str = buffer;
    std::string root_path = str.substr(0, str.rfind('\\') + 1);

    //各类文件位置
    ins_file_path = root_path + "\\ins.json";
    md_ca_file_path = root_path + "\\ca.cert";
}
