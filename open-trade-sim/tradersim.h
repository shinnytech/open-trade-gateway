/////////////////////////////////////////////////////////////////////////
///@file tradersim.h
///@brief	Sim交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

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

struct ActionOrder
{
	ActionOrder()
	{
		price_type = kPriceTypeLimit;
		volume_condition = kOrderVolumeConditionAny;
		time_condition = kOrderTimeConditionGFD;
		hedge_flag = kHedgeFlagSpeculation;
		limit_price = 0;
		volume = 0;
	}
	std::string aid;
	std::string order_id;
	std::string user_id;
	std::string exchange_id;
	std::string ins_id;
	long direction;
	long offset;
	int volume;
	long price_type;
	double limit_price;
	long volume_condition;
	long time_condition;
	long hedge_flag;
};

struct ActionTransfer
{
	ActionTransfer()
	{
		currency = "CNY";
		amount = 0.0;
	}
	std::string currency;
	double amount;
};

class SerializerSim
	: public RapidSerialize::Serializer<SerializerSim>
{
public:
	using RapidSerialize::Serializer<SerializerSim>::Serializer;

	void DefineStruct(ActionOrder& d);
	void DefineStruct(ActionTransfer& d);
};

class tradersim
{
public:
	tradersim(boost::asio::io_context& ios, const std::string& logFileName);
	   
	void Start();

	void Stop();
private:
	std::atomic_bool m_b_login;

	std::string _logFileName;
	
	boost::asio::io_context& _ios;

	std::shared_ptr<boost::interprocess::message_queue> _out_mq_ptr;

	std::string _out_mq_name;

	std::shared_ptr<boost::interprocess::message_queue> _in_mq_ptr;

	std::string _in_mq_name;

	boost::shared_ptr<boost::thread> _thread_ptr;

	ReqLogin _req_login;

	std::string m_user_file_path;

	//业务信息
	//交易账号
	std::string m_user_id; 

	//交易账户全信息
	User m_data;  

	std::set<Order*> m_alive_order_set;

	//模拟交易管理
	Account* m_account; //指向 m_data.m_trade["SIM"].m_accounts["CNY"]

	int m_loging_connectId;

	std::vector<int> m_logined_connIds;

	bool m_something_changed;

	int m_notify_seq;

	int m_data_seq;

	int m_last_seq_no;

	bool m_peeking_message;

	long long m_next_send_dt;

	void ReceiveMsg();

	void CloseConnection(int nId);

	bool IsConnectionLogin(int nId);

	void OutputNotifyAsych(int connId, long notify_code
		, const std::string& ret_msg, const char* level = "INFO"
		, const char* type = "MESSAGE");

	void OutputNotifySycn(int connId, long notify_code
		, const std::string& ret_msg, const char* level = "INFO"
		, const char* type = "MESSAGE");

	void SendMsg(int connId,std::shared_ptr<std::string> msg_ptr);

	void OutputNotifyAllAsych(long notify_code
		, const std::string& ret_msg, const char* level = "INFO"
		, const char* type = "MESSAGE");

	void OutputNotifyAllSycn(long notify_code
		, const std::string& ret_msg, const char* level = "INFO"
		, const char* type = "MESSAGE");

	void SendMsgAll(std::shared_ptr<std::string> msg_ptr);

	void ProcessInMsg(int connId,std::shared_ptr<std::string> msg_ptr);

	void ProcessReqLogIn(int connId, ReqLogin& req);
	
	void OnInit();

	void OnIdle();

	bool WaitLogIn();
	
	//数据存取档
	void LoadUserDataFile();

	void SaveUserDataFile();

	//用户请求处理
	void OnClientReqInsertOrder(ActionOrder action_insert_order);

	void OnClientReqCancelOrder(ActionOrder action_cancel_order);

	void OnClientReqTransfer(ActionTransfer action_transfer);

	void UpdateOrder(Order* order);

	Position& GetPosition(const std::string position_key);

	void UpdatePositionVolume(Position* position);

	void SendUserData();

	void SendUserDataImd(int connectId);

	void TryOrderMatch();

	void CheckOrderTrade(Order* order);

	void DoTrade(Order* order, int volume, double price);

	void OnClientPeekMessage();
};
