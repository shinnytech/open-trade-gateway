#pragma once

#include "types.h"
#include "condition_order_serializer.h"

#include <map>
#include <queue>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

class traderks
{
public:
	traderks(boost::asio::io_context& ios
		,const std::string& key);
	
	void Start();

	void Stop();		
private:
	std::atomic_bool m_b_login;

	std::string _key;

	boost::asio::io_context& _ios;

	std::shared_ptr<boost::interprocess::message_queue> _out_mq_ptr;

	std::string _out_mq_name;

	std::shared_ptr<boost::interprocess::message_queue> _in_mq_ptr;

	std::string _in_mq_name;

	boost::shared_ptr<boost::thread> _thread_ptr;

	ReqLogin _req_login;

	std::atomic_bool m_run_receive_msg;

	std::vector<int> m_logined_connIds;
			
	void ReceiveMsg(const std::string& key);
	
	void OnIdle();

	void ProcessInMsg(int connId, std::shared_ptr<std::string> msg_ptr);

	void CloseConnection(int nId);
};
