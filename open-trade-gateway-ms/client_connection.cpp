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
	_agent(""),
	_analysis(""),
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
	int fd=m_ws_socket.next_layer().native_handle();
	int flags = fcntl(fd, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fd, F_SETFD, flags);

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
		LogMs().WithField("fun","on_read_header")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ec.message())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"client connection on_read_header fail");		
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
		LogMs().WithField("fun","OnOpenConnection")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ec.message())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"client connection accept fail");	
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

		_agent= req_[boost::beast::http::field::user_agent].to_string();				
		_analysis= req_["analysis"].to_string();

		SendTextMsg(m_broker_list_str);

		DoRead();
	}
	catch (std::exception& ex)
	{
		LogMs().WithField("fun","OnOpenConnection")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ex.what())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"open trade gateway master client connection OnOpenConnection exception");		
	}
}

void client_connection::DoRead()
{
	try
	{
		m_ws_socket.async_read(
			m_input_buffer,
			boost::beast::bind_front_handler(
				&client_connection::OnRead,
				shared_from_this()));
	}
	catch (std::exception& ex)
	{
		LogMs().WithField("fun","DoRead")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ex.what())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"DoRead exception");		
	}
}

void client_connection::OnRead(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec)
	{
		LogMs().WithField("fun","OnRead")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ec.message())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"client connection read fail");

		OnCloseConnection();
		return;
	}
	std::string strMsg = boost::beast::buffers_to_string(m_input_buffer.data());
	m_input_buffer.consume(bytes_transferred);
	OnMessage(strMsg);
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
		LogMs().WithField("fun","SendTextMsg")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ex.what())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_ERROR,"client_connection SendTextMsg exception");		
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
		LogMs().WithField("fun","DoWrite")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ex.what())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_ERROR,"client_connection DoWrite exception");		
	}
}

