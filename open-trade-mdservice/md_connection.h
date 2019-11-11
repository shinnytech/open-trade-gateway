/////////////////////////////////////////////////////////////////////////
///@file md_connection.h
///@brief	行情连接管理
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

#include <array>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>

class mdservice;

class md_connection
	: public std::enable_shared_from_this<md_connection>
{
public:
	md_connection(boost::asio::io_context& ios
		,const std::string& req_subscribe_quote
		,const std::string& req_peek_message
		,InsMapType* ins_map
		,mdservice& mds);

	~md_connection();

	md_connection(const md_connection&) = delete;

	md_connection& operator=(const md_connection&) = delete;

	void Start();

	void Stop();
private:
	boost::asio::io_context& io_context_;

	std::string m_req_subscribe_quote;

	std::string m_req_peek_message;

	InsMapType* m_ins_map;

	mdservice& m_mds;

	boost::asio::ip::tcp::resolver m_resolver;

	boost::beast::websocket::stream<boost::asio::ip::tcp::socket,true> m_ws_socket;

	boost::beast::multi_buffer m_input_buffer;

	std::list<std::string> m_output_buffer;	

	bool m_connect_to_server;
	
	void DoResolve();

	void OnResolve(boost::system::error_code ec
		, boost::asio::ip::tcp::resolver::results_type results);

	void OnConnect(boost::system::error_code ec);

	void OnHandshake(boost::system::error_code ec);

	void SendTextMsg(const std::string &msg);

	void DoRead();

	void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);

	void DoWrite();

	void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);

	void OnMessage(const std::string &json_str);

	void OnConnectionnClose();

	void OnConnectionnError();
};

typedef std::shared_ptr<md_connection> md_connection_ptr;