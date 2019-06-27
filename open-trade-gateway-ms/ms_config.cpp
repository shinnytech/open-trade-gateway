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
{
}

MasterConfig::MasterConfig()
	:host("0.0.0.0")
	,port(5566)
	,slaveNodeList()
{
}

RtnBrokersMsg::RtnBrokersMsg()
	:aid("")
	,brokers()
{
}

bool LoadMasterConfig()
{
	MasterSerializerConfig ss;
	if (!ss.FromFile("/etc/open-trade-gateway/config-ms.json"))
	{
		LogMs.WithField("msg", "load /etc/open-trade-gateway/config-ms.json file fail").WithField("key", "gatewayms").Write(LOG_FATAL);
		return false;
	}

	ss.ToVar(g_masterConfig);

	return true;
}

void MasterSerializerConfig::DefineStruct(MasterConfig& c)
{
	AddItem(c.host, "host");
	AddItem(c.port, "port");
	AddItem(c.slaveNodeList, "slaveNodeList");
}

void MasterSerializerConfig::DefineStruct(SlaveNodeInfo& s)
{
	AddItem(s.name, "name");
	AddItem(s.host, "host");
	AddItem(s.port, "port");
	AddItem(s.path, "path");
}

void MasterSerializerConfig::DefineStruct(RtnBrokersMsg& b)
{
	AddItem(b.aid, "aid");
	AddItem(b.brokers, "brokers");
}