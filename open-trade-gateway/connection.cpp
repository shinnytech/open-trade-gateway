/////////////////////////////////////////////////////////////////////////
///@file connection.cpp
///@brief	websocket连接类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "connection.h"
#include "connection_manager.h"
#include "log.h"
#include "config.h"
#include "SerializerTradeBase.h"
#include "user_process_info.h"

#include <iostream>

using namespace std::chrono;

TUserProcessInfoMap g_userProcessInfoMap;

connection::connection(boost::asio::io_context& ios
	,boost::asio::ip::tcp::socket socket,
	connection_manager& manager,
	int connection_id)
	:m_ios(ios),
	m_ws_socket(std::move(socket)),		
	m_input_buffer(),
	m_output_buffer(),
	connection_manager_(manager),	
	_connection_id(connection_id),	
	_reqLogin(),	
	_user_broker_key(""),
	flat_buffer_(),
	req_(),
	_X_Real_IP(""),
	_X_Real_Port(0)
{		
}

void connection::start()
{	
	int fd=m_ws_socket.next_layer().native_handle();
	int flags = fcntl(fd, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fd, F_SETFD, flags);

	req_ = {};
	boost::beast::http::async_read(m_ws_socket.next_layer()
		,flat_buffer_
		,req_,
		boost::beast::bind_front_handler(
			&connection::on_read_header,
			shared_from_this()));	
}

void connection::stop()
{
	try
	{
		Log(LOG_INFO
			, NULL
			, "trade connection stop,connectionid=%d,fd=%d"
			, _connection_id
			, m_ws_socket.next_layer().native_handle());
		m_ws_socket.next_layer().close();
	}
	catch (std::exception& ex)
	{
		Log(LOG_INFO, NULL, "connection stop exception:%s"
			, ex.what());
	}
}

void connection::OnOpenConnection(boost::system::error_code ec)
{
	if (ec)
	{
		Log(LOG_WARNING
			, NULL
			, "trade connection accept fail,msg=%s,connectionid=%d,fd=%d"
			,ec.message().c_str()
			, _connection_id
			, m_ws_socket.next_layer().native_handle());
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
			_X_Real_Port = remote_ep.port();
		}
		else
		{
			_X_Real_Port = atoi(real_port.c_str());
		}

		SendTextMsg(g_config.broker_list_str);
		DoRead();
	}
	catch (std::exception& ex)
	{
		Log(LOG_INFO,NULL,"connection OnOpenConnection exception:%s"
			, ex.what());
	}	
}

void connection::on_read_header(boost::beast::error_code ec
	, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);

	if (ec == boost::beast::http::error::end_of_stream)
	{
		Log(LOG_INFO
			, NULL
			, "connection on_read_header fail,msg=%s,connectionid=%d,fd=%d"
			, ec.message().c_str()
			,_connection_id
			,m_ws_socket.next_layer().native_handle());
		OnCloseConnection();
		return;
	}
	m_ws_socket.async_accept(
		req_,
		boost::beast::bind_front_handler(
			&connection::OnOpenConnection,
			shared_from_this()));
}

void connection::SendTextMsg(const std::string& msg)
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
		Log(LOG_ERROR, NULL, "connection SendTextMsg exception:%s,connectionid=%d,fd=%d"
			, ex.what()
			, _connection_id
			, m_ws_socket.next_layer().native_handle());
	}	
}

void connection::DoWrite()
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
				&connection::OnWrite,
				shared_from_this()));
	}
	catch (std::exception& ex)
	{
		Log(LOG_ERROR, NULL, "connection DoWrite exception=%s,connectionid=%d,fd=%d"
			,ex.what()
			,_connection_id
			,m_ws_socket.next_layer().native_handle());
	}	
}

void connection::DoRead()
{
	m_ws_socket.async_read(
		m_input_buffer,
		boost::beast::bind_front_handler(
			&connection::OnRead,
			shared_from_this()));
}

void connection::OnRead(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec)
	{
		if (ec != boost::beast::websocket::error::closed)
		{
			Log(LOG_INFO
				, NULL
				, "trade connection read fail,connection=%d,fd=%d,error=%s"
				, _connection_id
				, m_ws_socket.next_layer().native_handle()
				,ec.message().c_str());
		}
		OnCloseConnection();
		return;
	}
	
	std::string strMsg = boost::beast::buffers_to_string(m_input_buffer.data());
	m_input_buffer.consume(bytes_transferred);
	OnMessage(strMsg);
	DoRead();
}

