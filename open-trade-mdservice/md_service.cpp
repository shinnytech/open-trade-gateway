/////////////////////////////////////////////////////////////////////////
///@file md_service.cpp
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "md_service.h"

#include <iostream>
#include <functional>
#include <utility>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "log.h"
#include "config.h"
#include "rapid_serialize.h"
#include "http.h"
#include "version.h"

using namespace std::chrono;

const char* ins_file_url = "http://openmd.shinnytech.com/t/md/symbols/latest.json";

class InsFileParser
	: public RapidSerialize::Serializer<InsFileParser>
{
public:
	void DefineStruct(Instrument& d)
	{
		AddItem(d.expired, ("expired"));
		AddItemEnum(d.product_class, ("class"), {
			{ kProductClassFutures, ("FUTURE") },
			{ kProductClassOptions, ("OPTION") },
			{ kProductClassCombination, ("FUTURE_COMBINE") },
			{ kProductClassFOption, ("FUTURE_OPTION") },
			{ kProductClassFutureIndex, ("FUTURE_INDEX") },
			{ kProductClassFutureContinuous, ("FUTURE_CONT") },
			{ kProductClassStock, ("STOCK") },
			});
		AddItem(d.volume_multiple, ("volume_multiple"));
		AddItem(d.price_tick, ("price_tick"));
		AddItem(d.margin, ("margin"));
		AddItem(d.commission, ("commission"));
	}
};

mdservice::mdservice(boost::asio::io_context& ios)
	:io_context_(ios)
	,m_segment(nullptr)
	,m_alloc_inst(nullptr)
	,m_ins_map(nullptr)	
	,m_req_subscribe_quote("")
	,m_req_peek_message("")
	,m_md_connection_ptr()
	,m_stop_reconnect(false)
	,_timer(io_context_)
{
}

bool mdservice::init()
{
	if (!LoadInsList())
	{
		return false;
	}
		
	try
	{
		std::string ins_list;
		for (auto it = m_ins_map->begin(); it != m_ins_map->end(); ++it)
		{
			auto& key = it->first;
			Instrument& ins = it->second;
			if (!ins.expired && (ins.product_class == kProductClassFutures
				|| ins.product_class == kProductClassOptions
				|| ins.product_class == kProductClassFOption
				)) {
				ins_list += std::string(key.data());
				ins_list += ",";
			}
		}

		m_req_peek_message = "{\"aid\":\"peek_message\"}";
		m_req_subscribe_quote = "{\"aid\": \"subscribe_quote\", \"ins_list\": \"" + ins_list + "\"}";

		StartConnect();

		return true;
	}
	catch (...)
	{
		Log.WithField("fun", "Init").WithField("msg", "mdservice Init exception").WithField("key", "mdservice").Write(LOG_ERROR);
		return false;
	}
}


bool mdservice::LoadInsList()
{
	try
	{
		boost::interprocess::shared_memory_object::remove("InsMapSharedMemory");

		m_segment =new boost::interprocess::managed_shared_memory
							(boost::interprocess::create_only
								, "InsMapSharedMemory" //segment name
								, 32 * 1024 * 1024);  //segment size in bytes

		//Initialize the shared memory STL-compatible allocator
		//ShmemAllocator alloc_inst (segment.get_segment_manager());
		m_alloc_inst = new ShmemAllocator(m_segment->get_segment_manager());

		//Construct a shared memory map.
		//Note that the first parameter is the comparison function,
		//and the second one the allocator.
		//This the same signature as std::map's constructor taking an allocator
		m_ins_map =
			m_segment->construct<InsMapType>("InsMap")//object name
			(CharArrayComparer() //first  ctor parameter
				, *m_alloc_inst);     //second ctor parameter		
	}
	catch (std::exception& ex)
	{
		Log.WithField("fun", "LoadInsList").WithField("errmsg", ex.what()).WithField("msg", "mdservice construct m_ins_map fail").WithField("key", "mdservice").Write(LOG_FATAL);
		return false;
	}

	try
	{
		//下载和加载合约表文件
		std::string content;
		if (HttpGet(ins_file_url, &content) != 0)
		{
			Log.WithField("fun", "LoadInsList").WithField("msg", "md service download ins file fail").WithField("key", "mdservice").Write(LOG_FATAL);
			return false;
		}

		Log.WithField("fun", "LoadInsList").WithField("msg", "mdservice download ins file success").WithField("key", "mdservice").WithField("pack", content.c_str()).Write(LOG_INFO);

		InsFileParser ss;
		if (!ss.FromString(content.c_str()))
		{
			Log.WithField("fun", "LoadInsList").WithField("msg", "md service parse downloaded ins file fail").WithField("key", "mdservice").Write(LOG_FATAL);
			return false;
		}

		for (auto& m : ss.m_doc->GetObject())
		{
			std::array<char, 64> key = {};
			strncpy(key.data(), m.name.GetString(), 64);
			ss.ToVar((*m_ins_map)[key], &m.value);
		}

	}
	catch (std::exception& ex)
	{
		Log.WithField("fun", "LoadInsList").WithField("errmsg", ex.what()).WithField("msg", "get inst list fail").WithField("key", "mdservice").Write(LOG_FATAL);
		return false;
	}
	
	return true;
}

