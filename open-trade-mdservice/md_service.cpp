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
#include "version.h"

using namespace std::chrono;

mdservice::mdservice(boost::asio::io_context& ios)
	:io_context_(ios)	
	,m_segment(nullptr)
	,m_ins_map(nullptr)	
	,m_req_subscribe_quote("")
	,m_req_peek_message("")
	,m_md_connection_ptr()
	,m_stop_reconnect(false)
	,m_need_reconnect(false)
	,_timer(io_context_)
{
}

bool mdservice::init()
{		
	try
	{
		if (!OpenInstList())
		{
			Log().WithField("fun","init")
				.WithField("key","mdservice")				
				.Log(LOG_INFO,"mdservice open ins list map fail");

			return false;
		}

		std::string ins_list;
		for (auto it = m_ins_map->begin(); it != m_ins_map->end(); ++it)
		{
			auto& key = it->first;
			Instrument& ins = it->second;
			if (!ins.expired && (ins.product_class == kProductClassFutures
				|| ins.product_class == kProductClassOptions
				|| ins.product_class == kProductClassFOption
				|| ins.product_class == kProductClassCombination
				)) {
				ins_list += std::string(key.data());
				ins_list += ",";
			}
		}

		m_req_peek_message = "{\"aid\":\"peek_message\"}";
		m_req_subscribe_quote = "{\"aid\": \"subscribe_quote\", \"ins_list\": \"" + ins_list + "\"}";

		StartConnect();

		//10秒后尝试重连
		_timer.expires_from_now(boost::posix_time::seconds(10));
		_timer.async_wait(std::bind(
			&mdservice::ReStartConnect
			, this));

		return true;
	}
	catch (...)
	{
		Log().WithField("fun","Init")
			.WithField("key","mdservice")			
			.Log(LOG_WARNING,"mdservice Init exception");	
		return false;
	}
}

void mdservice::stop()
{
	m_stop_reconnect = true;
	m_need_reconnect = false;

	if (nullptr != m_md_connection_ptr)
	{		
		m_md_connection_ptr->Stop();
	}
	m_md_connection_ptr.reset();
	
	Log().WithField("fun","stop")
		.WithField("key","mdservice")		
		.Log(LOG_INFO,"mdservice stop");		
}

void mdservice::StartConnect()
{
	try
	{
		Log().WithField("fun","StartConnect")
			.WithField("key","mdservice")
			.Log(LOG_INFO,"mdservice StartConnect openmd service");
			
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
		Log().WithField("fun","StartConnect")
			.WithField("key","mdservice")
			.WithField("errmsg",ex.what())
			.Log(LOG_FATAL,"mdservice StartConnect fail");
	}
}

void mdservice::ReStartConnect()
{
	try
	{
		if (m_stop_reconnect)
		{
			return;
		}

		if (!m_need_reconnect)
		{
			//10秒后尝试重连
			_timer.expires_from_now(boost::posix_time::seconds(10));
			_timer.async_wait(std::bind(
				&mdservice::ReStartConnect
				, this));
			return;
		}

		Log().WithField("fun","ReStartConnect")
			.WithField("key","mdservice")			
			.Log(LOG_INFO,"mdservice ReStartConnect openmd service");
				
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

		//10秒后尝试重连
		m_need_reconnect = false;
		_timer.expires_from_now(boost::posix_time::seconds(10));
		_timer.async_wait(std::bind(
			&mdservice::ReStartConnect
			, this));
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun", "ReStartConnect")
			.WithField("key", "mdservice")
			.WithField("errmsg",ex.what())
			.Log(LOG_FATAL, "mdservice ReStartConnect fail");
	}
}

void mdservice::OnConnectionnClose()
{
	m_need_reconnect = true;	
}

void mdservice::OnConnectionnError()
{
	m_need_reconnect = true;
}

bool mdservice::OpenInstList()
{
	try
	{
		m_segment = new  boost::interprocess::managed_shared_memory(
			boost::interprocess::open_only,
			"InsMapSharedMemory");
		std::pair<InsMapType*, std::size_t> p = m_segment->find<InsMapType>("InsMap");
		m_ins_map = p.first;
		return true;
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun", "OpenInstList")
			.WithField("key", "mdservice")
			.WithField("errmsg", ex.what()).
			Log(LOG_FATAL, "open InsMapSharedMemory fail");
		return false;
	}
}