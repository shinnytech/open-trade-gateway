/////////////////////////////////////////////////////////////////////////
///@file trade_server.cpp
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "trade_server.h"
#include "log.h"
#include "config.h"
#include "user_process_info.h"

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
{
}

bool trade_server::init()
{	
	boost::system::error_code ec;

	//Open the acceptor
	acceptor_.open(_endpoint.protocol(), ec);
	if (ec)
	{
		Log(LOG_ERROR, NULL, 
			"trade server acceptor open fail, ec=%s"
			, ec.message().c_str());
		return false;
	}

	//Allow address reuse
	acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec)
	{
		Log(LOG_ERROR, NULL
			, "trade server acceptor set option fail, ec=%s"
			, ec.message().c_str());
		return false;
	}

	// Bind to the server address
	acceptor_.bind(_endpoint, ec);
	if (ec)
	{
		Log(LOG_ERROR, NULL
			, "trade server acceptor bind fail, ec=%s"
			, ec.message().c_str());
		return false;
	}

	//Start listening for connections
	acceptor_.listen(boost::asio::socket_base::max_listen_connections
		, ec);
	if (ec)
	{
		Log(LOG_ERROR, NULL
			, "trade server acceptor listen fail, ec=%s"
			, ec.message().c_str());
		return false;
	}

	do_accept();

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
		Log(LOG_WARNING, NULL
			, "trade_server accept error, ec=%s"
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


