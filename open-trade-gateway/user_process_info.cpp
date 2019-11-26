/////////////////////////////////////////////////////////////////////////
///@file user_process_info.cpp
///@brief	用户进程管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "user_process_info.h"
#include "SerializerTradeBase.h"
#include "condition_order_serializer.h"

#include <boost/algorithm/string.hpp>

using namespace std::chrono;

UserProcessInfo::UserProcessInfo(boost::asio::io_context& ios
	, const std::string& key
	, const ReqLogin& reqLogin)
	:io_context_(ios)
	,_key(key)
	,_reqLogin(reqLogin)
	,_out_mq_ptr()
	,_out_mq_name("")
	,_run_thread(true)
	,_thread_ptr()
	,_in_mq_ptr()
	,_in_mq_name("")	
	,_process_ptr()	
	,user_connections_()	
{
}

UserProcessInfo::~UserProcessInfo()
{
}

bool UserProcessInfo::StartProcess()
{
	try
	{
		if(_reqLogin.broker.broker_type == "ctp")
		{			
			return StartProcess_i("open-trade-ctp",_key);
		}
		else if (_reqLogin.broker.broker_type == "ctpsopt")
		{
			return StartProcess_i("open-trade-ctpsopt", _key);
		}
		else if (_reqLogin.broker.broker_type == "ctpse")
		{
			return StartProcess_i("open-trade-ctpse15",_key);
		}		
		else if (_reqLogin.broker.broker_type == "sim")
		{
			return StartProcess_i("open-trade-sim",_key);
		}
		else if (_reqLogin.broker.broker_type == "perftest")
		{
			return StartProcess_i("open-trade-perftest", _key);
		}
		else
		{
			Log().WithField("fun","StartProcess")
				.WithField("key","gateway")
				.WithField("user_key",_key)
				.WithField("bidtype",_reqLogin.broker.broker_type)
				.Log(LOG_ERROR,"trade server req_login invalid broker_type");		
			return false;
		}		
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun","StartProcess")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_WARNING,"UserProcessInfo StartProcess() fail!");	
		return false;
	}	
}

bool UserProcessInfo::ReStartProcess()
{
	bool flag = false;
	if (_reqLogin.broker.broker_type == "ctp")
	{
		flag=RestartProcess_i("open-trade-ctp", _key);
	}
	else if (_reqLogin.broker.broker_type == "ctpsopt")
	{
		return RestartProcess_i("open-trade-ctpsopt", _key);
	}
	else if (_reqLogin.broker.broker_type == "ctpse")
	{
		flag=RestartProcess_i("open-trade-ctpse15", _key);
	}
	else if (_reqLogin.broker.broker_type == "sim")
	{
		flag=RestartProcess_i("open-trade-sim", _key);
	}
	else if (_reqLogin.broker.broker_type == "perftest")
	{
		flag=RestartProcess_i("open-trade-perftest", _key);
	}
	else
	{
		Log().WithField("fun","ReStartProcess")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("bidtype",_reqLogin.broker.broker_type)			
			.Log(LOG_ERROR,"ReStartProcess invalid broker_type");		
		flag = false;
	}

	if (!flag)
	{
		Log().WithField("fun","ReStartProcess")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("bidtype",_reqLogin.broker.broker_type)
			.Log(LOG_ERROR,"ReStartProcess fail!");		
		return false;
	}
	
	SerializerTradeBase nss;
	nss.FromVar(_reqLogin);
	std::string strMsg;
	nss.ToString(&strMsg);
	SendMsg(0,strMsg);

	req_reconnect_trade_instance req_reconnect;
	for (auto a : user_connections_)
	{
		req_reconnect.connIds.push_back(a.first);
	}
	SerializerConditionOrderData ns;
	ns.FromVar(req_reconnect);
	ns.ToString(&strMsg);
	SendMsg(0, strMsg);

	return true;
}