void client_connection::OnWrite(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec)
	{
		LogMs().WithField("fun","OnWrite")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("errmsg",ec.message())
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"client_connection OnWrite exception");	
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
		LogMs().WithField("fun","OnMessage")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)			
			.WithField("connId",_connection_id)
			.WithField("msgcontent",json_str)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"receive invalide msg from client");		
		return;
	}
		
	ReqLogin req;
	ss.ToVar(req);

	if (req.aid == "req_login")
	{
		LogMs().WithField("fun","OnMessage")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("bid",req.bid)
			.WithField("user_name",req.user_name)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_INFO,"req_login message");
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
		LogMs().WithField("fun","ProcessLogInMessage")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("bid",req.bid)
			.WithField("user_name",req.user_name)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_WARNING,"open trade gateway master get invalid bid");		
		std::stringstream ss;
		ss << u8"暂不支持," << req.bid << u8",请联系该期货公司或快期技术支持人员!";
		OutputNotifySycn(301,ss.str(), "WARNING");
		return;
	}

	SlaveNodeInfo slaveNodeInfo = it->second;

	//如果已经连接到服务器
	if (m_connect_to_server)
	{
		//如果是同一家期货公司(一定在同一个结点上)
		if (req.bid == m_last_req_login.bid)
		{
			//直接重发登录请求并返回
			LogMs().WithField("fun","ProcessLogInMessage")
				.WithField("key","gatewayms")
				.WithField("agent",_agent)
				.WithField("ip",_X_Real_IP)
				.WithField("analysis",_analysis)
				.WithField("connId",_connection_id)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
				.Log(LOG_WARNING,"same bid and last login");			
			m_last_req_login = req;
			SendTextMsgToServer(json_str);
			return;
		}

		//如果在同一个结点上
		if (slaveNodeInfo.name == m_last_slave_node.name)
		{
			//直接重发登录请求并返回
			LogMs().WithField("fun","ProcessLogInMessage")
				.WithField("key","gatewayms")
				.WithField("agent",_agent)
				.WithField("ip",_X_Real_IP)
				.WithField("analysis",_analysis)
				.WithField("connId",_connection_id)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
				.Log(LOG_INFO,"same node name and last login");			
			m_last_req_login = req;
			SendTextMsgToServer(json_str);
			return;
		}
		
		//否则,关闭掉客户端连接
		LogMs().WithField("fun","ProcessLogInMessage")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("bid",req.bid)
			.WithField("user_name",req.user_name)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_WARNING,"diffrent node name and last login");		
		OnCloseConnection();

		/*
		//如果不在同一个结点,需要关闭掉原来的连接并重新连接
		boost::system::error_code ec;
		m_ws_socket_to_server.next_layer().close(ec);
		if (ec)
		{
			LogMs(LOG_WARNING, nullptr
				, "msg=open trade gateway master clsoe websocket to server fail!;errmsg=%s;nodename=%s;key=gatewayms"
				, ec.message().c_str()
				, m_last_slave_node.name.c_str());
		}
		m_cached_msgs.clear();
		m_connect_to_server = false;
		m_last_req_login = req;
		m_last_slave_node = slaveNodeInfo;

		//尝试连接到slave
		m_cached_msgs.push_back(json_str);
		start_connect_to_server();*/
	}
	//如果还没有连接到服务器
	else
	{
		//表示已经正在连接服务器,但还没有连上,这时不能再连
		if (!m_last_req_login.bid.empty())
		{
			std::stringstream ss;
			ss <<u8"重登录过于频繁,请稍后重试!";
			OutputNotifySycn(302,ss.str(),"WARNING");
			return;
		}
		
		m_cached_msgs.clear();
		m_connect_to_server = false;
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
		LogMs().WithField("fun","OnResolve")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)			
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING, "open trade gateway master OnResolve Slave node");
		
		std::stringstream ss;
		ss << u8"服务器暂时不可用,请联系快期技术支持人员!";
		OutputNotifySycn(303,ss.str(),"WARNING");
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
		LogMs().WithField("fun","OnConnectToServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master OnConnectToServer Slave node");		
		std::stringstream ss;
		ss << u8"服务器暂时不可用,请联系快期技术支持人员!";
		OutputNotifySycn(303,ss.str(), "WARNING");
		return;
	}

	int fd = m_ws_socket_to_server.next_layer().native_handle();
	int flags = fcntl(fd, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fd, F_SETFD, flags);

	try
	{
		//Perform the websocket handshake
		m_ws_socket_to_server.set_option(boost::beast::websocket::stream_base::decorator(
			[&](boost::beast::websocket::request_type& m)
		{
			m.insert(boost::beast::http::field::accept,"application/v1+json");
			m.insert(boost::beast::http::field::user_agent,_agent);
			m.insert("analysis",_analysis);
			m.insert("X-Real-IP", _X_Real_IP);
			m.insert("X-Real-Port", _X_Real_Port);
		}));
		m_ws_socket_to_server.async_handshake(m_last_slave_node.host
			, m_last_slave_node.path,
			boost::beast::bind_front_handler(
				&client_connection::OnHandshakeWithServer,
				shared_from_this()));
	}
	catch (const std::exception& ex)
	{
		LogMs().WithField("fun","OnConnectToServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_WARNING,"m_ws_socket_to_server async_handshake exception");		
	}
}

