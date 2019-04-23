/////////////////////////////////////////////////////////////////////////
///@file trade_server.h
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "connection_manager.h"

#include <boost/asio.hpp>

class trade_server
{
public:
	explicit trade_server(boost::asio::io_context& ios,int port);

	trade_server(const trade_server&) = delete;

	trade_server& operator=(const trade_server&) = delete;

	bool init();
		
	void stop();
private:	
	void do_accept();

	void OnAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);

	boost::asio::io_context& io_context_;

	boost::asio::ip::tcp::endpoint _endpoint;
	
	boost::asio::ip::tcp::acceptor acceptor_;

	connection_manager connection_manager_;

	int _connection_id;
};