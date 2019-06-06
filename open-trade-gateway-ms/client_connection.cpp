/////////////////////////////////////////////////////////////////////////
///@file client_connection.cpp
///@brief	websocket连接类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "client_connection.h"
#include "client_connection_manager.h"
#include "log.h"
#include "ms_config.h"
#include "SerializerTradeBase.h"

#include <iostream>

client_connection::client_connection(
	boost::asio::io_context& ios
	,boost::asio::ip::tcp::socket socket
	,client_connection_manager& manager
	,int connection_id
	,TBrokerSlaveNodeMap& broker_slave_node_Map
	,std::string& broker_list_str)
	:m_ios(ios),
	m_ws_socket(std::move(socket)),		
	m_input_buffer(),
	m_output_buffer(),
	client_connection_manager_(manager),
	_connection_id(connection_id),	
	flat_buffer_(),
	req_(),
	_X_Real_IP(""),
	_X_Real_Port(""),
	m_broker_slave_node_Map(broker_slave_node_Map),
	m_broker_list_str(broker_list_str),
	m_connect_to_server(false),
	m_ws_socket_to_server(ios),
	m_resolver(ios),
	m_input_buffer_from_server(),
	m_output_buffer_to_server(),
	m_cached_msgs(),
	m_last_req_login(),
	m_last_slave_node()
{
}

void client_connection::start()
{	
	req_ = {};
	boost::beast::http::async_read(m_ws_socket.next_layer()
		,flat_buffer_
		,req_
		,boost::beast::bind_front_handler(
			&client_connection::on_read_header,
			shared_from_this()));	
}

void client_connection::on_read_header(boost::beast::error_code ec
	, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	if (ec)
	{
		LogMs(LOG_INFO,nullptr
			, "msg=open trade gateway master client connection on_read_header fail;errmsg=%s;key=gatewayms"
			, ec.message().c_str());
		OnCloseConnection();
		return;
	}

	m_ws_socket.async_accept(
		req_,
		boost::beast::bind_front_handler(
			&client_connection::OnOpenConnection,
			shared_from_this()));
}

void client_connection::OnOpenConnection(boost::system::error_code ec)
{
	if (ec)
	{
		LogMs(LOG_WARNING, nullptr
			, "msg=open trade gateway master client connection accept fail;errmsg=%s;key=gatewayms"
			, ec.message().c_str());
		OnCloseConnection();
		return;
	}

	try
	{
		boost::asio::ip::tcp::endpoint remote_ep = m_ws_socket.next_layer().remote_endpoint();
		
		_X_Real_IP = req_["X-Real-IP"].to_string();
		if (_X_Real_IP.empty())
		{
			_X_Real_IP = remote_ep.address().to_string();
		}

		std::string real_port = req_["X-Real-Port"].to_string();
		if (real_port.empty())
		{
			_X_Real_Port = std::to_string(remote_ep.port());
		}
		else
		{
			_X_Real_Port = real_port.c_str();
		}

		SendTextMsg(m_broker_list_str);

		DoRead();
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_INFO,nullptr
			, "msg=open trade gateway master client connection OnOpenConnection exception;errmsg=%s;key=gatewayms"
			, ex.what());
	}
}

void client_connection::DoRead()
{
	m_ws_socket.async_read(
		m_input_buffer,
		boost::beast::bind_front_handler(
			&client_connection::OnRead,
			shared_from_this()));
}

void client_connection::OnRead(boost::system::error_code ec, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	if (ec)
	{
		LogMs(LOG_INFO, nullptr
			, "msg=open trade gateway master client connection read fail;errmsg=%s;key=gatewayms"
			, ec.message().c_str());
		OnCloseConnection();
		return;
	}
	OnMessage(boost::beast::buffers_to_string(m_input_buffer.data()));
	m_input_buffer.consume(m_input_buffer.size());
	DoRead();
}

