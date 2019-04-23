/////////////////////////////////////////////////////////////////////////
///@file user_process_info.cpp
///@brief	用户进程管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "user_process_info.h"
#include "SerializerTradeBase.h"

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
			std::string cmd = "ctp_" + _reqLogin.bid + "_" + _reqLogin.user_name;
			
			return StartProcess_i("open-trade-ctp", cmd);			
		}
		else if (_reqLogin.broker.broker_type == "ctpse")
		{
			std::string cmd = "ctpse_" + _reqLogin.bid + "_" + _reqLogin.user_name;			
			return StartProcess_i("open-trade-ctpse", cmd);
		}
		else if (_reqLogin.broker.broker_type == "sim")
		{
			std::string cmd = "sim_" + _reqLogin.bid + "_" + _reqLogin.user_name;
			return StartProcess_i("open-trade-sim", cmd);			
		}
		else if (_reqLogin.broker.broker_type == "perftest")
		{
			std::string cmd = "perftest_" + _reqLogin.bid + "_" + _reqLogin.user_name;
			return StartProcess_i("open-trade-perftest",cmd);
		}
		else
		{
			Log(LOG_ERROR,NULL,"trade server req_login invalid broker_type=%s"
				, _reqLogin.broker.broker_type.c_str());			
			return false;
		}		
	}
	catch (const std::exception& ex)
	{
		Log(LOG_WARNING, NULL
			,"UserProcessInfo::StartProcess() fail:%s!",ex.what());
		return false;
	}	
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

void UserProcessInfo::SendMsg(int connid,const std::string& msg)
{	
	if (nullptr == _in_mq_ptr)
	{
		Log(LOG_WARNING, NULL, "UserProcessInfo::SendMsg,nullptr == _in_mq_ptr");
		return;
	}

	std::stringstream ss;
	ss << connid << "|" << msg;
	std::string str = ss.str();
	try
	{
		_in_mq_ptr->send(str.c_str(),str.length(), 0);
	}
	catch (std::exception& ex)
	{
		Log(LOG_ERROR, NULL
			, "UserProcessInfo::SendMsg Erro:%s,msg:%s,length:%d"
			, ex.what(), str.c_str(), str.length());
	}	
}

void UserProcessInfo::NotifyClose(int connid)
{
	if (nullptr == _in_mq_ptr)
	{
		Log(LOG_WARNING, NULL, "UserProcessInfo::NotifyClose,nullptr == _in_mq_ptr");
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
		Log(LOG_ERROR, NULL
			, "UserProcessInfo::SendMsg Erro:%s,msg:%s,length:%d"
			, ex.what(), str.c_str(), str.length());
	}
}

void UserProcessInfo::Child_Exit_handle(boost::system::error_code ec, int code)
{
	if (ec)
	{
		Log(LOG_WARNING,NULL
			, "UserProcessInfo Child_Exit_handle Erorr,msg=%s"
			,ec.message().c_str());		
	}
	else
	{
		Log(LOG_WARNING
			, NULL
			, "UserProcessInfo Child_Exit_handle,code=%d"
			, code);
	}
}

bool UserProcessInfo::StartProcess_i(const std::string& name, const std::string& cmd)
{	
	_out_mq_name = cmd + "_msg_out";
	_in_mq_name = cmd + "_msg_in";
	boost::interprocess::message_queue::remove(_out_mq_name.c_str());
	boost::interprocess::message_queue::remove(_in_mq_name.c_str());

	_out_mq_ptr = std::shared_ptr <boost::interprocess::message_queue>
		(new boost::interprocess::message_queue(boost::interprocess::create_only
			, _out_mq_name.c_str(), MAX_MSG_NUMS, MAX_MSG_LENTH));
	_thread_ptr.reset();

	_thread_ptr = std::shared_ptr<boost::thread>(
		new boost::thread(boost::bind(&UserProcessInfo::ReceiveMsg_i,shared_from_this())));

	_in_mq_ptr = std::shared_ptr <boost::interprocess::message_queue>
		(new boost::interprocess::message_queue(boost::interprocess::create_only
			, _in_mq_name.c_str(), MAX_MSG_NUMS, MAX_MSG_LENTH));
	
	_process_ptr = std::make_shared<boost::process::child>(boost::process::child(
		boost::process::search_path(name)
		,cmd.c_str()));
	if (nullptr == _process_ptr)
	{
		return false;
	}
	return _process_ptr->running();
}

void UserProcessInfo::ReceiveMsg_i()
{
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
			Log(LOG_ERROR, NULL, "ReceiveMsg_i Erro:%s", ex.what());
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