void client_connection::OnHandshakeWithServer(boost::system::error_code ec)
{
	if (ec)
	{
		LogMs().WithField("fun","OnHandshakeWithServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master OnHandshakeWithServer Slave node");
				
		std::stringstream ss;
		ss << u8"服务器暂时不可用,请联系快期技术支持人员!";
		OutputNotifySycn(303,ss.str(), "WARNING");
		return;
	}

	if (!m_ws_socket.next_layer().is_open())
	{
		stop_server();
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
	try
	{
		m_ws_socket_to_server.async_read(
			m_input_buffer_from_server,
			boost::beast::bind_front_handler(
				&client_connection::OnReadFromServer,
				shared_from_this()));
	}
	catch (const std::exception& ex)
	{
		LogMs().WithField("fun","DoReadFromServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_WARNING,"m_ws_socket_to_server async_read exception");		
	}
}

void client_connection::OnReadFromServer(boost::system::error_code ec
	, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	if (ec)
	{
		LogMs().WithField("fun","OnReadFromServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master OnReadFromServer Slave node");
		//关闭客户端连接
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
		LogMs().WithField("fun","OnTextMsgFromServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)			
			.Log(LOG_WARNING,"receive invalide msg from server");		
		return;
	}
		
	RtnBrokersMsg req;
	ss.ToVar(req);

	if (req.aid == "rtn_brokers")
	{
		return;
	}

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
		LogMs().WithField("fun","DoWriteToServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"open trade gateway master client connection DoWriteToServer exception");		
	}
}

void client_connection::OnWriteServer(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec)
	{
		LogMs().WithField("fun","OnWriteServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"open trade gateway master client connection send message to server fail");		
		//关闭客户端连接
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
		//如果服务端连接已经关闭
		if (!m_ws_socket_to_server.next_layer().is_open())
		{
			//关闭客户端连接
			OnCloseConnection();
			return;
		}

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
		LogMs().WithField("fun","SendTextMsgToServer")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"open trade gateway master client connection SendTextMsgToServer exception");	
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
		LogMs().WithField("fun","OnCloseConnection")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)			
			.Log(LOG_INFO,"client connection close connection");		

		client_connection_manager_.stop(shared_from_this());
	}
	catch (std::exception& ex)
	{
		LogMs().WithField("fun","OnCloseConnection")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"client_connection OnCloseConnection exception");		
	}	
}

void client_connection::stop()
{
	try
	{
		LogMs().WithField("fun","stop")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.Log(LOG_INFO,"m_ws_socket next_layer close");

		boost::system::error_code ec;
		m_ws_socket.next_layer().close(ec);
		if (ec)
		{
			LogMs().WithField("fun","stop")
				.WithField("key","gatewayms")
				.WithField("agent",_agent)
				.WithField("ip",_X_Real_IP)
				.WithField("analysis",_analysis)
				.WithField("connId",_connection_id)
				.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
				.WithField("bid",m_last_req_login.bid)
				.WithField("nodename",m_last_slave_node.name)
				.WithField("errmsg",ec.message())
				.Log(LOG_INFO,"m_ws_socket stop exception");			
		}

		if (m_connect_to_server)
		{
			stop_server();
		}
	}
	catch (std::exception& ex)
	{
		LogMs().WithField("fun","stop")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_INFO,"m_ws_socket stop exception");		
	}	
}

void client_connection::stop_server()
{
	try
	{
		LogMs().WithField("fun","stop_server")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)			
			.Log(LOG_INFO,"m_ws_socket_to_server next_layer close");
		
		boost::system::error_code ec;
		m_ws_socket_to_server.next_layer().close(ec);
		if (ec)
		{
			LogMs().WithField("fun","stop_server")
				.WithField("key","gatewayms")
				.WithField("agent",_agent)
				.WithField("ip",_X_Real_IP)
				.WithField("analysis",_analysis)
				.WithField("connId",_connection_id)
				.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
				.WithField("bid",m_last_req_login.bid)
				.WithField("nodename",m_last_slave_node.name)
				.WithField("errmsg",ec.message())
				.Log(LOG_INFO,"m_ws_socket_to_server close exception");			
		}
	}
	catch (std::exception& ex)
	{
		LogMs().WithField("fun","stop_server")
			.WithField("key","gatewayms")
			.WithField("agent",_agent)
			.WithField("ip",_X_Real_IP)
			.WithField("analysis",_analysis)
			.WithField("connId",_connection_id)
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.WithField("bid",m_last_req_login.bid)
			.WithField("nodename",m_last_slave_node.name)
			.WithField("errmsg",ex.what())
			.Log(LOG_INFO,"m_ws_socket_to_server stop exception");		
	}
}