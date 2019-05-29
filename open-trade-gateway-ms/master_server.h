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
	bool GetSlaveBrokerList();

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

	TBrokerSlaveNodeMap m_broker_slave_node_Map;

	std::string m_broker_list_str;
};

class WebSocketClient
{
public:
	WebSocketClient(SlaveNodeInfo& slaveNodeInfo);

	void start();

	std::string WaitBrokerList();
private:
	void OnResolve(boost::system::error_code ec
		, boost::asio::ip::tcp::resolver::results_type results);

	void OnConnect(boost::system::error_code ec);

	void OnHandshake(boost::system::error_code ec);

	void DoRead();

	void OnRead(boost::system::error_code ec
		, std::size_t bytes_transferred);

	void OnTimeOut();

	void OnError();

	void OnFinish();

	boost::asio::io_context _ios;

	boost::asio::io_service::work _worker;

	boost::asio::deadline_timer _timer;

	SlaveNodeInfo& _slaveNodeInfo;

	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_ws_socket;

	boost::beast::multi_buffer m_input_buffer;
	
	boost::asio::ip::tcp::resolver m_resolver;

	std::string m_broker_list;

	boost::thread m_thead;
};

typedef std::shared_ptr<WebSocketClient> WebSocketClient_Ptr;