/////////////////////////////////////////////////////////////////////////
///@file master_server.cpp
///@brief	交易代理服务器
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "master_server.h"
#include "SerializerTradeBase.h"
#include "log.h"

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
	,m_broker_slave_node_Map()
	,m_broker_list_str("")
{
}

bool master_server::init()
{	
	if (!GetSlaveBrokerList())
	{
		LogMs.WithField("msg", "open trade gateway master GetSlaveBrokerList fail,check if slave node is start up!").WithField("key", "gatewayms").Write(LOG_ERROR);
		return false;
	}

	if (!InitClientAcceptor())
	{
		LogMs.WithField("msg", "open trade gateway master InitClientAcceptor fail!").WithField("key", "gatewayms").Write(LOG_ERROR);
		return false;
	}

	return true;
}

bool master_server::GetSlaveBrokerList()
{
	for (auto node : masterConfig_.slaveNodeList)
	{
		try
		{
			WebSocketClient socketClient(node);
			socketClient.start();
			std::string strBrokerList = socketClient.WaitBrokerList();
			if (strBrokerList.empty())
			{
				LogMs.WithField("msg", "open trade gateway master can not get broker list from slave node!").WithField("nodename", node.name.c_str()).WithField("key", "gatewayms").Write(LOG_INFO);
				continue;
			}

			MasterSerializerConfig ss;
			if (!ss.FromString(strBrokerList.c_str()))
			{
				LogMs.WithField("msg", "open trade gateway master GetSlaveBrokerList is not json!").WithField("brokerlist", strBrokerList.c_str()).WithField("key", "gatewayms").WithField("nodename", node.name.c_str()).Write(LOG_INFO);
				continue;
			}

			RtnBrokersMsg rtnBroker;
			ss.ToVar(rtnBroker);

			if (rtnBroker.aid != "rtn_brokers")
			{
				LogMs.WithField("msg", "open trade gateway master GetSlaveBrokerList is not rtn_brokers!").WithField("nodename", node.name.c_str()).WithField("key", "gatewayms").WithField("pack", strBrokerList.c_str()).Write(LOG_INFO);
				continue;
			}

			for (auto bid : rtnBroker.brokers)
			{
				TBrokerSlaveNodeMap::iterator it = m_broker_slave_node_Map.find(bid);
				if (it == m_broker_slave_node_Map.end())
				{
					m_broker_slave_node_Map.insert(TBrokerSlaveNodeMap::value_type(bid,node));
				}
			}
		}
		catch (std::exception const& ex)
		{
			LogMs.WithField("msg", "open trade gateway master GetSlaveBrokerList fail!").WithField("errmsg", ex.what()).WithField("key", "gatewayms").WithField("nodename", node.name.c_str()).Write(LOG_WARNING);
			continue;
		}
	}

	if (m_broker_slave_node_Map.empty())
	{
		LogMs.WithField("msg", "open trade gateway master can not get slave broker list!").WithField("key", "gatewayms").Write(LOG_WARNING);
		return false;
	}
	
	SerializerTradeBase ss_broker_list_str;
	rapidjson::Pointer("/aid").Set(*ss_broker_list_str.m_doc, "rtn_brokers");
	int i = 0;
	for (auto b : m_broker_slave_node_Map)
	{
		std::string brokerName = b.first;
		rapidjson::Pointer("/brokers/" + std::to_string(i)).Set(*ss_broker_list_str.m_doc,brokerName);
		i++;
	}
	ss_broker_list_str.ToString(&m_broker_list_str);

	return true;
}

bool  master_server::InitClientAcceptor()
{
	boost::system::error_code ec;
	client_acceptor_.open(client_endpoint_.protocol(),ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master client acceptor open fail").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").Write(LOG_WARNING);
		return false;
	}

	client_acceptor_.set_option(boost::asio::socket_base::reuse_address(true),ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master client acceptor set option fail").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").Write(LOG_WARNING);
		return false;
	}

	client_acceptor_.bind(client_endpoint_,ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master client acceptor bind fail").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").Write(LOG_WARNING);
		return false;
	}

	client_acceptor_.listen(boost::asio::socket_base::max_listen_connections,ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master client acceptor listen fail").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").Write(LOG_WARNING);
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
		LogMs.WithField("msg", "open trade gateway master client acceptor is not open!").WithField("key", "gatewayms").Write(LOG_WARNING);
		return;
	}

	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master client acceptor accept error").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").Write(LOG_WARNING);
		do_client_accept();
		return;
	}
		
	client_connection_manager_.start(std::make_shared<client_connection>(
		io_context_
		,std::move(socket)
		,client_connection_manager_
		,++client_connection_
		,m_broker_slave_node_Map
		,m_broker_list_str));		

	do_client_accept();
}

