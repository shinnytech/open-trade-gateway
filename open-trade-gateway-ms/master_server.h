/////////////////////////////////////////////////////////////////////////
///@file master_server.h
///@brief	交易代理服务器
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "ms_config.h"
#include "client_connection_manager.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

class master_server
{
public:
	master_server(boost::asio::io_context& ios
		,MasterConfig& masterConfig);

	master_server(const master_server&) = delete;

	master_server& operator=(const master_server&) = delete;

	bool init();
		
	void stop();
private:	
	bool InitClientAcceptor();

	void do_client_accept();

	void OnClientAccept(boost::system::error_code ec
		, boost::asio::ip::tcp::socket socket);

	boost::asio::io_context& io_context_;

	MasterConfig& masterConfig_;

	boost::asio::ip::tcp::endpoint client_endpoint_;
	
	boost::asio::ip::tcp::acceptor client_acceptor_;

	int client_connection_;

	client_connection_manager client_connection_manager_;
};