void UserProcessInfo::StopProcess()
{
	if ((nullptr != _process_ptr)
		&&(_process_ptr->running()))
	{		
		_process_ptr->terminate();
		_process_ptr->wait();
		_process_ptr.reset();		
	}

	if (nullptr != _thread_ptr)
	{
		_run_thread.store(false);
		_thread_ptr->detach();
		_thread_ptr.reset();
	}

	user_connections_.clear();	
	if (nullptr != _out_mq_ptr)
	{		
		boost::interprocess::message_queue::remove(_out_mq_name.c_str());
		_out_mq_ptr.reset();		
	}

	if (nullptr != _in_mq_ptr)
	{
		boost::interprocess::message_queue::remove(_in_mq_name.c_str());
		_in_mq_ptr.reset();		
	}

	TUserProcessInfoMap::iterator it = g_userProcessInfoMap.find(_key);
	if (it != g_userProcessInfoMap.end())
	{
		g_userProcessInfoMap.erase(it);
	}	
}

bool UserProcessInfo::ProcessIsRunning()
{
	if (nullptr == _process_ptr)
	{
		return false;
	}
	return _process_ptr->running();
}

bool UserProcessInfo::SendMsg(int connid,const std::string& msg)
{	
	if (nullptr == _in_mq_ptr)
	{
		Log().WithField("fun","SendMsg")
			.WithField("key","gateway")
			.WithField("user_key",_key)			
			.Log(LOG_WARNING,"UserProcessInfo SendMsg and _in_mq_ptr is nullptr");		
		return false;
	}

	std::stringstream ss;
	ss << connid << "|" << msg;
	std::string str = ss.str();
	try
	{
		unsigned int priority = 0;
		bool send_success=_in_mq_ptr->try_send(str.c_str(),str.length(),priority);
		return send_success;
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","SendMsg")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.WithField("msglen",(int)str.length())
			.Log(LOG_ERROR,"UserProcessInfo SendMsg exception");	
		return false;
	}	
}

void UserProcessInfo::NotifyClose(int connid)
{
	if (nullptr == _in_mq_ptr)
	{
		Log().WithField("fun","NotifyClose")
			.WithField("key","gateway")
			.WithField("user_key",_key)		
			.Log(LOG_WARNING,"UserProcessInfo NotifyClose and _in_mq_ptr is nullptr");		
		return;
	}

	std::stringstream ss;
	ss << connid << "|"<< CLOSE_CONNECTION_MSG;
	std::string str = ss.str();
	try
	{
		_in_mq_ptr->send(str.c_str(),str.length(),0);
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","NotifyClose")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.WithField("msgcontent",str)
			.WithField("msglen",(int)str.length())
			.Log(LOG_ERROR,"UserProcessInfo SendMsg exception");		
	}
}

bool UserProcessInfo::RestartProcess_i(const std::string& name, const std::string& cmd)
{
	try
	{
		if (nullptr != _process_ptr)
		{
			_process_ptr->wait();

			Log().WithField("fun", "RestartProcess_i")
				.WithField("key", "gateway")
				.WithField("user_key", _key)
				.WithField("exit_code", _process_ptr->exit_code())
				.WithField("native_exit_code", _process_ptr->native_exit_code())
				.Log(LOG_ERROR, "before restart process user process");

			_process_ptr.reset();
		}

		_process_ptr = std::make_shared<boost::process::child>(boost::process::child(
			boost::process::search_path(name)
			, cmd.c_str()));
		
		if (nullptr == _process_ptr)
		{
			return false;
		}

		return _process_ptr->running();
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","RestartProcess_i")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())		
			.Log(LOG_ERROR,"restart process user process exception");	
		return false;
	}
}

