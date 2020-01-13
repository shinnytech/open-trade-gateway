/////////////////////////////////////////////////////////////////////////
///@file trade_server.cpp
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "trade_server.h"
#include "log.h"
#include "config.h"
#include "user_process_info.h"
#include "condition_order_serializer.h"
#include "datetime.h"
#include "SerializerTradeBase.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <functional>

using namespace std;

trade_server::trade_server(boost::asio::io_context& ios,int port)
	:io_context_(ios)
	,_endpoint(boost::asio::ip::tcp::v4(), port)
	,acceptor_(io_context_)
	,connection_manager_()
	,_connection_id(0)
	,_timer(io_context_)	
	,m_need_auto_start_ctp(true)
	,m_stop_server(false)
{
}

bool trade_server::init()
{	
	boost::system::error_code ec;

	//Open the acceptor
	acceptor_.open(_endpoint.protocol(),ec);
	if (ec)
	{
		Log().WithField("fun","init")
			.WithField("key","gateway")
			.Log(LOG_ERROR,"trade server acceptor open fail");		
		return false;
	}

	//Allow address reuse
	acceptor_.set_option(boost::asio::socket_base::reuse_address(true),ec);
	if (ec)
	{
		Log().WithField("fun","init")
			.WithField("key","gateway")
			.WithField("errmsg",ec.message())
			.Log(LOG_ERROR,"trade server acceptor set option fail");		
		return false;
	}

	// Bind to the server address
	acceptor_.bind(_endpoint,ec);
	if (ec)
	{
		Log().WithField("fun","init")
			.WithField("key","gateway")
			.WithField("errmsg",ec.message())
			.Log(LOG_ERROR,"trade server acceptor bind fail");		
		return false;
	}

	//Start listening for connections
	acceptor_.listen(boost::asio::socket_base::max_listen_connections
		, ec);
	if (ec)
	{
		Log().WithField("fun","init")
			.WithField("key","gateway")
			.WithField("errmsg",ec.message())
			.Log(LOG_ERROR,"trade server acceptor listen fail");		
		return false;
	}

	do_accept();
	
	if (!m_stop_server)
	{
		_timer.expires_from_now(boost::posix_time::seconds(10));
		_timer.async_wait(boost::bind(&trade_server::OnCheckServerStatus, this));
	}

	return true;
}

void trade_server::do_accept()
{
	acceptor_.async_accept(
		std::bind(&trade_server::OnAccept,
			this,
			std::placeholders::_1,
			std::placeholders::_2));
}

void trade_server::OnAccept(boost::system::error_code ec
	, boost::asio::ip::tcp::socket socket)
{
	if (!acceptor_.is_open())
	{
		return;
	}

	if (ec)
	{
		Log().WithField("fun","OnAccept")
			.WithField("key","gateway")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"trade_server accept error");		
		do_accept();
		return;
	}	
	
	connection_manager_.start(std::make_shared<connection>(
		io_context_,std::move(socket), connection_manager_
		,++_connection_id));

	do_accept();
}

void trade_server::stop()
{
	m_stop_server = true;
	boost::system::error_code ec;
	_timer.cancel(ec);
	if (ec)
	{
		Log().WithField("fun","stop")
			.WithField("key","gateway")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway cancel timer fail!");	
	}
	else
	{
		Log().WithField("fun", "stop")
			.WithField("key", "gateway")			
			.Log(LOG_INFO, "open trade gateway cancel timer success!");
	}

	acceptor_.close();
	connection_manager_.stop_all();
	
	//关闭子进程
	for (auto it:g_userProcessInfoMap)
	{
		UserProcessInfo_ptr& pro_ptr = it.second;
		if (nullptr != pro_ptr)
		{
			pro_ptr->StopProcess();
		}		
	}	
	g_userProcessInfoMap.clear();
}