void connection::OnWrite(boost::system::error_code ec,std::size_t bytes_transferred)
{
	if (ec)
	{
		Log(LOG_INFO, NULL, "trade server send message fail,connection=%d,fd=%d,err=%s"
			,_connection_id
			,m_ws_socket.next_layer().native_handle()
			,ec.message().c_str());
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

void connection::OnMessage(const std::string &json_str)
{
	SerializerTradeBase ss;
	if (!ss.FromString(json_str.c_str()))
	{
		Log(LOG_INFO, NULL
			, "connection recieve invalid diff data package=%s,connection=%d,fd=%d"
			, json_str.c_str()
			,_connection_id
			,m_ws_socket.next_layer().native_handle());
		return;
	}

	ReqLogin req;
	ss.ToVar(req);

	if (req.aid == "req_login")
	{
		Log(LOG_INFO, NULL
			, "req_login client_system_info=%s,client_app_id=%s"
			, req.client_system_info.c_str()
		, req.client_app_id.c_str());
		ProcessLogInMessage(req, json_str);
	}
	else
	{
		ProcessOtherMessage(json_str);
	}
}

void connection::ProcessLogInMessage(const ReqLogin& req, const std::string &json_str)
{
	_login_msg = json_str;	
	_reqLogin = req;
	auto it = g_config.brokers.find(_reqLogin.bid);
	if (it == g_config.brokers.end())
	{
		Log(LOG_WARNING,NULL,
			"trade server req_login invalid bid,connection=%d, bid=%s"
			, _connection_id,req.bid.c_str());
		return;
	}

	_reqLogin.broker = it->second;
	_reqLogin.client_ip = _X_Real_IP;
	_reqLogin.client_port = _X_Real_Port;
	SerializerTradeBase nss;
	nss.FromVar(_reqLogin);
	nss.ToString(&_login_msg);
	std::string strBrokerType = _reqLogin.broker.broker_type;

	if (_user_broker_key.empty())
	{
		_user_broker_key = strBrokerType + "_" + _reqLogin.bid + "_" + _reqLogin.user_name;
	}
	else
	{		
		//防止一个连接进行多个登录
		std::string new_user_broker_key= strBrokerType + "_" + _reqLogin.bid + "_" + _reqLogin.user_name;
		Log(LOG_INFO, NULL,
			"old key=%s,new key=%s"
			, _user_broker_key.c_str(), new_user_broker_key.c_str());
		if (new_user_broker_key != _user_broker_key)
		{
			auto userIt = g_userProcessInfoMap.find(_user_broker_key);
			if (userIt != g_userProcessInfoMap.end())
			{
				UserProcessInfo_ptr userProcessInfoPtr = userIt->second;
				bool flag = userProcessInfoPtr->ProcessIsRunning();
				if (flag)
				{
					userProcessInfoPtr->NotifyClose(_connection_id);
				}
				if (userProcessInfoPtr->user_connections_.find(_connection_id)
					!= userProcessInfoPtr->user_connections_.end())
				{
					userProcessInfoPtr->user_connections_.erase(_connection_id);
				}
			}
		}
		_user_broker_key = new_user_broker_key;
	}
	
	auto userIt = g_userProcessInfoMap.find(_user_broker_key);
	//如果用户进程没有启动,启动用户进程处理
	if (userIt == g_userProcessInfoMap.end())
	{
		 UserProcessInfo_ptr userProcessInfoPtr = std::make_shared<UserProcessInfo>(m_ios,_user_broker_key,_reqLogin);
		 if (nullptr == userProcessInfoPtr)
		 {
			 Log(LOG_ERROR, NULL,"new user process fail=%s"
				 ,_user_broker_key.c_str());			
			 return;
		 }
		 if (!userProcessInfoPtr->StartProcess())
		 {
			 Log(LOG_ERROR,NULL, "can not start up user process=%s"
				 , _user_broker_key.c_str());			
			 return;
		 }
		 userProcessInfoPtr->user_connections_.insert(
			 std::map<int,connection_ptr>::value_type(
				 _connection_id,shared_from_this()));
		 g_userProcessInfoMap.insert(TUserProcessInfoMap::value_type(
			 _user_broker_key,userProcessInfoPtr));		
		 userProcessInfoPtr->SendMsg(_connection_id,_login_msg);
		 return;
	}
	//如果用户进程已经启动,直接利用
	else
	{
		UserProcessInfo_ptr userProcessInfoPtr = userIt->second;
		//进程是否正常运行
		bool flag = userProcessInfoPtr->ProcessIsRunning();
		if (flag)
		{
			userProcessInfoPtr->user_connections_.insert(
				std::map<int, connection_ptr>::value_type(
					_connection_id, shared_from_this()));
			userProcessInfoPtr->SendMsg(_connection_id,_login_msg);
			return;
		}
		else
		{
			flag = userProcessInfoPtr->StartProcess();
			if (!flag)
			{
				Log(LOG_ERROR, NULL, "can not start up user process=%s"
					, _user_broker_key.c_str());				
				return;
			}
			userProcessInfoPtr->user_connections_.insert(
				std::map<int, connection_ptr>::value_type(
					_connection_id, shared_from_this()));			
			userProcessInfoPtr->SendMsg(_connection_id,_login_msg);
			return;
		}
	}
}

void connection::ProcessOtherMessage(const std::string &json_str)
{
	auto userIt = g_userProcessInfoMap.find(_user_broker_key);
	if (userIt == g_userProcessInfoMap.end())
	{
		Log(LOG_INFO, NULL
			, "send msg before user process start up,msg droped=%s"
			, json_str.c_str());
		return;
	}

	UserProcessInfo_ptr userProcessInfoPtr = userIt->second;	
	bool flag = userProcessInfoPtr->ProcessIsRunning();
	if (!flag)
	{
		Log(LOG_ERROR, NULL
			,"user process is down,msg can not send to user process=%s"
			,json_str.c_str());
		return;
	}
		
	userProcessInfoPtr->SendMsg(_connection_id,json_str);
}

void connection::OnCloseConnection()
{	
	try
	{
		auto userIt = g_userProcessInfoMap.find(_user_broker_key);		
		if (userIt != g_userProcessInfoMap.end())
		{			
			UserProcessInfo_ptr userProcessInfoPtr = userIt->second;
			userProcessInfoPtr->user_connections_.erase(_connection_id);
			if (userProcessInfoPtr->ProcessIsRunning())
			{				
				userProcessInfoPtr->NotifyClose(_connection_id);
			}
		}		
		connection_manager_.stop(shared_from_this());		
	}
	catch (std::exception& ex)
	{
		Log(LOG_ERROR, NULL, "connection::OnCloseConnection(),err=%s,connection=%d,fd=%d"
			, ex.what()
			, _connection_id
			, m_ws_socket.next_layer().native_handle());
	}	
}