void client_connection::SendTextMsg(const std::string& msg)
{
	try
	{
		if (m_output_buffer.size() > 0)
		{
			m_output_buffer.push_back(msg);
		}
		else
		{
			m_output_buffer.push_back(msg);
			DoWrite();
		}
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_ERROR,nullptr
			, "msg=open trade gateway master client connection SendTextMsg exception;errmsg=%s;key=gatewayms"
			, ex.what());
	}
}

void client_connection::DoWrite()
{
	try
	{
		if (m_output_buffer.empty())
		{
			return;
		}
		auto write_buf = boost::asio::buffer(m_output_buffer.front());
		m_ws_socket.text(true);
		m_ws_socket.async_write(
			write_buf,
			boost::beast::bind_front_handler(
				&client_connection::OnWrite,
				shared_from_this()));
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_ERROR, nullptr
			, "msg=open trade gateway master client connection DoWrite exception;errmsg=%s;key=gatewayms"
			, ex.what());
	}
}

void client_connection::OnWrite(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec)
	{
		LogMs(LOG_INFO,nullptr
			, "msg=open trade gateway master client connection send message fail;errmsg=%s;key=gatewayms"
			, ec.message().c_str());
		OnCloseConnection();
		return;
	}
	if (m_output_buffer.empty())
	{
		return;
	}
	m_output_buffer.pop_front();
	if (m_output_buffer.size() > 0)
	{
		DoWrite();
	}
}