void trade_server::OnCheckServerStatus()
{
	Log().WithField("fun","OnCheckServerStatus")
		.WithField("key","gateway")
		.Log(LOG_INFO,"on check server status");

	if (m_stop_server)
	{
		return;
	}
	
	boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
	int weekNumber = tm.date().day_of_week();
	int timeValue = tm.time_of_day().hours() * 100 + tm.time_of_day().minutes();

	if (IsInTimeSpan(g_condition_order_config.auto_start_ctp_time,weekNumber,timeValue))
	{
		if (!g_condition_order_config.has_auto_start_ctp)
		{
			Log().WithField("fun", "OnCheckServerStatus")
				.WithField("key", "gateway")
				.Log(LOG_INFO, "auto start ctp");
			g_condition_order_config.has_auto_start_ctp = true;
			TryStartTradeInstance();
			m_need_auto_start_ctp = false;
		}
	}
	else
	{
		g_condition_order_config.has_auto_start_ctp = false;
	}

	if (IsInTimeSpan(g_condition_order_config.auto_close_ctp_time, weekNumber, timeValue))
	{
		if (!g_condition_order_config.has_auto_close_ctp)
		{
			Log().WithField("fun", "OnCheckServerStatus")
				.WithField("key", "gateway")
				.Log(LOG_INFO, "auto stop ctp");
			g_condition_order_config.has_auto_close_ctp = true;
			TryStopTradeInstance();
		}
	}
	else
	{
		g_condition_order_config.has_auto_close_ctp = false;
	}

	if (IsInTimeSpan(g_condition_order_config.auto_restart_process_time,weekNumber,timeValue))
	{
		if (m_need_auto_start_ctp)
		{
			Log().WithField("fun", "OnCheckServerStatus")
				.WithField("key", "gateway")
				.Log(LOG_INFO, "auto start ctp in auto restart process time span");
			TryStartTradeInstance();
			m_need_auto_start_ctp = false;
		}
		else
		{
			TryRestartProcesses();
		}
	}

	_timer.expires_from_now(boost::posix_time::seconds(30));
	_timer.async_wait(boost::bind(&trade_server::OnCheckServerStatus,this));
}

bool trade_server::GetReqStartTradeKeyMap(req_start_trade_key_map& rsckMap)
{
	boost::filesystem::path user_file_path;
	if (!g_config.user_file_path.empty())
	{
		user_file_path = g_config.user_file_path + "/";
	}
	else
	{
		user_file_path = "/var/local/lib/open-trade-gateway/";
	}

	if (!boost::filesystem::exists(user_file_path))
	{
		return false;
	}

	boost::filesystem::recursive_directory_iterator beg_iter(user_file_path);
	boost::filesystem::recursive_directory_iterator end_iter;
	for (; beg_iter != end_iter; ++beg_iter)
	{
		if (!boost::filesystem::is_directory(*beg_iter))
		{
			continue;
		}

		boost::filesystem::path bid_file_path = beg_iter->path();
		boost::filesystem::recursive_directory_iterator beg_iter_2(bid_file_path);
		boost::filesystem::recursive_directory_iterator end_iter_2;
				
		req_start_trade_instance req_start_trade;
		for (; beg_iter_2 != end_iter_2; ++beg_iter_2)
		{
			if (!boost::filesystem::is_regular_file(*beg_iter_2))
			{
				continue;
			}

			boost::filesystem::path key_file_path = (*beg_iter_2).path();
			if (key_file_path.extension().string() != ".co")
			{
				continue;
			}

			std::string strKey = key_file_path.filename().string();
			strKey = strKey.substr(0, strKey.length() - 3);

			//ycsong 2019/08/16
			if (strKey.find(u8"sim_快期模拟_游客_", 0) != std::string::npos)
			{
				continue;
			}			
			
			SerializerConditionOrderData nss;
			bool loadfile = nss.FromFile(key_file_path.string().c_str());
			if (!loadfile)
			{
				continue;
			}

			ConditionOrderData condition_order_data;
			nss.ToVar(condition_order_data);

			//条件单数量为空的用户不自动重启
			if (condition_order_data.condition_orders.empty())
			{
				Log().WithField("fun", "GetReqStartTradeKeyMap")
					.WithField("key", "gateway")
					.WithField("trading_day", condition_order_data.trading_day)
					.WithField("user_name",condition_order_data.user_id)
					.WithField("broker_id", condition_order_data.broker_id)					
					.Log(LOG_INFO, "user's condition order is empty");

				continue;
			}

			req_start_trade.bid = condition_order_data.broker_id;
			req_start_trade.user_name = condition_order_data.user_id;
			req_start_trade.password = condition_order_data.user_password;
											
			if (rsckMap.find(strKey) == rsckMap.end())
			{
				rsckMap.insert(req_start_trade_key_map::value_type(strKey
					, req_start_trade));
			}
		}
	}

	return true;
}

void trade_server::TryStartTradeInstance()
{
	Log().WithField("fun","TryStartTradeInstance")
		.WithField("key","gateway")
		.Log(LOG_INFO,"try to start trade instance");

	req_start_trade_key_map rsckMap;
	if (!GetReqStartTradeKeyMap(rsckMap))
	{
		return;
	}

	if (rsckMap.empty())
	{
		Log().WithField("fun", "TryStartTradeInstance")
			.WithField("key", "gateway")
			.Log(LOG_INFO, "rsckMap is empty");
		return;
	}

	for (auto a : rsckMap)
	{
		const std::string& strKey = a.first;
		req_start_trade_instance& req_start_trade = a.second;
		
		Log().WithField("fun", "TryStartTradeInstance")
			.WithField("key", "gateway")
			.WithField("user_key",strKey)
			.Log(LOG_INFO, "start trade instance");		

		StartTradeInstance(strKey, req_start_trade);
	}
}

