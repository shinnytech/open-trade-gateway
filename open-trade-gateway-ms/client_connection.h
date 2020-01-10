/////////////////////////////////////////////////////////////////////////
///@file client_connection.h
///@brief	websocket连接类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"
#include "ms_config.h"

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

class client_connection_manager;

class client_connection
	: public std::enable_shared_from_this<client_connection>
{
public:
	client_connection(boost::asio::io_context& ios
		,boost::asio::ip::tcp::socket socket
		,client_connection_manager& manager
		,int connection_id
		,MasterConfig& masterConfig);

	~client_connection();

	client_connection(const client_connection&) = delete;

	client_connection& operator=(const client_connection&) = delete;

	void start();

	void stop();

	void stop_server();

	int connection_id()
	{
		return _connection_id;
	}	
private:	
	void OnOpenConnection(boost::system::error_code ec);

	void DoRead();

	void DoWrite();

	void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);

	void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);
		
	void OnMessage(const std::string &json_str);	

	void ProcessLogInMessage(const ReqLogin& req,const std::string &json_str);
	
	void OnCloseConnection();

	void on_read_header(boost::beast::error_code ec
		, std::size_t bytes_transferred);
	
	void SendTextMsg(const std::string& msg);

	void start_connect_to_server();

	void OnResolve(boost::system::error_code ec
		, boost::asio::ip::tcp::resolver::results_type results);

	void OnConnectToServer(boost::system::error_code ec);

	void OnHandshakeWithServer(boost::system::error_code ec);

	void SendTextMsgToServer(const std::string& msg);

	void OnTextMsgFromServer(const std::string& msg);

	void DoReadFromServer();

	void OnReadFromServer(boost::system::error_code ec
		, std::size_t bytes_transferred);

	void DoWriteToServer();

	void OnWriteServer(boost::system::error_code ec, std::size_t bytes_transferred);

	std::string GetUserKey(const ReqLogin& req,const BrokerConfig& brokerConfig);

	bool GetSlaveNodeInfoFromUserKey(const std::string& bid,const std::string& userKey,SlaveNodeInfo& slaveNodeInfo);

	void SaveMsUsersConfig(const std::string& userKey);

	boost::asio::io_context& m_ios;
			
	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_ws_socket;
		
	boost::beast::multi_buffer m_input_buffer;

	std::list<std::string> m_output_buffer;

	client_connection_manager& client_connection_manager_;

	int _connection_id;	
	
	boost::beast::flat_buffer flat_buffer_;

	boost::beast::http::request<boost::beast::http::string_body> req_;	

	std::string _X_Real_IP;

	std::string _X_Real_Port;

	std::string _agent;

	std::string _analysis;

	MasterConfig& _masterConfig;

	bool m_connect_to_server;	

	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_ws_socket_to_server;

	boost::asio::ip::tcp::resolver m_resolver;

	boost::beast::multi_buffer m_input_buffer_from_server;

	std::list<std::string> m_output_buffer_to_server;

	std::vector<std::string> m_cached_msgs;

	ReqLogin m_last_req_login;

	SlaveNodeInfo m_last_slave_node;
};

typedef std::shared_ptr<client_connection> client_connection_ptr;

