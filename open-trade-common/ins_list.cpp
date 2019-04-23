/////////////////////////////////////////////////////////////////////////
///@file ins_list.cpp
///@brief	查询合约信息
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "ins_list.h"
#include "log.h"

Instrument* GetInstrument(const std::string& symbol)
{
	static InsMapType* m_ins_map = nullptr;
	if (nullptr == m_ins_map)
	{
		try
		{			
			boost::interprocess::managed_shared_memory* segment=new  boost::interprocess::managed_shared_memory(
				boost::interprocess::open_only,
				"InsMapSharedMemory"
			);					
			std::pair<InsMapType*, std::size_t> p = segment->find<InsMapType>("InsMap");			
			m_ins_map = p.first;				
		}
		catch (const std::exception& ex)
		{
			Log(LOG_FATAL, NULL, "GetInstrument open InsMapSharedMemory fail:%s",ex.what());
			return nullptr;
		}		
	}	
	
	if (nullptr != m_ins_map)
	{
		std::array<char, 64> key = {};
		std::copy(symbol.begin(), symbol.end(), key.data());
		auto it = m_ins_map->find(key);
		if (it != m_ins_map->end())
		{			
			return &(it->second);
		}
	}
	return nullptr;
}