void master_server::stop()
{
	client_acceptor_.close();
	client_connection_manager_.stop_all();
}


WebSocketClient::WebSocketClient(SlaveNodeInfo& slaveNodeInfo)
	:_ios()
	,_worker(_ios)
	,_timer(_ios)
	,_slaveNodeInfo(slaveNodeInfo)
	,m_ws_socket(_ios)
	,m_input_buffer()
	,m_resolver(_ios)
	,m_broker_list("")
	,m_thead(boost::bind(&boost::asio::io_service::run,&_ios))
{
}

void WebSocketClient::start()
{
	m_resolver.async_resolve(
		_slaveNodeInfo.host,
		_slaveNodeInfo.port,
		std::bind(&WebSocketClient::OnResolve,
			this, 
			std::placeholders::_1,
			std::placeholders::_2));
	_timer.expires_from_now(boost::posix_time::seconds(5));
	_timer.async_wait(boost::bind(&WebSocketClient::OnTimeOut,this));
}

void WebSocketClient::OnResolve(boost::system::error_code ec
	, boost::asio::ip::tcp::resolver::results_type results)
{
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master resolve node host fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
		OnError();
		return;
	}

	boost::asio::async_connect(
		m_ws_socket.next_layer(),
		results.begin(),
		results.end(),
		std::bind(
			&WebSocketClient::OnConnect,
			this,
			std::placeholders::_1));
}

void WebSocketClient::OnConnect(boost::system::error_code ec)
{
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master connect node host fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
		OnError();
		return;
	}

	m_ws_socket.set_option(boost::beast::websocket::stream_base::decorator(
		[](boost::beast::websocket::request_type& m)
	{
		m.insert(boost::beast::http::field::accept, "application/v1+json");
		m.insert(boost::beast::http::field::user_agent, "OTG-MS-1.1.0.0");
	}));

	m_ws_socket.async_handshake(
		_slaveNodeInfo.host
		,_slaveNodeInfo.path
		,boost::beast::bind_front_handler(
			&WebSocketClient::OnHandshake,
			this));
}

void WebSocketClient::OnHandshake(boost::system::error_code ec)
{
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master handshake with node host fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
		OnError();
		return;
	}
	DoRead();
}

void WebSocketClient::DoRead()
{
	m_ws_socket.async_read(
		m_input_buffer,
		boost::beast::bind_front_handler(
			&WebSocketClient::OnRead,
			this));
}

void WebSocketClient::OnRead(boost::system::error_code ec
	,std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master read broker list from node host fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
		OnError();
		return;
	}
	m_broker_list = boost::beast::buffers_to_string(m_input_buffer.data());
	m_input_buffer.consume(m_input_buffer.size());
	OnFinish();
}

std::string WebSocketClient::WaitBrokerList()
{
	m_thead.join();
	return m_broker_list;
}

void WebSocketClient::OnTimeOut()
{
	boost::beast::error_code ec;
	m_ws_socket.close(boost::beast::websocket::close_code::normal,ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master clsoe websocket client fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
	}

	_ios.stop();
}

void WebSocketClient::OnError()
{
	boost::system::error_code ec;
	_timer.cancel(ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master cancel timer fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
	}

	boost::beast::error_code ec2;
	m_ws_socket.close(boost::beast::websocket::close_code::normal,ec2);
	if (ec2)
	{
		LogMs.WithField("msg", "open trade gateway master clsoe websocket client fail!").WithField("errmsg", ec2.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
	}

	_ios.stop();
}

void WebSocketClient::OnFinish()
{
	boost::system::error_code ec;
	_timer.cancel(ec);
	if (ec)
	{
		LogMs.WithField("msg", "open trade gateway master cancel timer fail!").WithField("errmsg", ec.message().c_str()).WithField("key", "gatewayms").WithField("nodename", _slaveNodeInfo.name.c_str()).Write(LOG_WARNING);
	}

	_ios.stop();
}