void client_connection::OutputNotifySycn(long notify_code
	, const std::string& notify_msg, const char* level
	, const char* type)
{
	//构建数据包
	SerializerTradeBase nss;
	rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");
	rapidjson::Value node_message;
	node_message.SetObject();
	node_message.AddMember("type", rapidjson::Value(type, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	node_message.AddMember("level", rapidjson::Value(level, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	node_message.AddMember("code", notify_code, nss.m_doc->GetAllocator());
	node_message.AddMember("content", rapidjson::Value(notify_msg.c_str(), nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/notify/N" + std::to_string(0)).Set(*nss.m_doc, node_message);
	std::string json_str;
	nss.ToString(&json_str);
	SendTextMsg(json_str);
}

void client_connection::OnMessage(const std::string &json_str)
{
	SerializerTradeBase ss;
	if (!ss.FromString(json_str.c_str()))
	{
		LogMs(LOG_INFO, nullptr
			,"msg=receive invalide msg from client;key=gatewayms");
		return;
	}

	ReqLogin req;
	ss.ToVar(req);

	if (req.aid == "req_login")
	{
		ProcessLogInMessage(req,json_str);
	}
	else
	{
		ProcessOtherMessage(json_str);
	}
}

void client_connection::ProcessLogInMessage(const ReqLogin& req
	, const std::string &json_str)
{
	//如果请求登录的bid不存在
	TBrokerSlaveNodeMap::iterator it = m_broker_slave_node_Map.find(req.bid);
	if (it == m_broker_slave_node_Map.end())
	{
		LogMs(LOG_WARNING, nullptr
			, "msg=open trade gateway master get invalid bid;bid=%s;key=gatewayms"
			, req.bid.c_str());
		std::stringstream ss;
		ss << u8"暂不支持:" << req.bid << u8",请联系该期货公司或快期技术支持人员!";
		OutputNotifySycn(1, ss.str(), "WARNING");
		return;
	}

	SlaveNodeInfo slaveNodeInfo = it->second;

	//如果已经连接到服务器
	if (m_connect_to_server)
	{
		//已经登录的连接,直接关掉
		std::stringstream ss;
		ss << u8"重复登录!";
		OutputNotifySycn(1, ss.str(), "WARNING");
		OnCloseConnection();
		return;
	}
	//如果还没有连接到服务器
	else
	{
		m_cached_msgs.clear();
		m_last_req_login = req;
		m_last_slave_node = slaveNodeInfo;

		//尝试连接到slave
		m_cached_msgs.push_back(json_str);
		start_connect_to_server();
	}
}

void client_connection::start_connect_to_server()
{
	m_resolver.async_resolve(
		m_last_slave_node.host,
		m_last_slave_node.port,
		std::bind(&client_connection::OnResolve,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2));
}

void client_connection::OnResolve(boost::system::error_code ec
	, boost::asio::ip::tcp::resolver::results_type results)
{
	if (ec)
	{
		LogMs(LOG_ERROR,nullptr
			,"msg=open trade gateway master OnResolve Slave node;SlaveNode=%s;bid=%s;errmsg=%s;key=gatewayms"
			, m_last_slave_node.name.c_str()
			, m_last_req_login.bid.c_str()
			, ec.message().c_str());
		std::stringstream ss;
		ss << u8"服务器暂时不可用,请联系快期技术支持人员!";
		OutputNotifySycn(1,ss.str(),"WARNING");
		return;
	}

	boost::asio::async_connect(
		m_ws_socket_to_server.next_layer(),
		results.begin(),
		results.end(),
		std::bind(
			&client_connection::OnConnectToServer,
			shared_from_this(),
			std::placeholders::_1));
}

void client_connection::OnConnectToServer(boost::system::error_code ec)
{
	if (ec)
	{
		LogMs(LOG_ERROR,nullptr
			, "msg=open trade gateway master OnConnectToServer Slave node;SlaveNode=%s;bid=%s;errmsg=%s;key=gatewayms"
			, m_last_slave_node.name.c_str()
			, m_last_req_login.bid.c_str()
			, ec.message().c_str());
		std::stringstream ss;
		ss << u8"服务器暂时不可用,请联系快期技术支持人员!";
		OutputNotifySycn(1, ss.str(), "WARNING");
		return;
	}

	//Perform the websocket handshake
	m_ws_socket_to_server.set_option(boost::beast::websocket::stream_base::decorator(
		[&](boost::beast::websocket::request_type& m)
	{
		m.insert(boost::beast::http::field::accept, "application/v1+json");
		m.insert(boost::beast::http::field::user_agent, "OTG-1.1.0.0");
		m.insert("X-Real-IP",_X_Real_IP);
		m.insert("X-Real-Port",_X_Real_Port);		
	}));
	m_ws_socket_to_server.async_handshake(m_last_slave_node.host
		,m_last_slave_node.path,
		boost::beast::bind_front_handler(
			&client_connection::OnHandshakeWithServer,
			shared_from_this()));
}

void client_connection::OnHandshakeWithServer(boost::system::error_code ec)
{
	if (ec)
	{
		LogMs(LOG_ERROR,nullptr
			, "msg=open trade gateway master OnHandshakeWithServer Slave node;SlaveNode=%s;bid=%s;errmsg=%s;key=gatewayms"
			, m_last_slave_node.name.c_str()
			, m_last_req_login.bid.c_str()
			, ec.message().c_str());
		std::stringstream ss;
		ss << u8"服务器暂时不可用,请快期技术支持人员!";
		OutputNotifySycn(1, ss.str(), "WARNING");
		return;
	}

	//成功连接上slaveNode
	m_connect_to_server = true;

	//将缓存的消息全部发出
	for (auto msg : m_cached_msgs)
	{
		SendTextMsgToServer(msg);
	}
	m_cached_msgs.clear();

	//从服务器读数据
	DoReadFromServer();
}

void client_connection::DoReadFromServer()
{
	m_ws_socket_to_server.async_read(
		m_input_buffer_from_server,
		boost::beast::bind_front_handler(
			&client_connection::OnReadFromServer,
			shared_from_this()));
}

void client_connection::OnReadFromServer(boost::system::error_code ec
	, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	if (ec)
	{
		LogMs(LOG_INFO,nullptr
			, "msg=open trade gateway master OnReadFromServer Slave node;SlaveNode=%s;bid=%s;errmsg=%s;key=gatewayms"
			,m_last_slave_node.name.c_str()
			,m_last_req_login.bid.c_str()
			,ec.message().c_str());
		//关闭整个连接,要求客户端重新登录
		OnCloseConnection();
		return;
	}
	OnTextMsgFromServer(boost::beast::buffers_to_string(m_input_buffer_from_server.data()));
	m_input_buffer_from_server.consume(m_input_buffer_from_server.size());
	DoReadFromServer();
}

void client_connection::OnTextMsgFromServer(const std::string& msg)
{
	MasterSerializerConfig ss;
	if (!ss.FromString(msg.c_str()))
	{
		LogMs(LOG_INFO,nullptr
			, "msg=receive invalide msg from server;key=gatewayms");
		return;
	}

	RtnBrokersMsg req;
	ss.ToVar(req);

	if (req.aid == "rtn_brokers")
	{
		return;
	}

	//LogMs(LOG_INFO
	//	, msg.c_str()
	//	, "fun=OnTextMsgFromServer;key=gatewayms");

	//发到客户端
	SendTextMsg(msg);
}

void client_connection::DoWriteToServer()
{
	try
	{
		if (m_output_buffer_to_server.empty())
		{
			return;
		}
		auto write_buf = boost::asio::buffer(m_output_buffer_to_server.front());
		m_ws_socket_to_server.text(true);
		m_ws_socket_to_server.async_write(
			write_buf,
			boost::beast::bind_front_handler(
				&client_connection::OnWriteServer,
				shared_from_this()));
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_ERROR,nullptr
			, "msg=open trade gateway master client connection DoWriteToServer exception;errmsg=%s;key=gatewayms"
			, ex.what());
	}
}

void client_connection::OnWriteServer(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec)
	{
		LogMs(LOG_INFO,nullptr
			, "msg=open trade gateway master client connection send message to server fail;errmsg=%s;key=gatewayms"
			, ec.message().c_str());

		//关闭整个连接,要求客户端重新登录
		OnCloseConnection();
		return;
	}

	if (m_output_buffer_to_server.empty())
	{
		return;
	}
	m_output_buffer_to_server.pop_front();
	if (m_output_buffer_to_server.size() > 0)
	{
		DoWriteToServer();
	}
}

void client_connection::SendTextMsgToServer(const std::string& msg)
{
	try
	{
		//LogMs(LOG_INFO
		//	, msg.c_str()
		//	, "fun=SendTextMsgToServer;key=gatewayms");

		if (m_output_buffer_to_server.size() > 0)
		{
			m_output_buffer_to_server.push_back(msg);
		}
		else
		{
			m_output_buffer_to_server.push_back(msg);
			DoWriteToServer();
		}
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_ERROR,nullptr
			, "msg=open trade gateway master client connection SendTextMsgToServer exception;errmsg=%s;key=gatewayms"
			,ex.what());
	}
}



void client_connection::ProcessOtherMessage(const std::string &json_str)
{
	if (m_connect_to_server)
	{
		SendTextMsgToServer(json_str);
	}
	else
	{
		m_cached_msgs.push_back(json_str);
	}	
}

void client_connection::OnCloseConnection()
{	
	try
	{
		//m_connect_to_server = false;
		client_connection_manager_.stop(shared_from_this());
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_ERROR,nullptr
			,"msg=client_connection::OnCloseConnection();errmsg=%s;key=gatewayms"
			, ex.what());
	}	
}

void client_connection::stop()
{
	try
	{
		LogMs(LOG_INFO, nullptr
			, "msg=client_connection stop;key=gatewayms");

		//关掉客户端连接
		LogMs(LOG_INFO, nullptr
			, "msg=client_connection stop m_ws_socket;key=gatewayms");
		m_ws_socket.next_layer().close();
				
		//关掉服务端连接
		LogMs(LOG_INFO, nullptr
			, "msg=client_connection stop m_ws_socket_to_server;key=gatewayms");
		m_ws_socket_to_server.next_layer().close();
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_INFO,nullptr
			, "msg=open trade gateway master connection stop exception;errmsg=%s;key=gatewayms"
			, ex.what());
	}
}