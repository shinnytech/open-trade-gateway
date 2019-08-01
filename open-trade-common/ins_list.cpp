/////////////////////////////////////////////////////////////////////////
///@file ins_list.cpp
///@brief	查询合约信息
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "ins_list.h"
#include "log.h"

#include <map>
#include <boost/algorithm/string.hpp>

struct InstConfig
{
	InstConfig();

	InsMapType* m_ins_map;

	boost::interprocess::managed_shared_memory* m_segment;

	std::map<std::string, std::string> m_instrumentExchangeIdMap;
};

InstConfig::InstConfig()
	:m_ins_map(nullptr)
	, m_segment(nullptr)
	, m_instrumentExchangeIdMap()
{
}

InstConfig g_instConfig;

bool GenInstrumentExchangeIdMap()
{
	if (nullptr == g_instConfig.m_ins_map)
	{
		try
		{
			g_instConfig.m_segment= new  boost::interprocess::managed_shared_memory(
				boost::interprocess::open_only,
				"InsMapSharedMemory");
			std::pair<InsMapType*, std::size_t> p = g_instConfig.m_segment->find<InsMapType>("InsMap");
			g_instConfig.m_ins_map = p.first;
		}
		catch (const std::exception& ex)
		{
			Log().WithField("fun","GenInstrumentExchangeIdMap")
				.WithField("errmsg",ex.what()).
				Log(LOG_FATAL,"open InsMapSharedMemory fail");			
			return false;
		}
	}

	if (nullptr != g_instConfig.m_ins_map)
	{
		for (auto it : *(g_instConfig.m_ins_map))
		{			
			const std::array<char, 64>& key  = it.first;
			std::string symbol= std::string(key.data());
			std::vector<std::string> kvs;
			boost::algorithm::split(kvs, symbol, boost::algorithm::is_any_of("."));
					
			if (kvs.size() != 2)
			{
				continue;
			}

			std::string exchangeId = kvs[0];
			std::string instrumentId= kvs[1];
			
			if (g_instConfig.m_instrumentExchangeIdMap.find(instrumentId) ==
				g_instConfig.m_instrumentExchangeIdMap.end())
			{
				g_instConfig.m_instrumentExchangeIdMap.insert(
					std::map<std::string, std::string>::value_type(instrumentId, exchangeId)
				);
			}
						
		}	

		return true;
	}
	else
	{
		return false;
	}	
}

Instrument* GetInstrument(const std::string& symbol)
{	
	if (nullptr == g_instConfig.m_ins_map)
	{
		try
		{			
			g_instConfig.m_segment =new  boost::interprocess::managed_shared_memory(
				boost::interprocess::open_only,
				"InsMapSharedMemory"
			);					
			std::pair<InsMapType*, std::size_t> p = g_instConfig.m_segment->find<InsMapType>("InsMap");
			g_instConfig.m_ins_map = p.first;
		}
		catch (const std::exception& ex)
		{			
			Log().WithField("fun", "GetInstrument")
				.WithField("errmsg", ex.what()).
				Log(LOG_FATAL,"open InsMapSharedMemory fail");
			return nullptr;
		}		
	}	
	
	if (nullptr != g_instConfig.m_ins_map)
	{
		std::array<char, 64> key = {};
		std::copy(symbol.begin(), symbol.end(), key.data());
		auto it = g_instConfig.m_ins_map->find(key);
		if (it != g_instConfig.m_ins_map->end())
		{			
			return &(it->second);
		}
	}
	return nullptr;
}

std::string GuessExchangeId(const std::string& instrument_id)
{		
	std::map<std::string, std::string>::iterator it
		= g_instConfig.m_instrumentExchangeIdMap.find(instrument_id);
	if (it == g_instConfig.m_instrumentExchangeIdMap.end())
	{
		return "UNKNOWN";
	}
	else
	{
		return it->second;
	}
}