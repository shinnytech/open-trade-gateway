/////////////////////////////////////////////////////////////////////////
///@file master_server.cpp
///@brief	交易代理服务器
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "master_server.h"
#include "SerializerTradeBase.h"
#include "log.h"
#include "version.h"

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

master_server::master_server(boost::asio::io_context& ios
	, MasterConfig& masterConfig)
	:io_context_(ios)
	,masterConfig_(masterConfig)
	,client_endpoint_(boost::asio::ip::tcp::v4(),masterConfig.port)
	,client_acceptor_(io_context_)
	,client_connection_(0)
	,client_connection_manager_()	
{
}

bool master_server::init()
{	
	if (!InitClientAcceptor())
	{
		LogMs().WithField("fun","init")
			.WithField("key","gatewayms")
			.Log(LOG_ERROR, "open trade gateway master InitClientAcceptor fail!");		
		return false;
	}

	return true;
}

bool  master_server::InitClientAcceptor()
{
	boost::system::error_code ec;
	client_acceptor_.open(client_endpoint_.protocol(),ec);
	if (ec)
	{
		LogMs().WithField("fun","InitClientAcceptor")
			.WithField("key","gatewayms")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master client acceptor open fail");		
		return false;
	}

	client_acceptor_.set_option(boost::asio::socket_base::reuse_address(true),ec);
	if (ec)
	{
		LogMs().WithField("fun","InitClientAcceptor")
			.WithField("key","gatewayms")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master client acceptor set option fail");	
		return false;
	}

	client_acceptor_.bind(client_endpoint_,ec);
	if (ec)
	{
		LogMs().WithField("fun","InitClientAcceptor")
			.WithField("key","gatewayms")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master client acceptor bind fail");		
		return false;
	}

	client_acceptor_.listen(boost::asio::socket_base::max_listen_connections,ec);
	if (ec)
	{
		LogMs().WithField("fun","InitClientAcceptor")
			.WithField("key","gatewayms")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master client acceptor listen fail");	
		return false;
	}

	do_client_accept();

	return true;
}

void master_server::do_client_accept()
{
	client_acceptor_.async_accept(
		std::bind(&master_server::OnClientAccept,
			this,
			std::placeholders::_1,
			std::placeholders::_2));
}

void master_server::OnClientAccept(boost::system::error_code ec
	, boost::asio::ip::tcp::socket socket)
{
	if (!client_acceptor_.is_open())
	{
		LogMs().WithField("fun","OnClientAccept")
			.WithField("key","gatewayms")		
			.Log(LOG_WARNING,"open trade gateway master client acceptor is not open!");		
		return;
	}

	if (ec)
	{
		LogMs().WithField("fun","OnClientAccept")
			.WithField("key","gatewayms")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master client acceptor accept error");
				
		do_client_accept();
		return;
	}
		
	client_connection_manager_.start(std::make_shared<client_connection>(
		io_context_
		,std::move(socket)
		,client_connection_manager_
		,++client_connection_
		,masterConfig_));

	do_client_accept();
}

void master_server::stop()
{
	client_acceptor_.close();
	client_connection_manager_.stop_all();
}
