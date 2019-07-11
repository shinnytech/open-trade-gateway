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
	,_co_config()
{
}

bool trade_server::init()
{	
	boost::system::error_code ec;

	//Open the acceptor
	acceptor_.open(_endpoint.protocol(), ec);
	if (ec)
	{
		Log(LOG_ERROR,nullptr
			,"msg=trade server acceptor open fail;errmsg=%s;key=gateway"
			, ec.message().c_str());
		return false;
	}

	//Allow address reuse
	acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec)
	{
		Log(LOG_ERROR,nullptr
			,"msg=trade server acceptor set option fail;errmsg=%s;key=gateway"
			,ec.message().c_str());
		return false;
	}

	// Bind to the server address
	acceptor_.bind(_endpoint, ec);
	if (ec)
	{
		Log(LOG_ERROR, nullptr
			,"msg=trade server acceptor bind fail;errmsg=%s;key=gateway"
			, ec.message().c_str());
		return false;
	}

	//Start listening for connections
	acceptor_.listen(boost::asio::socket_base::max_listen_connections
		, ec);
	if (ec)
	{
		Log(LOG_ERROR, nullptr
			,"msg=trade server acceptor listen fail;errmsg=%s;key=gateway"
			, ec.message().c_str());
		return false;
	}

	do_accept();

	condition_order_config tmp_co_config;
	if (LoadConditionOrderConfig(tmp_co_config))
	{
		_co_config = tmp_co_config;
	}
	_timer.expires_from_now(boost::posix_time::seconds(30));
	_timer.async_wait(boost::bind(&trade_server::OnCheckServerStatus,this));

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
		Log(LOG_WARNING, nullptr
			,"msg=trade_server accept error;errmsg=%s;key=gateway"
			, ec.message().c_str());
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
	boost::system::error_code ec;
	_timer.cancel(ec);
	if (ec)
	{
		Log(LOG_WARNING, nullptr
			, "fun=stop;msg=open trade gateway cancel timer fail!;errmsg=%s;key=gateway"
			, ec.message().c_str());
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
	Log(LOG_INFO,nullptr
		, "fun=OnCheckServerStatus;msg=on check server status;key=gateway");

	condition_order_config tmp_co_config;
	if (LoadConditionOrderConfig(tmp_co_config))
	{
		if (tmp_co_config.run_server != _co_config.run_server)
		{
			Log(LOG_INFO, nullptr
				, "fun=OnCheckServerStatus;msg=condition server status changed;key=gateway");

			_co_config.run_server = tmp_co_config.run_server;
			NotifyConditionOrderServerStatus();
		}

		_co_config.auto_start_ctp_time = tmp_co_config.auto_start_ctp_time;
		_co_config.auto_close_ctp_time = tmp_co_config.auto_close_ctp_time;
		_co_config.auto_restart_process_time = tmp_co_config.auto_restart_process_time;

		boost::posix_time::ptime tm=boost::posix_time::second_clock::local_time();
		int weekNumber=tm.date().day_of_week();
		int timeValue = tm.time_of_day().hours() * 100 + tm.time_of_day().minutes();
		
		if (IsInTimeSpan(_co_config.auto_start_ctp_time, weekNumber, timeValue))
		{
			if (!_co_config.has_auto_start_ctp)
			{
				Log(LOG_INFO, nullptr
					, "fun=OnCheckServerStatus;msg=auto start ctp;key=gateway");

				_co_config.has_auto_start_ctp = true;
				TryStartTradeInstance();
			}
		}
		else
		{
			_co_config.has_auto_start_ctp = false;
		}

		if (IsInTimeSpan(_co_config.auto_close_ctp_time, weekNumber, timeValue))
		{
			if (!_co_config.has_auto_close_ctp)
			{
				Log(LOG_INFO, nullptr
					, "fun=OnCheckServerStatus;msg=auto stop ctp;key=gateway");
				_co_config.has_auto_close_ctp = true;
				TryStopTradeInstance();
			}
		}
		else
		{
			_co_config.has_auto_close_ctp = false;
		}

		if(IsInTimeSpan(_co_config.auto_restart_process_time, weekNumber, timeValue))
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

		boost::filesystem::path bid_file_path= beg_iter->path();
		boost::filesystem::recursive_directory_iterator beg_iter_2(bid_file_path);
		boost::filesystem::recursive_directory_iterator end_iter_2;

		ConditionOrderData condition_order_data;
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

			SerializerConditionOrderData nss;
			bool loadfile = nss.FromFile(key_file_path.string().c_str());
			if (!loadfile)
			{
				continue;
			}

			nss.ToVar(condition_order_data);
			req_start_trade.bid = condition_order_data.broker_id;
			req_start_trade.user_name = condition_order_data.user_id;
			req_start_trade.password = condition_order_data.user_password;

			std::string strKey = key_file_path.filename().string();
			strKey = strKey.substr(0, strKey.length() - 3);
						
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
	Log(LOG_INFO, nullptr
		, "fun=TryStartTradeInstance;msg=try to start trade instance;key=gateway");

	req_start_trade_key_map rsckMap;
	if (!GetReqStartTradeKeyMap(rsckMap))
	{
		return;
	}

	if (rsckMap.empty())
	{
		return;
	}

	for (auto a : rsckMap)
	{
		const std::string& strKey = a.first;
		req_start_trade_instance& req_start_trade = a.second;
		
		Log(LOG_INFO, nullptr
			, "fun=TryStartTradeInstance;msg=start trade instance;key=gateway;instancekey=%s"
			, strKey.c_str());

		StartTradeInstance(strKey, req_start_trade);
	}
}

void trade_server::StartTradeInstance(const std::string& strKey
	, req_start_trade_instance& req_start_trade)
{
	auto it = g_config.brokers.find(req_start_trade.bid);
	if (it == g_config.brokers.end())
	{
		Log(LOG_WARNING, nullptr
			, "fun=StartTradeInstance;msg=invalid bid;key=gateway;bid=%s"			
			, req_start_trade.bid.c_str());
		return;
	}
	
	std::string broker_type = it->second.broker_type;
	bool flag = false;
	if (broker_type == "ctp")
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
		Log(LOG_WARNING, nullptr
			, "fun=StartTradeInstance;msg=invalid bid type;key=gateway;bid=%s;bidtype=%s"
			, req_start_trade.bid.c_str()
			, broker_type.c_str());
		return;
	}
	   
	auto userIt = g_userProcessInfoMap.find(strKey);
	//如果用户进程没有启动,启动用户进程处理
	if (userIt == g_userProcessInfoMap.end())
	{
		Log(LOG_INFO, nullptr
			, "fun=StartTradeInstance;msg=start not started trade instance;key=gateway;instancekey=%s"
			, strKey.c_str());

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
			Log(LOG_ERROR, nullptr
				, "fun=StartTradeInstance;msg=new user process fail;key=%s"				
				, strKey.c_str());
			return;
		}

		if (!userProcessInfoPtr->StartProcess())
		{
			Log(LOG_ERROR, nullptr
				, "fun=StartTradeInstance;msg=can not start up user process;key=%s"				
				, strKey.c_str());
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
			Log(LOG_INFO, nullptr
				, "fun=StartTradeInstance;msg=start still running trade instance;key=gateway;instancekey=%s"
				, strKey.c_str());

			SerializerConditionOrderData nss;
			nss.FromVar(req_start_trade);
			std::string strMsg;
			nss.ToString(&strMsg);
			//发送重启交易实例的请求
			userProcessInfoPtr->SendMsg(0,strMsg);
		}
		else
		{
			Log(LOG_INFO, nullptr
				, "fun=StartTradeInstance;msg=start has stopped trade instance;key=gateway;instancekey=%s"
				, strKey.c_str());

			//启动进程
			flag = userProcessInfoPtr->StartProcess();
			if (!flag)
			{
				Log(LOG_ERROR, nullptr
					, "fun=StartTradeInstance;msg=can not start up user process;key=%s"					
					, strKey.c_str());
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
	Log(LOG_INFO, nullptr
		, "fun=TryStopTradeInstance;msg=try to stop trade instance;key=gateway");

	req_start_trade_key_map rsckMap;
	if (!GetReqStartTradeKeyMap(rsckMap))
	{
		return;
	}

	if (rsckMap.empty())
	{
		return;
	}

	for (auto a : rsckMap)
	{
		const std::string& strKey = a.first;
		req_start_trade_instance& req_start_trade = a.second;
		req_start_trade.aid = "req_stop_ctp";

		Log(LOG_INFO, nullptr
			, "fun=TryStopTradeInstance;msg=stop trade instance;key=gateway;instancekey=%s"
			, strKey.c_str());

		StopTradeInstance(strKey, req_start_trade);
	}
}

void trade_server::StopTradeInstance(const std::string& strKey
	, req_start_trade_instance& req_start_trade)
{
	auto it = g_config.brokers.find(req_start_trade.bid);
	if (it == g_config.brokers.end())
	{
		Log(LOG_WARNING, nullptr
			, "fun=StopTradeInstance;msg=invalid bid;key=gateway;bid=%s"
			, req_start_trade.bid.c_str());
		return;
	}

	std::string broker_type = it->second.broker_type;
	bool flag = false;
	if (broker_type == "ctp")
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
		Log(LOG_WARNING, nullptr
			, "fun=StopTradeInstance;msg=invalid bid type;key=gateway;bid=%s;bidtype=%s"
			, req_start_trade.bid.c_str()
			, broker_type.c_str());
		return;
	}

	auto userIt = g_userProcessInfoMap.find(strKey);

	//如果用户进程没有启动,直接退出
	if (userIt == g_userProcessInfoMap.end())
	{
		Log(LOG_WARNING, nullptr
			, "fun=StopTradeInstance;msg=process is not started;key=gateway;bid=%s"
			, req_start_trade.bid.c_str());
		return;
	}

	UserProcessInfo_ptr userProcessInfoPtr = userIt->second;
	//进程是否正常运行
	flag = userProcessInfoPtr->ProcessIsRunning();
	if (!flag)
	{
		Log(LOG_WARNING, nullptr
			, "fun=StopTradeInstance;msg=process is not running;key=gateway;bid=%s"
			, req_start_trade.bid.c_str());
		return;
	}
	else
	{
		Log(LOG_INFO, nullptr
			, "fun=StopTradeInstance;msg=send stop trade instance msg;key=gateway;instancekey=%s"
			, strKey.c_str());

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

		Log(LOG_INFO, nullptr
			, "fun=TryRestartProcesses;msg=try to restart dead process;key=gateway");

		if (!pro_ptr->ReStartProcess())
		{
			Log(LOG_WARNING, nullptr
				, "fun=TryRestartProcesses;msg=can not restart up %s user process;key=gateway"
				, it.first.c_str());
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

bool trade_server::LoadConditionOrderConfig(condition_order_config& tmp_co_config)
{
	SerializerConditionOrderData ss;
	if (!ss.FromFile("/etc/open-trade-gateway/config-condition-order.json"))
	{
		Log(LOG_INFO, nullptr
			, "fun=LoadConditionOrderConfig;msg=trade server load /etc/open-trade-gateway/config-condition-order.json file fail;key=gateway");
		return false;
	}
	ss.ToVar(tmp_co_config);
	return true;
}

void trade_server::NotifyConditionOrderServerStatus()
{
	Log(LOG_INFO, nullptr
		, "fun=NotifyConditionOrderServerStatus;msg=nofity sub process to change cos status;key=gateway");

	SerializerConditionOrderData ss;
	req_ccos_status req;
	req.run_server = _co_config.run_server;
	ss.FromVar(req);
	std::string msg;
	ss.ToString(&msg);
	//给子进程发消息
	for (auto it : g_userProcessInfoMap)
	{
		Log(LOG_INFO, nullptr
			, "fun=NotifyConditionOrderServerStatus;msg=nofity sub process to change cos status;key=gateway;subprocesskey=%s"
		, it.first.c_str());

		UserProcessInfo_ptr& pro_ptr = it.second;
		if (nullptr != pro_ptr)
		{
			pro_ptr->SendMsg(0,msg);
		}
	}
}