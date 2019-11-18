/////////////////////////////////////////////////////////////////////////
///@file connection.h
///@brief	websocket连接类
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

class connection_manager;

class connection
	: public std::enable_shared_from_this<connection>
{
public:
	explicit connection(boost::asio::io_context& ios
		,boost::asio::ip::tcp::socket socket,
		connection_manager& manager
		,int connection_id);

	connection(const connection&) = delete;

	connection& operator=(const connection&) = delete;

	void start();

	void stop();

	int connection_id()
	{
		return _connection_id;
	}

	void SendTextMsg(const std::string& msg);
private:	
	void OnOpenConnection(boost::system::error_code ec);

	void DoRead();

	void DoWrite();

	void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);

	void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);
		
	bool OnMessage(const std::string &json_str);	

	void DelayProcessInMsg(std::shared_ptr<std::string> msg_ptr);

	bool ProcessLogInMessage(const ReqLogin& req,const std::string &json_str);

	bool ProcessOtherMessage(const std::string &json_str);

	void OnCloseConnection();

	void on_read_header(boost::beast::error_code ec
		, std::size_t bytes_transferred);
	
	boost::asio::io_context& m_ios;
		
	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_ws_socket;
		
	boost::beast::multi_buffer m_input_buffer;

	std::list<std::string> m_output_buffer;

	connection_manager& connection_manager_;	

	int _connection_id;	
	
	ReqLogin _reqLogin;

	std::string _user_broker_key;

	std::string _login_msg;

	boost::beast::flat_buffer flat_buffer_;

	boost::beast::http::request<boost::beast::http::string_body> req_;	

	std::string _X_Real_IP;

	int _X_Real_Port;

	std::string _agent;

	std::string _analysis;

	std::vector<std::string> _msg_cache;

	boost::asio::deadline_timer m_timer;
};

typedef std::shared_ptr<connection> connection_ptr;