void mdservice::stop()
{
	m_stop_reconnect = true;
	if (nullptr != m_md_connection_ptr)
	{		
		m_md_connection_ptr->Stop();
	}
	m_md_connection_ptr.reset();
	
	Log.WithField("fun", "stop").WithField("msg", "mdservice stop").WithField("key", "mdservice").Write(LOG_INFO);

	//TODO::先不释放如下资源,反正进程要退出
	//m_segment
	//m_alloc_inst
	//m_ins_map 	
}

void mdservice::StartConnect()
{
	try
	{
		Log.WithField("fun", "StartConnect").WithField("msg", "mdservice StartConnect openmd service").WithField("key", "mdservice").Write(LOG_INFO);

		if (nullptr != m_md_connection_ptr)
		{
			m_md_connection_ptr.reset();
		}
		m_md_connection_ptr = std::make_shared<md_connection>(
			io_context_
			, m_req_subscribe_quote
			, m_req_peek_message
			, m_ins_map
			, *this);
		if (nullptr != m_md_connection_ptr)
		{
			m_md_connection_ptr->Start();
		}
	}
	catch (std::exception& ex)
	{
		Log.WithField("fun", "StartConnect").WithField("errmsg", ex.what()).WithField("msg", "mdservice StartConnect fail").WithField("key", "mdservice").Write(LOG_FATAL);
	}
}

void mdservice::ReStartConnect()
{
	try
	{
		Log.WithField("fun", "ReStartConnect").WithField("msg", "mdservice ReStartConnect openmd service").WithField("key", "mdservice").Write(LOG_INFO);

		if (nullptr != m_md_connection_ptr)
		{
			m_md_connection_ptr.reset();
		}
		m_md_connection_ptr = std::make_shared<md_connection>(
			io_context_
			, m_req_subscribe_quote
			, m_req_peek_message
			, m_ins_map
			, *this);
		if (nullptr != m_md_connection_ptr)
		{
			m_md_connection_ptr->Start();
		}
	}
	catch (std::exception& ex)
	{
		Log.WithField("fun", "ReStartConnect").WithField("errmsg", ex.what()).WithField("msg", "mdservice ReStartConnect fail").WithField("key", "mdservice").Write(LOG_FATAL);
	}
}

void mdservice::OnConnectionnClose()
{
	if (m_stop_reconnect)
	{
		return;
	}
	//10秒后重连
	_timer.expires_from_now(boost::posix_time::seconds(10));
	_timer.async_wait(std::bind(
		&mdservice::ReStartConnect
		, this));
}

void mdservice::OnConnectionnError()
{
	if (m_stop_reconnect)
	{
		return;
	}
	//30秒后重连
	_timer.expires_from_now(boost::posix_time::seconds(30));
	_timer.async_wait(std::bind(
		&mdservice::ReStartConnect
		, this));
}