void trade_server::StartTradeInstance(const std::string& strKey
	, req_start_trade_instance& req_start_trade)
{
	auto it = g_config.brokers.find(req_start_trade.bid);
	if (it == g_config.brokers.end())
	{
		Log().WithField("fun","StartTradeInstance")
			.WithField("key","gateway")
			.WithField("bid",req_start_trade.bid)
			.Log(LOG_WARNING,"invalid bid");		
		return;
	}
	
	std::string broker_type = it->second.broker_type;
	bool flag = false;
	if (broker_type == "ctp")
	{
		flag = true;
	}
	else if (broker_type == "ctpsopt")
	{
		flag = true;
	}
	else if (broker_type == "ctpse")
	{
		flag = true;
	}
	else if (broker_type == "sim")
	{
		flag = true;
	}
	else if (broker_type == "kingstar")
	{
		flag = true;
	}
	else if (broker_type == "perftest")
	{
		flag = true;
	}
	else
	{
		flag = false;
	}
	
	if (!flag)
	{
		Log().WithField("fun","StartTradeInstance")
			.WithField("key","gateway")
			.WithField("bid",req_start_trade.bid)
			.WithField("bidtype",broker_type)
			.Log(LOG_WARNING,"invalid bid type");	
		return;
	}
	   
	auto userIt = g_userProcessInfoMap.find(strKey);
	//如果用户进程没有启动,启动用户进程处理
	if (userIt == g_userProcessInfoMap.end())
	{
		Log().WithField("fun","StartTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)		
			.Log(LOG_INFO,"start not started trade instance");

		ReqLogin reqLogin;
		reqLogin.aid = "req_login";
		reqLogin.bid = req_start_trade.bid;
		reqLogin.user_name = req_start_trade.user_name;
		reqLogin.password = req_start_trade.password;
		reqLogin.broker = it->second;

		UserProcessInfo_ptr userProcessInfoPtr = std::make_shared<UserProcessInfo>(
			io_context_, strKey, reqLogin);

		if (nullptr == userProcessInfoPtr)
		{
			Log().WithField("fun","StartTradeInstance")
				.WithField("key","gateway")
				.WithField("user_key", strKey)
				.Log(LOG_ERROR,"new user process fail");		
			return;
		}

		if (!userProcessInfoPtr->StartProcess())
		{
			Log().WithField("fun","StartTradeInstance")
				.WithField("key","gateway")
				.WithField("user_key",strKey)
				.Log(LOG_ERROR,"can not start up user process");			
			return;
		}

		g_userProcessInfoMap.insert(TUserProcessInfoMap::value_type(
			strKey,userProcessInfoPtr));

		SerializerTradeBase nss;
		nss.FromVar(reqLogin);
		std::string strMsg;
		nss.ToString(&strMsg);

		//发送登录请求
		userProcessInfoPtr->SendMsg(0,strMsg);
	}
	//如果用户进程已经启动,直接利用
	else
	{
		UserProcessInfo_ptr userProcessInfoPtr = userIt->second;
		//进程是否正常运行
		bool flag = userProcessInfoPtr->ProcessIsRunning();
		if (flag)
		{
			Log().WithField("fun","StartTradeInstance")
				.WithField("key","gateway")
				.WithField("user_key", strKey)
				.Log(LOG_INFO,"start still running trade instance");		

			SerializerConditionOrderData nss;
			nss.FromVar(req_start_trade);
			std::string strMsg;
			nss.ToString(&strMsg);
			//发送重启交易实例的请求
			userProcessInfoPtr->SendMsg(0,strMsg);
		}
		else
		{
			Log().WithField("fun","StartTradeInstance")
				.WithField("key","gateway")
				.WithField("user_key",strKey)
				.Log(LOG_INFO,"start has stopped trade instance");

			//启动进程
			flag = userProcessInfoPtr->StartProcess();
			if (!flag)
			{
				Log().WithField("fun","StartTradeInstance")
					.WithField("key","gateway")
					.WithField("user_key",strKey)
					.Log(LOG_ERROR,"can not start up user process");				
				return;
			}

			ReqLogin reqLogin;
			reqLogin.aid = "req_login";
			reqLogin.bid = req_start_trade.bid;
			reqLogin.user_name = req_start_trade.user_name;
			reqLogin.password = req_start_trade.password;
			reqLogin.broker = it->second;

			SerializerTradeBase nss;
			nss.FromVar(reqLogin);
			std::string strMsg;
			nss.ToString(&strMsg);

			//发送登录请求
			userProcessInfoPtr->SendMsg(0,strMsg);
		}
	}	
}

void trade_server::TryStopTradeInstance()
{
	Log().WithField("fun","TryStopTradeInstance")
		.WithField("key","gateway")		
		.Log(LOG_INFO,"try to stop trade instance");

	//给子进程发消息
	for (auto it : g_userProcessInfoMap)
	{
		UserProcessInfo_ptr& pro_ptr = it.second;

		if (!pro_ptr->ProcessIsRunning())
		{
			continue;
		}

		ReqLogin reqLogin= pro_ptr->GetReqLogin();
		std::string strKey = it.first;
		req_start_trade_instance req_start_trade;
		req_start_trade.aid = "req_stop_ctp";
		req_start_trade.bid = reqLogin.bid;
		req_start_trade.user_name = reqLogin.user_name;
		req_start_trade.password = reqLogin.password;

		Log().WithField("fun","TryStopTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)
			.Log(LOG_INFO,"stop trade instance");
		
		StopTradeInstance(strKey,req_start_trade);
	}	
}

void trade_server::StopTradeInstance(const std::string& strKey
	, req_start_trade_instance& req_start_trade)
{
	auto it = g_config.brokers.find(req_start_trade.bid);
	if (it == g_config.brokers.end())
	{
		Log().WithField("fun","StopTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)
			.WithField("bid",req_start_trade.bid)
			.Log(LOG_WARNING,"invalid bid");		
		return;
	}

	std::string broker_type = it->second.broker_type;
	bool flag = false;
	if (broker_type == "ctp")
	{
		flag = true;
	}
	else if (broker_type == "ctpsopt")
	{
		flag = true;
	}
	else if (broker_type == "ctpse")
	{
		flag = true;
	}
	else if (broker_type == "sim")
	{
		flag = true;
	}
	else if (broker_type == "kingstar")
	{
		flag = true;
	}
	else if (broker_type == "perftest")
	{
		flag = true;
	}
	else
	{
		flag = false;
	}

	if (!flag)
	{
		Log().WithField("fun","StopTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)
			.WithField("bid",req_start_trade.bid)
			.WithField("bidtype",broker_type)
			.Log(LOG_WARNING,"invalid bid type");		
		return;
	}

	auto userIt = g_userProcessInfoMap.find(strKey);

	//如果用户进程没有启动,直接退出
	if (userIt == g_userProcessInfoMap.end())
	{
		Log().WithField("fun","StopTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)
			.WithField("bid",req_start_trade.bid)			
			.Log(LOG_WARNING,"process is not started");	
		return;
	}

	UserProcessInfo_ptr userProcessInfoPtr = userIt->second;
	//进程是否正常运行
	flag = userProcessInfoPtr->ProcessIsRunning();
	if (!flag)
	{
		Log().WithField("fun","StopTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)
			.WithField("bid",req_start_trade.bid)
			.Log(LOG_WARNING,"process is not running");		
		return;
	}
	else
	{
		Log().WithField("fun","StopTradeInstance")
			.WithField("key","gateway")
			.WithField("user_key",strKey)
			.WithField("bid",req_start_trade.bid)
			.Log(LOG_INFO,"send stop trade instance msg");		

		SerializerConditionOrderData nss;
		nss.FromVar(req_start_trade);
		std::string strMsg;
		nss.ToString(&strMsg);
		//发送停止交易实例的请求
		userProcessInfoPtr->SendMsg(0,strMsg);
	}	
}

void trade_server::TryRestartProcesses()
{
	//给子进程发消息
	for (auto it : g_userProcessInfoMap)
	{
		UserProcessInfo_ptr& pro_ptr = it.second;
		
		if (pro_ptr->ProcessIsRunning())
		{
			continue;
		}

		Log().WithField("fun","TryRestartProcesses")
			.WithField("key","gateway")			
			.Log(LOG_INFO,"try to restart dead process");
		
		if (!pro_ptr->ReStartProcess())
		{
			Log().WithField("fun","TryRestartProcesses")
				.WithField("key", "gateway")
				.WithField("user_key",it.first)
				.Log(LOG_WARNING, "can not restart up user process");			
		}

	}
}

bool trade_server::IsInTimeSpan(const std::vector<weekday_time_span>& timeSpan
	, int weekNumber, int timeValue)
{
	bool flag = false;
	for (auto w : timeSpan)
	{
		if (w.weekday == weekNumber)
		{
			for (auto d : w.time_span_list)
			{
				if ((timeValue >= d.begin) && (timeValue <= d.end))
				{
					flag = true;
					break;
				}
			}
			break;
		}
	}
	return flag;
}
