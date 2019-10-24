/////////////////////////////////////////////////////////////////////////
///@file user_process_info.h
///@brief	用户进程管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"
#include "connection.h"

#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

class UserProcessInfo
	: public std::enable_shared_from_this<UserProcessInfo>
{
public:
	UserProcessInfo(boost::asio::io_context& ios
		,const std::string& key
		, const ReqLogin& reqLogin);

	~UserProcessInfo();

	bool StartProcess();

	bool ReStartProcess();
	   
	void StopProcess();

	bool ProcessIsRunning();

	bool SendMsg(int connid, const std::string& msg);

	void NotifyClose(int connid);

	ReqLogin GetReqLogin();

	std::string GetKey();
private:
	bool RestartProcess_i(const std::string& name, const std::string& cmd);

	bool StartProcess_i(const std::string& name, const std::string& cmd);

	void ReceiveMsg_i(const std::string& key);

	void ProcessMsg(std::shared_ptr<std::string> msg_ptr);
private:
	boost::asio::io_context& io_context_;

	const std::string _key;

	ReqLogin _reqLogin;
	
	std::shared_ptr<boost::interprocess::message_queue> _out_mq_ptr;
	
	std::string _out_mq_name;

	std::atomic_bool _run_thread;

	std::shared_ptr<boost::thread> _thread_ptr;

	std::shared_ptr<boost::interprocess::message_queue> _in_mq_ptr;

	std::string _in_mq_name;

	std::shared_ptr<boost::process::child> _process_ptr;	
public:
	std::map<int, connection_ptr> user_connections_;	
};

typedef std::shared_ptr<UserProcessInfo> UserProcessInfo_ptr;

typedef std::map<std::string,UserProcessInfo_ptr> TUserProcessInfoMap;

extern TUserProcessInfoMap g_userProcessInfoMap;