bool UserProcessInfo::StartProcess_i(const std::string& name, const std::string& cmd)
{	
	try
	{
		_out_mq_name = cmd + "_msg_out";
		boost::interprocess::message_queue::remove(_out_mq_name.c_str());
		_out_mq_ptr = std::shared_ptr <boost::interprocess::message_queue>
			(new boost::interprocess::message_queue(boost::interprocess::create_only
				, _out_mq_name.c_str(), MAX_MSG_NUMS_OUT, MAX_MSG_LENTH));
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","StartProcess_i")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"StartProcess_i and create out message queue exception");		
		return false;
	}

	try
	{
		_thread_ptr.reset();
		_thread_ptr = std::shared_ptr<boost::thread>(
			new boost::thread(boost::bind(&UserProcessInfo::ReceiveMsg_i,shared_from_this(),_key)));
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","StartProcess_i")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"StartProcess_i and start ReceiveMsg_i thread exception");	
		return false;
	}

	try
	{
		_in_mq_name = cmd + "_msg_in";
		boost::interprocess::message_queue::remove(_in_mq_name.c_str());
		_in_mq_ptr = std::shared_ptr <boost::interprocess::message_queue>
			(new boost::interprocess::message_queue(boost::interprocess::create_only
				, _in_mq_name.c_str(),MAX_MSG_NUMS_IN, MAX_MSG_LENTH));
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","StartProcess_i")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"StartProcess_i and create in message queue exception");		
		return false;
	}
	
	try
	{
		_process_ptr = std::make_shared<boost::process::child>(boost::process::child(
			boost::process::search_path(name)
			, cmd.c_str()));
		if (nullptr == _process_ptr)
		{
			return false;
		}
		return _process_ptr->running();
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","StartProcess_i")
			.WithField("key","gateway")
			.WithField("user_key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"StartProcess_i and start user process fail");		
		return false;
	}
}

void UserProcessInfo::ReceiveMsg_i(const std::string& key)
{
	std::string strKey = key;
	std::string _str_packge_splited = "";
	bool _packge_is_begin = false;
	char buf[MAX_MSG_LENTH+1];
	unsigned int priority=0;
	boost::interprocess::message_queue::size_type recvd_size=0;
	while (_run_thread)
	{
		try
		{
			memset(buf, 0, sizeof(buf));
			_out_mq_ptr->receive(buf,sizeof(buf),recvd_size,priority);
			std::string msg=buf;
			if (msg.empty())
			{
				continue;
			}
			if (msg == BEGIN_OF_PACKAGE)
			{
				_str_packge_splited = "";
				_packge_is_begin = true;
				continue;
			}
			else if (msg == END_OF_PACKAGE)
			{
				_packge_is_begin = false;
				if (_str_packge_splited.length() > 0)
				{
					if (!io_context_.stopped())
					{
						std::shared_ptr<std::string> msg_ptr =
							std::shared_ptr<std::string>(new std::string(_str_packge_splited));
						io_context_.post(boost::bind(&UserProcessInfo::ProcessMsg
							, shared_from_this(), msg_ptr));
					}					
				}
				continue;
			}
			else
			{
				if (_packge_is_begin)
				{
					_str_packge_splited += msg;
					continue;
				}
				else
				{					
					if (!io_context_.stopped())
					{
						std::shared_ptr<std::string> msg_ptr =
							std::shared_ptr<std::string>(new std::string(msg));
						io_context_.post(boost::bind(&UserProcessInfo::ProcessMsg
							, shared_from_this(), msg_ptr));
					}					
					continue;
				}
			}
		}
		catch (const std::exception& ex)
		{
			Log().WithField("fun","ReceiveMsg_i")
				.WithField("key","gateway")				
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"ReceiveMsg_i Erro");			
		}
	}
}

void UserProcessInfo::ProcessMsg(std::shared_ptr<std::string> msg_ptr)
{	
	if (nullptr== msg_ptr)
	{
		return;
	}

	std::string& msg = *msg_ptr;

	int nPos = msg.find_first_of('#');
	if ((nPos <= 0) || (nPos + 1 >= msg.length()))
	{
		return;
	}
	
	std::string strIds = msg.substr(0, nPos);
	std::string strMsg = msg.substr(nPos + 1);
	if (strIds.empty() || strMsg.empty())
	{
		return;
	}
	
	std::vector<std::string> ids;
	boost::algorithm::split(ids,strIds,boost::algorithm::is_any_of("|"));
	for (auto strId : ids)
	{
		int nId = atoi(strId.c_str());
		auto it = user_connections_.find(nId);
		if (it == user_connections_.end())
		{
			continue;
		}
		connection_ptr conn_ptr = it->second;
		if (nullptr != conn_ptr)
		{
			conn_ptr->SendTextMsg(strMsg);
		}
	}
}

ReqLogin UserProcessInfo::GetReqLogin()
{
	return _reqLogin;
}

std::string UserProcessInfo::GetKey()
{
	return _key;
}