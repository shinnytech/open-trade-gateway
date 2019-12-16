/////////////////////////////////////////////////////////////////////////
///@file tradersim.cpp
///@brief	Sim交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "tradersim.h"
#include "SerializerTradeBase.h"
#include "config.h"
#include "utility.h"
#include "ins_list.h"
#include "log.h"
#include "rapid_serialize.h"
#include "numset.h"
#include "types.h"
#include "types.h"
#include "datetime.h"
#include "condition_order_serializer.h"

#include <iostream>
#include <string>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

tradersim::tradersim(boost::asio::io_context& ios
	, const std::string& key)
:m_b_login(false)
,_key(key)
,_ios(ios)
,_out_mq_ptr()
,_out_mq_name(_key+"_msg_out")
,_in_mq_ptr()
,_in_mq_name(_key+"_msg_in")
,_thread_ptr()
,_req_login()
,m_user_file_path("")
,m_alive_order_set()
,m_loging_connectId(-1)
,m_logined_connIds()
,m_account(NULL)
,m_something_changed(false)
,m_data_seq(0)
,m_last_seq_no(0)
,m_peeking_message(false)
,m_next_send_dt(0)
,m_transfer_seq(0)
,m_run_receive_msg(false)
,m_session_id(0)
,m_condition_order_data()
,m_condition_order_his_data()
,m_condition_order_manager(_key
,m_condition_order_data
,m_condition_order_his_data,*this)
{
}

#pragma region systemlogic

void tradersim::Start()
{
	try
	{
		_out_mq_ptr = std::shared_ptr<boost::interprocess::message_queue>
			(new boost::interprocess::message_queue(boost::interprocess::open_only
				, _out_mq_name.c_str()));

		_in_mq_ptr = std::shared_ptr<boost::interprocess::message_queue>
			(new boost::interprocess::message_queue(boost::interprocess::open_only
				, _in_mq_name.c_str()));
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun","Start")
			.WithField("key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"tradersim open message queue exception");
	}

	try
	{
		m_run_receive_msg.store(true);
		_thread_ptr = boost::make_shared<boost::thread>(
			boost::bind(&tradersim::ReceiveMsg,this,_key));
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun","Start")
			.WithField("key",_key)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"tradersim start ReceiveMsg thread exception");	
	}
}

void tradersim::Stop()
{
	if (nullptr != _thread_ptr)
	{
		m_run_receive_msg.store(false);
		_thread_ptr->join();		
	}	

	SaveUserDataFile();
}

void tradersim::ReceiveMsg(const std::string& key)
{
	std::string strKey = key;
	char buf[MAX_MSG_LENTH + 1];
	unsigned int priority = 0;
	boost::interprocess::message_queue::size_type recvd_size = 0;
	while (m_run_receive_msg.load())
	{
		try
		{
			memset(buf, 0, sizeof(buf));
			boost::posix_time::ptime tm = boost::get_system_time()
				+ boost::posix_time::milliseconds(100);
			bool flag = _in_mq_ptr->timed_receive(buf, sizeof(buf), recvd_size, priority, tm);
			if (!m_run_receive_msg.load())
			{
				break;
			}
			if (!flag)
			{
				_ios.post(boost::bind(&tradersim::OnIdle, this));
				continue;
			}
			std::string line = buf;
			if (line.empty())
			{
				continue;
			}
			int connId = -1;
			std::string msg = "";
			int nPos = line.find_first_of('|');
			if ((nPos <= 0) || (nPos + 1 >= line.length()))
			{
				Log().WithField("fun","ReceiveMsg")
					.WithField("key",_key)
					.WithField("msgcontent",line)
					.Log(LOG_ERROR,"tradersim ReceiveMsg is invalid!");				
				continue;
			}
			else
			{
				std::string strId = line.substr(0, nPos);
				connId = atoi(strId.c_str());
				msg = line.substr(nPos + 1);
				std::shared_ptr<std::string> msg_ptr(new std::string(msg));
				_ios.post(boost::bind(&tradersim::ProcessInMsg
					, this, connId, msg_ptr));
			}
		}
		catch (const std::exception& ex)
		{
			Log().WithField("fun","ReceiveMsg")
				.WithField("key",_key)
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"tradersim ReceiveMsg exception!");			
		}
	}
}

void tradersim::ProcessReqLogIn(int connId, ReqLogin& req)
{
	Log().WithField("fun","ProcessReqLogIn")
		.WithField("key",_key)
		.WithField("bid",req.bid)
		.WithField("user_name",req.user_name)
		.WithField("client_ip",req.client_ip)
		.WithField("client_port",req.client_port)
		.WithField("client_app_id",req.client_app_id)
		.WithField("client_system_info_length",(int)req.client_system_info.length())
		.WithField("front",req.front)
		.WithField("broker_id",req.broker_id)
		.WithField("connId",connId)
		.Log(LOG_INFO,"tradersim ProcessReqLogIn");

	//如果已经登录成功
	if (m_b_login)
	{
		//判断是否重复登录
		bool flag = false;
		for (auto id : m_logined_connIds)
		{
			if (id == connId)
			{
				flag = true;
				break;
			}
		}
				
		if (flag)
		{
			OutputNotifySycn(connId,400,u8"重复发送登录请求!");
			return;
		}
		
		//简单比较登陆凭证,判断是否能否成功登录
		if ((_req_login.bid == req.bid)
			&& (_req_login.user_name == req.user_name)
			&& (_req_login.password == req.password))
		{
			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("loginresult",true)
				.WithField("loginstatus",1)
				.WithField("connId",connId)
				.Log(LOG_INFO,"m_b_login is true");

			if (0 != connId)
			{
				//加入登录客户端列表
				m_logined_connIds.push_back(connId);

				OutputNotifySycn(connId,401,u8"登录成功");

				char json_str[1024];
				sprintf(json_str, (u8"{"\
					"\"aid\": \"rtn_data\","\
					"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
					"\"user_id\" : \"%s\","\
					"\"trading_day\" : \"%s\""
					"}}}}]}")
					, _req_login.user_name.c_str()
					, _req_login.user_name.c_str()
					, g_config.trading_day.c_str());
				
				std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
				SendMsg(connId, msg_ptr);

				//发送用户数据
				SendUserDataImd(connId);			
			}
		}
		else
		{
			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("loginresult",false)
				.WithField("loginstatus",1)
				.WithField("connId",connId)
				.Log(LOG_INFO,"m_b_login is true");			

			if (0 != connId)
			{
				OutputNotifySycn(connId,402,u8"账户和密码不匹配!");
			}			
		}
	}
	else
	{
		_req_login = req;
		auto it = g_config.brokers.find(_req_login.bid);
		_req_login.broker = it->second;

		if (!g_config.user_file_path.empty())
		{
			m_user_file_path = g_config.user_file_path + "/" + _req_login.bid;

			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("connId",connId)
				.WithField("fileName",m_user_file_path)
				.Log(LOG_INFO,"g_config user_file_path is not empty");			
		}
		else
		{
			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("connId",connId)
				.WithField("fileName",m_user_file_path)
				.Log(LOG_INFO,"g_config user_file_path is empty");			
		}

		m_user_id = _req_login.user_name;
		m_data.user_id = m_user_id;
		LoadUserDataFile();
		m_loging_connectId = connId;
		m_b_login = WaitLogIn();
		if (m_b_login.load())
		{
			m_peeking_message = true;

			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("loginresult",true)
				.WithField("loginstatus", 0)
				.WithField("connId",connId)
				.Log(LOG_INFO,"trade sim login success");			

			OnInit();

			SetExchangeTime();
			
			if (connId != 0)
			{
				//加入登录客户端列表
				m_logined_connIds.push_back(connId);

				OutputNotifySycn(connId,401,u8"登录成功");

				char json_str[1024];
				sprintf(json_str, (u8"{"\
					"\"aid\": \"rtn_data\","\
					"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
					"\"user_id\" : \"%s\","\
					"\"trading_day\" : \"%s\""
					"}}}}]}")
					, m_user_id.c_str()
					, m_user_id.c_str()
					, g_config.trading_day.c_str());

				std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
				SendMsg(connId,msg_ptr);
			}
			
			//加载条件单数据
			m_condition_order_manager.Load(_req_login.bid,
				_req_login.user_name,
				_req_login.password,
				g_config.trading_day);

			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)	
				.WithField("connId",connId)
				.Log(LOG_INFO,"load condition order success");
		}
		else
		{
			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",req.bid)
				.WithField("user_name",req.user_name)
				.WithField("loginresult",false)
				.WithField("loginstatus",0)
				.WithField("connId",connId)
				.Log(LOG_INFO,"trade sim login fail");		

			if (connId != 0)
			{
				OutputNotifySycn(connId,403,u8"用户登录失败!");
			}			
		}
	}
}

bool tradersim::WaitLogIn()
{
	unsigned seed = time(0);
	srand(seed);
	m_session_id = rand();
	return true;
}

void tradersim::CloseConnection(int nId)
{
	Log().WithField("fun","CloseConnection")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("connId",nId)
		.Log(LOG_INFO,"tradersim CloseConnection");

	for (std::vector<int>::iterator it = m_logined_connIds.begin();
		it != m_logined_connIds.end(); it++)
	{
		if (*it == nId)
		{
			m_logined_connIds.erase(it);
			break;
		}
	}
}

bool tradersim::IsConnectionLogin(int nId)
{
	bool flag = false;
	for (auto connId : m_logined_connIds)
	{
		if (connId == nId)
		{
			flag = true;
			break;
		}
	}
	return flag;
}

void tradersim::ProcessInMsg(int connId, std::shared_ptr<std::string> msg_ptr)
{
	if (nullptr == msg_ptr)
	{
		return;
	}

	std::string& msg = *msg_ptr;
	//一个特殊的消息
	if (msg == CLOSE_CONNECTION_MSG)
	{
		CloseConnection(connId);
		return;
	}
	
	SerializerTradeBase ss;
	if (!ss.FromString(msg.c_str()))
	{
		Log().WithField("fun","ProcessInMsg")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("connId",connId)
			.WithField("msgcontent",msg)
			.Log(LOG_WARNING,"tradersim parse json msg fail");		
		return;
	}
	
	ReqLogin req;
	ss.ToVar(req);
	if (req.aid == "req_login")
	{
		ProcessReqLogIn(connId,req);
	}
	else
	{
		if (!m_b_login)
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)				
				.Log(LOG_WARNING,"trade sim receive other msg before login");			
			return;
		}

		if (!IsConnectionLogin(connId)
			&& (connId != 0))
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)
				.Log(LOG_WARNING,"trade sim receive other msg which from not login connecion");		
			return;
		}

		SerializerSim ss;
		if (!ss.FromString(msg.c_str()))
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)
				.WithField("msgcontent",msg)
				.Log(LOG_WARNING,"tradersim parse json fail");			
			return;
		}
			
		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ss.m_doc));
		if (!dt || !dt->IsString())
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)			
				.Log(LOG_WARNING,"tradersim receive invalid json fail");			
			return;
		}
			
		std::string aid = dt->GetString();
		if (aid == "peek_message")
		{
			OnClientPeekMessage();
		}
		else if (aid == "req_reconnect_trade")
		{
			if (connId != 0)
			{
				return;
			}

			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)
				.WithPack("diff_req_pack",msg)
				.Log(LOG_INFO,"receive req reconnect trade msg");

			SerializerConditionOrderData ns;
			if (!ns.FromString(msg.c_str()))
			{
				return;
			}
			req_reconnect_trade_instance req_reconnect_trade;
			ns.ToVar(req_reconnect_trade);
			for (auto id : req_reconnect_trade.connIds)
			{
				m_logined_connIds.push_back(id);
			}
		}
		else if (aid == "insert_order")
		{
			ActionOrder action_insert_order;
			ss.ToVar(action_insert_order);
			OnClientReqInsertOrder(action_insert_order);
		}
		else if (aid == "cancel_order")
		{
			ActionOrder action_cancel_order;
			ss.ToVar(action_cancel_order);
			OnClientReqCancelOrder(action_cancel_order);
		}
		else if (aid == "req_transfer")
		{
			ActionTransfer action_transfer;
			ss.ToVar(action_transfer);
			OnClientReqTransfer(action_transfer);
		}		
		else if (aid == "insert_condition_order")
		{
			m_condition_order_manager.InsertConditionOrder(msg);
		}
		else if (aid == "cancel_condition_order")
		{
			m_condition_order_manager.CancelConditionOrder(msg);
		}
		else if (aid == "pause_condition_order")
		{
			m_condition_order_manager.PauseConditionOrder(msg);
		}
		else if (aid == "resume_condition_order")
		{
			m_condition_order_manager.ResumeConditionOrder(msg);
		}
		else if (aid == "qry_his_condition_order")
		{
			m_condition_order_manager.QryHisConditionOrder(connId,msg);
		}
		else if (aid == "req_ccos_status")
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)
				.Log(LOG_INFO,"trade sim receive ccos msg");			
			if (connId != 0)
			{
				return;
			}
			m_condition_order_manager.ChangeCOSStatus(msg);
		}
		else if (aid == "req_start_ctp")
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connId",connId)
				.Log(LOG_INFO,"trade sim receive req_start_ctp msg");			
		}
		else if (aid == "req_stop_ctp")
		{
			Log().WithField("fun","ProcessInMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("connid",connId)
				.Log(LOG_INFO,"trade sim receive req_stop_ctp msg");					
		}
	}
}

void tradersim::OnIdle()
{
	//尝试匹配成交
	TryOrderMatch();

	//重算所有持仓项的持仓盈亏和浮动盈亏
	RecaculatePositionAndFloatProfit();
	
	//检查时间触发的条件单
	CheckTimeConditionOrder();

	//检查价格触发的条件单
	CheckPriceConditionOrder();	

	long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (m_peeking_message && (m_next_send_dt < now))
	{
		m_next_send_dt = now + 100;
		SendUserData();
	}
}

void tradersim::OutputNotifySycn(int connId, long notify_code
	, const std::string& notify_msg, const char* level
	, const char* type)
{
	//构建数据包
	SerializerTradeBase nss;
	rapidjson::Pointer("/aid").Set(*nss.m_doc,"rtn_data");

	rapidjson::Value node_message;
	node_message.SetObject();
	node_message.AddMember("type", rapidjson::Value(type, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	node_message.AddMember("level", rapidjson::Value(level, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	node_message.AddMember("code", notify_code, nss.m_doc->GetAllocator());
	node_message.AddMember("session_id", m_session_id, nss.m_doc->GetAllocator());
	node_message.AddMember("content", rapidjson::Value(notify_msg.c_str()
		, nss.m_doc->GetAllocator()).Move()
		, nss.m_doc->GetAllocator());
	
	rapidjson::Pointer("/data/0/notify/" + GenerateGuid()).Set(*nss.m_doc, node_message);
	std::string json_str;
	nss.ToString(&json_str);

	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	SendMsg(connId, msg_ptr);
}

void tradersim::OutputNotifyAllSycn(long notify_code
	, const std::string& ret_msg, const char* level
	, const char* type)
{
	//构建数据包
	SerializerTradeBase nss;
	rapidjson::Pointer("/aid").Set(*nss.m_doc,"rtn_data");

	rapidjson::Value node_message;
	node_message.SetObject();
	node_message.AddMember("type", rapidjson::Value(type, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	node_message.AddMember("level", rapidjson::Value(level, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	node_message.AddMember("code", notify_code, nss.m_doc->GetAllocator());
	node_message.AddMember("session_id", m_session_id, nss.m_doc->GetAllocator());
	node_message.AddMember("content", rapidjson::Value(ret_msg.c_str()
		, nss.m_doc->GetAllocator()).Move()
		, nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/notify/" + GenerateGuid()).Set(*nss.m_doc,node_message);

	std::string json_str;
	nss.ToString(&json_str);

	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	SendMsgAll(msg_ptr);
}

void tradersim::SendMsgAll(std::shared_ptr<std::string> msg_ptr)
{
	if (nullptr == msg_ptr)
	{
		return;
	}

	if (nullptr == _out_mq_ptr)
	{
		return;
	}

	if (m_logined_connIds.empty())
	{
		return;
	}

	std::string& msg = *msg_ptr;

	std::stringstream ss;
	for (int i = 0; i < m_logined_connIds.size(); ++i)
	{
		if ((i + 1) == m_logined_connIds.size())
		{
			ss << m_logined_connIds[i];
		}
		else
		{
			ss << m_logined_connIds[i] << "|";
		}
	}

	std::string strIds = ss.str();
	msg = strIds + "#" + msg;

	size_t totalLength = msg.length();
	if (totalLength > MAX_MSG_LENTH)
	{
		try
		{
			_out_mq_ptr->send(BEGIN_OF_PACKAGE.c_str(), BEGIN_OF_PACKAGE.length(), 0);
			const char* buffer = msg.c_str();
			size_t start_pos = 0;
			while (true)
			{
				if ((start_pos + MAX_MSG_LENTH) < totalLength)
				{
					_out_mq_ptr->send(buffer + start_pos, MAX_MSG_LENTH, 0);
				}
				else
				{
					_out_mq_ptr->send(buffer + start_pos, totalLength - start_pos, 0);
					break;
				}
				start_pos += MAX_MSG_LENTH;
			}
			_out_mq_ptr->send(END_OF_PACKAGE.c_str(), END_OF_PACKAGE.length(), 0);
		}
		catch (std::exception& ex)
		{
			Log().WithField("fun","SendMsgAll")
				.WithField("key",_key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("msglen",(int)msg.length())
				.WithField("errmsg",ex.what())				
				.Log(LOG_ERROR,"SendMsg exception");
		}
	}
	else
	{
		try
		{
			_out_mq_ptr->send(msg.c_str(),totalLength,0);
		}
		catch (std::exception& ex)
		{
			Log().WithField("fun","SendMsgAll")
				.WithField("key",_key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("msglen",(int)totalLength)
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"SendMsg exception");
		}
	}
}

void tradersim::SendMsg(int connId, std::shared_ptr<std::string> msg_ptr)
{
	if (nullptr == msg_ptr)
	{
		return;
	}

	if (nullptr == _out_mq_ptr)
	{
		return;
	}

	std::string& msg = *msg_ptr;
	std::stringstream ss;
	ss << connId << "#";
	msg = ss.str() + msg;

	size_t totalLength = msg.length();
	if (totalLength > MAX_MSG_LENTH)
	{
		try
		{
			_out_mq_ptr->send(BEGIN_OF_PACKAGE.c_str(), BEGIN_OF_PACKAGE.length(), 0);
			const char* buffer = msg.c_str();
			size_t start_pos = 0;
			while (true)
			{
				if ((start_pos + MAX_MSG_LENTH) < totalLength)
				{
					_out_mq_ptr->send(buffer + start_pos, MAX_MSG_LENTH, 0);
				}
				else
				{
					_out_mq_ptr->send(buffer + start_pos, totalLength - start_pos, 0);
					break;
				}
				start_pos += MAX_MSG_LENTH;
			}
			_out_mq_ptr->send(END_OF_PACKAGE.c_str(), END_OF_PACKAGE.length(), 0);
		}
		catch (std::exception& ex)
		{
			Log().WithField("fun","SendMsg")
				.WithField("key",_key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("msglen",(int)msg.length())
				.WithField("errmsg",ex.what())
				.WithField("connId",connId)
				.Log(LOG_ERROR,"SendMsg exception");			
		}
	}
	else
	{
		try
		{
			_out_mq_ptr->send(msg.c_str(),totalLength,0);
		}
		catch (std::exception& ex)
		{
			Log().WithField("fun","SendMsg")
				.WithField("key",_key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("msglen",(int)totalLength)
				.WithField("errmsg",ex.what())
				.WithField("connId",connId)
				.Log(LOG_ERROR,"SendMsg exception");			
		}
	}
}

void tradersim::SendUserDataImd(int connectId)
{
	//构建数据包		
	SerializerTradeBase nss;
	nss.dump_all = true;

	rapidjson::Pointer("/aid").Set(*nss.m_doc,"rtn_data");

	rapidjson::Value node_data;
	nss.FromVar(m_data, &node_data);

	rapidjson::Value node_user_id;
	node_user_id.SetString(m_user_id,nss.m_doc->GetAllocator());

	rapidjson::Value node_user;
	node_user.SetObject();
	node_user.AddMember(node_user_id,node_data,nss.m_doc->GetAllocator());

	rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc,node_user);

	std::string json_str;
	nss.ToString(&json_str);

	//发送	
	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	SendMsg(connectId, msg_ptr);

	//发送条件单数据
	SerializerConditionOrderData coss;
	rapidjson::Pointer("/aid").Set(*coss.m_doc,"rtn_condition_orders");
	rapidjson::Pointer("/user_id").Set(*coss.m_doc,m_condition_order_data.user_id);
	rapidjson::Pointer("/trading_day").Set(*coss.m_doc,m_condition_order_data.trading_day);
	std::vector<ConditionOrder> condition_orders;
	bool flag = false;
	for (auto& it : m_condition_order_data.condition_orders)
	{
		flag = true;
		condition_orders.push_back(it.second);
	}

	if (!flag)
	{
		return;
	}

	rapidjson::Value co_node_data;
	coss.FromVar(condition_orders,&co_node_data);
	rapidjson::Pointer("/condition_orders").Set(*coss.m_doc, co_node_data);
	coss.ToString(&json_str);

	std::shared_ptr<std::string> msg_ptr2(new std::string(json_str));
	_ios.post(boost::bind(&tradersim::SendMsg, this, connectId, msg_ptr2));
}

void tradersim::SendUserData()
{
	if (!m_peeking_message)
	{
		return;
	}

	if (m_something_changed)
	{
		//构建数据包
		m_data.m_trade_more_data = false;

		SerializerTradeBase nss;
		rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");

		rapidjson::Value node_data;
		nss.FromVar(m_data, &node_data);

		rapidjson::Value node_user_id;
		node_user_id.SetString(m_user_id, nss.m_doc->GetAllocator());

		rapidjson::Value node_user;
		node_user.SetObject();
		node_user.AddMember(node_user_id, node_data, nss.m_doc->GetAllocator());

		rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc, node_user);
		std::string json_str;
		nss.ToString(&json_str);

		//发送
		std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
		SendMsgAll(msg_ptr);		
	}

	//发送条件单数据
	SerializerConditionOrderData coss;
	rapidjson::Pointer("/aid").Set(*coss.m_doc,"rtn_condition_orders");
	rapidjson::Pointer("/user_id").Set(*coss.m_doc, m_condition_order_data.user_id);
	rapidjson::Pointer("/trading_day").Set(*coss.m_doc,m_condition_order_data.trading_day);
	std::vector<ConditionOrder> condition_orders;
	bool flag = false;
	for (auto& it : m_condition_order_data.condition_orders)
	{
		if (it.second.changed)
		{
			flag = true;
			condition_orders.push_back(it.second);
			it.second.changed = false;
		}
	}
	if (flag)
	{
		rapidjson::Value co_node_data;
		coss.FromVar(condition_orders, &co_node_data);
		rapidjson::Pointer("/condition_orders").Set(*coss.m_doc,co_node_data);
		std::string json_str;
		coss.ToString(&json_str);		
		//发送
		std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
		SendMsgAll(msg_ptr);		
	}
		
	if (!m_something_changed)
	{
		return;
	}

	m_something_changed = false;
	m_peeking_message = false;
}

#pragma endregion


#pragma region businesslogic

void SerializerSim::DefineStruct(ActionOrder& d)
{
	AddItem(d.aid,"aid");
	AddItem(d.order_id,"order_id");
	AddItem(d.user_id,"user_id");
	AddItem(d.exchange_id,"exchange_id");
	AddItem(d.ins_id,"instrument_id");
	AddItemEnum(d.direction, "direction",{
		{ kDirectionBuy,"BUY"},
		{ kDirectionSell,"SELL"},
		});
	AddItemEnum(d.offset, "offset",{
		{ kOffsetOpen,"OPEN" },
		{ kOffsetClose,"CLOSE"},
		{ kOffsetCloseToday,"CLOSETODAY" },
		});
	AddItemEnum(d.price_type,"price_type",{
		{ kPriceTypeLimit,"LIMIT"},
		{ kPriceTypeAny,"ANY"},
		{ kPriceTypeBest,"BEST"},
		{ kPriceTypeFiveLevel,"FIVELEVEL"},
		});
	AddItemEnum(d.volume_condition,"volume_condition",{
		{ kOrderVolumeConditionAny,"ANY"},
		{ kOrderVolumeConditionMin,"MIN"},
		{ kOrderVolumeConditionAll,"ALL"},
		});
	AddItemEnum(d.time_condition,"time_condition",{
		{ kOrderTimeConditionIOC,"IOC" },
		{ kOrderTimeConditionGFS,"GFS" },
		{ kOrderTimeConditionGFD,"GFD" },
		{ kOrderTimeConditionGTD,"GTD" },
		{ kOrderTimeConditionGTC,"GTC" },
		{ kOrderTimeConditionGFA,"GFA" },
		});
	AddItem(d.volume,"volume");
	AddItem(d.limit_price,"limit_price");
}

void SerializerSim::DefineStruct(ActionTransfer& d)
{
	AddItem(d.currency,"currency");
	AddItem(d.amount,"amount");
}

void tradersim::OnInit()
{
	m_account = &(m_data.m_accounts["CNY"]);
	if (m_account->static_balance < 1.0)
	{
		Log().WithField("fun","OnIni")
			.WithField("key",_key)			
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.Log(LOG_INFO,"sim init new balance");

		m_account->user_id = m_user_id;
		m_account->currency = "CNY";
		m_account->pre_balance = 0;
		m_account->deposit = 1000000;
		m_account->withdraw = 0;
		m_account->close_profit = 0;
		m_account->commission = 0;
		m_account->static_balance = 1000000;
		m_account->position_profit = 0;
		m_account->float_profit = 0;
		m_account->balance = m_account->static_balance;
		m_account->frozen_margin = 0;
		m_account->frozen_commission = 0;
		m_account->margin = 0;
		m_account->available = m_account->static_balance;
		m_account->risk_ratio = 0;
		m_account->changed = true;
	}
	auto bank = &(m_data.m_banks["SIM"]);
	bank->bank_id = "SIM";
	bank->bank_name = u8"模拟银行";
	bank->changed = true;
	m_something_changed = true;
	Log().WithField("fun","OnIni")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.Log(LOG_INFO,"sim OnInit Finish");	
}

void tradersim::LoadUserDataFile()
{	
	if (m_user_file_path.empty())
	{
		Log().WithField("fun","LoadUserDataFile")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_ERROR,"m_user_file_path is empty");		
		return;
	}
	
	//加载存档文件
	std::string fn = m_user_file_path + "/" + m_user_id;

	Log().WithField("fun","LoadUserDataFile")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("fileName",fn)
		.Log(LOG_INFO,"load user data file");	

	//加载存档文件
	SerializerTradeBase nss;
	bool loadfile=nss.FromFile(fn.c_str());
	if (!loadfile)
	{
		Log().WithField("fun","LoadUserDataFile")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("fileName",fn)
			.Log(LOG_WARNING,"load user data file failed!");		
	}

	nss.ToVar(m_data);
	//重建内存中的索引表和指针
	for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end();)
	{
		Position& position = it->second;
				
		position.ins = GetInstrument(position.symbol());
		if (position.ins && !position.ins->expired)
		{
			++it;
		}
		else if (position.ins && position.ins->expired)
		{
			Log().WithField("fun","LoadUserDataFile")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("fileName",fn)
				.WithField("exchange_id", position.exchange_id)
				.WithField("instrument_id", position.instrument_id)				
				.Log(LOG_WARNING,"sysmbol is expired!");
			it = m_data.m_positions.erase(it);			
		}
		else
		{
			Log().WithField("fun","LoadUserDataFile")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("fileName",fn)
				.WithField("exchange_id",position.exchange_id)
				.WithField("instrument_id",position.instrument_id)
				.Log(LOG_WARNING,"miss symbol in position!");
			it = m_data.m_positions.erase(it);			
		}			
	}
	
	//如果不是当天的存档文件,则需要调整
	if (m_data.trading_day != g_config.trading_day)
	{
		Log().WithField("fun","LoadUserDataFile")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("old_trading_day",m_data.trading_day)
			.WithField("trading_day",g_config.trading_day)
			.Log(LOG_INFO,"diffrent trading day!");		

		//清空全部委托记录
		m_data.m_orders.clear();

		//清空全部成交记录
		m_data.m_trades.clear();

		//清空全部转账记录
		m_data.m_transfers.clear();

		//用户权益转为昨权益
		for (auto it = m_data.m_accounts.begin(); it != m_data.m_accounts.end(); ++it)
		{
			Account& item = it->second;
			item.pre_balance += (item.close_profit - item.commission + item.deposit - item.withdraw);
			item.static_balance = item.pre_balance;
			item.close_profit = 0;
			item.commission = 0;
			item.withdraw = 0;
			item.deposit = 0;
			item.changed = true;

			//冻结保证金
			item.frozen_margin = 0;

			//冻结手续费
			item.frozen_commission = 0;	

			SerializerTradeBase nss;
			nss.FromVar(item);
			std::string msg;
			nss.ToString(&msg);
			Log().WithField("fun", "LoadUserDataFile")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("old_trading_day", m_data.trading_day)
				.WithField("trading_day", g_config.trading_day)
				.WithPack("account_pack",msg)
				.Log(LOG_INFO, "diffrent trading day!");
		}

		//重算持仓情况(上期和原油持仓手数全部移动到昨仓,其它全部为今仓)
		for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end(); ++it)
		{
			Position& item = it->second;

			//对历史数据进行清理
			ClearUserPosition(item);
			
			//上期和原油交易所
			if ((item.exchange_id == "SHFE" || item.exchange_id == "INE"))
			{
				//今天多头持仓调整为历史多头持仓
				item.volume_long_his = item.volume_long_his + item.volume_long_today;
				item.volume_long_today = 0;

				//今天多头开仓市值调整为历史多头开仓市值
				item.open_cost_long_his = item.open_cost_long_his + item.open_cost_long_today;
				item.open_cost_long_today = 0;

				//今天多头持仓成本调整为历史多头持仓成本
				item.position_cost_long_his = item.position_cost_long_his + item.position_cost_long_today;
				item.position_cost_long_today = 0;

				//今天多头保证金占用调整为历史多头保证金占用
				item.margin_long_his = item.margin_long_his + item.margin_long_today;
				item.margin_long_today = 0;

				//今日开盘前的多头持仓手数
				item.volume_long_yd = item.volume_long_his;

				//今天空头持仓调整为历史空头持仓
				item.volume_short_his = item.volume_short_his + item.volume_short_today;
				item.volume_short_today = 0;

				//今天空头开仓市值调整为历史空头开仓市值
				item.open_cost_short_his = item.open_cost_short_his + item.open_cost_short_today;
				item.open_cost_short_today = 0;

				//今天空头持仓成本调整为历史空头持仓成本
				item.position_cost_short_his = item.position_cost_short_his + item.position_cost_short_today;
				item.position_cost_short_today = 0;

				//今天空头保证金占用调整为历史空头保证金占用
				item.margin_short_his = item.margin_short_his + item.margin_short_today;
				item.margin_short_today = 0;

				//今日开盘前的空头持仓手数
				item.volume_short_yd = item.volume_short_his;

				//今昨仓持仓情况(这里表示实际上的今昨仓,不是概念上的今昨仓)
				item.pos_long_his = item.volume_long_yd;
				item.pos_long_today = 0;
				item.pos_short_his = item.volume_short_yd;
				item.pos_short_today = 0;

				//多头冻结手数
				item.volume_long_frozen_today = 0;
				item.volume_long_frozen_his = 0;
				item.volume_long_frozen = 0;

				//空头冻结手数
				item.volume_short_frozen_today = 0;
				item.volume_short_frozen_his = 0;
				item.volume_short_frozen = 0;

				//冻结保证金
				item.frozen_margin = 0;
			}
			//其它交易所
			else
			{
				//历史和今天多头持仓				
				item.volume_long_today = item.volume_long_his + item.volume_long_today;
				item.volume_long_his = 0;

				//多头开仓市值(今仓)
				item.open_cost_long_today = item.open_cost_long_his + item.open_cost_long_today;

				//多头开仓市值(昨仓)
				item.open_cost_long_his = 0;

				//多头持仓成本(今仓)
				item.position_cost_long_today = item.position_cost_long_his + item.position_cost_long_today;

				//多头持仓成本(昨仓)
				item.position_cost_long_his = 0;
				
				//多头保证金占用(今仓)
				item.margin_long_today = item.margin_long_his + item.margin_long_today;

				//多头保证金占用(昨仓)
				item.margin_long_his = 0;

				//今日开盘前的多头持仓手数
				item.volume_long_yd = item.volume_long_today;

				//历史和今天空头持仓
				item.volume_short_today = item.volume_short_his + item.volume_short_today;
				item.volume_short_his = 0;

				//空头开仓市值(今仓)
				item.open_cost_short_today = item.open_cost_short_his + item.open_cost_short_today;

				//空头开仓市值(昨仓)
				item.open_cost_short_his = 0;

				//空头持仓成本(今仓)	
				item.position_cost_short_today = item.position_cost_short_his + item.position_cost_short_today;

				//空头持仓成本(昨仓)
				item.position_cost_short_his = 0;

				//空头保证金占用(今仓)	
				item.margin_short_today = item.margin_short_his + item.margin_short_today;

				//空头保证金占用(昨仓)
				item.margin_short_his = 0;

				//今日开盘前的空头持仓手数
				item.volume_short_yd = item.volume_short_today;

				//今昨仓持仓情况(这里表示实际上的今昨仓,不是概念上的今昨仓)
				item.pos_long_his = item.volume_long_yd;
				item.pos_long_today = 0;
				item.pos_short_his = item.volume_short_yd;
				item.pos_short_today = 0;

				//多头冻结手数
				item.volume_long_frozen_today = 0;
				item.volume_long_frozen_his = 0;
				item.volume_long_frozen = 0;

				//空头冻结手数
				item.volume_short_frozen_today = 0;
				item.volume_short_frozen_his = 0;
				item.volume_short_frozen = 0;

				//冻结保证金
				item.frozen_margin = 0;
			}
			
			item.changed = true;

			SerializerTradeBase nss;
			nss.FromVar(item);
			std::string msg;
			nss.ToString(&msg);
			Log().WithField("fun", "LoadUserDataFile")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("old_trading_day", m_data.trading_day)
				.WithField("trading_day", g_config.trading_day)
				.WithPack("pos_pack",msg)
				.Log(LOG_INFO, "diffrent trading day!");

		}

		//删除持仓为零的持仓项
		for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end();)
		{
			Position& pos = it->second;

			if ((pos.volume_long_today == 0)
				&& (pos.volume_long_his == 0)
				&& (pos.volume_short_today == 0)
				&& (pos.volume_short_his == 0)
				&&(pos.pos_long_today==0)
				&&(pos.pos_long_his == 0)
				&& (pos.pos_short_today == 0)
				&& (pos.pos_short_his == 0)
				)
			{
				it = m_data.m_positions.erase(it);
			}
			else
			{
				++it;
			}			
		}

		m_data.trading_day = g_config.trading_day;
	}
	else
	{
		Log().WithField("fun","LoadUserDataFile")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("fileName",fn)	
		.WithField("trading_day",m_data.trading_day)
		.Log(LOG_INFO,"same trading day!");		

		for (auto it = m_data.m_orders.begin(); it != m_data.m_orders.end(); ++it)
		{
			m_alive_order_set.insert(&(it->second));
		}
	}
}

void tradersim::ClearUserPosition(Position& item)
{
	if (item.volume_long_today < 0)
	{
		item.volume_long_today = 0;
	}

	if (item.volume_long_his < 0)
	{
		item.volume_long_his = 0;
	}

	item.volume_long = item.volume_long_today + item.volume_long_his;

	if (item.volume_long_frozen_today < 0)
	{
		item.volume_long_frozen_today = 0;
	}

	if (item.volume_long_frozen_his < 0)
	{
		item.volume_long_frozen_his = 0;
	}

	item.volume_long_frozen = item.volume_long_frozen_today + item.volume_long_frozen_his;

	if (item.volume_short_today < 0)
	{
		item.volume_short_today = 0;
	}

	if (item.volume_long_his < 0)
	{
		item.volume_short_his = 0;
	}

	item.volume_short = item.volume_short_today + item.volume_short_his;

	if (item.volume_short_frozen_today < 0)
	{
		item.volume_short_frozen_today = 0;
	}

	if (item.volume_short_frozen_his < 0)
	{
		item.volume_short_frozen_his = 0;
	}

	item.volume_short_frozen = item.volume_short_frozen_his + item.volume_short_frozen_today;

	if (item.volume_long_yd < 0)
	{
		item.volume_long_yd = 0;
	}

	if (item.volume_short_yd < 0)
	{
		item.volume_short_yd = 0;
	}

	if (item.pos_long_his < 0)
	{
		item.pos_long_his = 0;
	}

	if (item.pos_long_today < 0)
	{
		item.pos_long_today = 0;
	}

	if (item.pos_short_his < 0)
	{
		item.pos_short_his = 0;
	}

	if (item.pos_short_today < 0)
	{
		item.pos_short_today = 0;
	}		

	if (!IsValid(item.open_price_long))
	{
		item.open_price_long = 0;
	}

	if (!IsValid(item.open_price_short))
	{
		item.open_price_short = 0;
	}

	if (!IsValid(item.open_cost_long))
	{
		item.open_cost_long = 0;
	}

	if (!IsValid(item.open_cost_short))
	{
		item.open_cost_short = 0;
	}

	if (!IsValid(item.position_price_long))
	{
		item.position_price_long = 0;
	}

	if (!IsValid(item.position_price_short))
	{
		item.position_price_short = 0;
	}

	if (!IsValid(item.position_cost_long))
	{
		item.position_cost_long = 0;
	}

	if (!IsValid(item.position_cost_short))
	{
		item.position_cost_short = 0;
	}

	if (!IsValid(item.last_price))
	{
		item.last_price = 0;
	}

	if (!IsValid(item.float_profit_long))
	{
		item.last_price = 0;
	}
}

void tradersim::SaveUserDataFile()
{
	if (m_user_file_path.empty())
	{
		Log().WithField("fun","SaveUserDataFile")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)		
			.Log(LOG_ERROR,"user file path is empty");
		return;
	}

	std::string fn = m_user_file_path + "/" + m_user_id;

	Log().WithField("fun","SaveUserDataFile")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("fileName",fn)
		.Log(LOG_INFO,"save user date file");
	
	SerializerTradeBase nss;
	nss.dump_all = true;
	m_data.trading_day = g_config.trading_day;
	nss.FromVar(m_data);
	bool saveFile=nss.ToFile(fn.c_str());
	if (!saveFile)
	{
		Log().WithField("fun","SaveUserDataFile")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("fileName",fn)
			.Log(LOG_ERROR,"save user date file fail!");		
	}
}

void tradersim::UpdateOrder(Order* order)
{
	order->seqno = m_last_seq_no++;
	order->changed = true;
	Position& position = GetPosition(order->exchange_id, order->instrument_id,_req_login.user_name);
	if (nullptr == position.ins)
	{
		Log().WithField("fun","UpdateOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_ERROR,"instrument of position is null!");
		return;
	}
	UpdatePositionVolume(&position);

	SerializerTradeBase nss;
	nss.FromVar(position);
	std::string msg;
	nss.ToString(&msg);
	Log().WithField("fun","UpdateOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)	
		.WithPack("pos_pack",msg)
		.Log(LOG_INFO,"update order and position");
}

Position& tradersim::GetPosition(const std::string& exchange_id
	, const std::string& instrument_id
	, const std::string& user_id)
{
	std::string symbol = exchange_id + "." + instrument_id;
	std::map<std::string, Position>::iterator it = m_data.m_positions.find(symbol);
	if (it == m_data.m_positions.end())
	{
		Position pos;
		pos.exchange_id = exchange_id;
		pos.instrument_id = instrument_id;
		pos.user_id = user_id;
		m_data.m_positions.insert(std::map<std::string, Position>::value_type(symbol, pos));
	}
	Position& position = m_data.m_positions[symbol];
	return position;
}

void tradersim::UpdatePositionVolume(Position* position)
{
	//重算如下指标

	//冻结保证金
	position->frozen_margin = 0;
	//多头冻结手数(今仓)
	position->volume_long_frozen_today = 0;
	//多头冻结手数(昨仓)
	position->volume_long_frozen_his = 0;
	//空头冻结手数(今仓)
	position->volume_short_frozen_today = 0;
	//空头冻结手数(昨仓)
	position->volume_short_frozen_his = 0;

	for (auto it_order = m_alive_order_set.begin()
		; it_order != m_alive_order_set.end()
		; ++it_order)
	{
		Order* order = *it_order;

		if (order->status != kOrderStatusAlive)
		{
			continue;
		}

		if (position->instrument_id != order->instrument_id)
		{
			continue;
		}

		if (order->offset == kOffsetOpen)
		{
			//如果是开仓,冻结保证金加上本订单占用的冻结保证金
			position->frozen_margin += position->ins->margin * order->volume_left;
		}
		else
			//如果是平仓
		{
			//上期和原油交易所(区分平今平昨)
			if ((position->exchange_id == "SHFE" || position->exchange_id == "INE"))
			{
				//平今
				if (order->offset == kOffsetCloseToday)
				{
					if (order->direction == kDirectionBuy)
					{
						//今仓空头冻结手数加上本定单的冻结手数
						position->volume_short_frozen_today += order->volume_left;
					}
					else
					{
						//今仓多头冻结手数加上本定单的冻结手数
						position->volume_long_frozen_today += order->volume_left;
					}
				}
				//平昨
				else if (order->offset == kOffsetClose)
				{
					if (order->direction == kDirectionBuy)
					{
						//昨仓空头冻结手数加上本定单的冻结手数
						position->volume_short_frozen_his += order->volume_left;
					}
					else
					{
						//昨仓多头冻结手数加上本定单的冻结手数						
						position->volume_long_frozen_his += order->volume_left;
					}
				}
			}
			else
				//其它交易所(不区分平今平昨)
			{
				//如果是平仓,冻结手数需加上本订单的订单手数
				if (order->direction == kDirectionBuy)
				{
					//今仓空头冻结手数加上本定单的冻结手数
					position->volume_short_frozen_today += order->volume_left;
				}
				else
				{
					//今仓多头冻结手数加上本定单的冻结手数
					position->volume_long_frozen_today += order->volume_left;
				}
			}
		}
	}

	//总的多头冻结手数
	position->volume_long_frozen = position->volume_long_frozen_his + position->volume_long_frozen_today;

	//总的空头冻结手数
	position->volume_short_frozen = position->volume_short_frozen_his + position->volume_short_frozen_today;

	//总的多头持仓
	position->volume_long = position->volume_long_his + position->volume_long_today;

	//总的空头持仓
	position->volume_short = position->volume_short_his + position->volume_short_today;

	//多头保证金占用
	position->margin_long = position->ins->margin * position->volume_long;

	//空头保证金占用
	position->margin_short = position->ins->margin * position->volume_short;

	//总的保证金占用
	position->margin = position->margin_long + position->margin_short;
	
	if (position->volume_long > 0)
	{
		//计算多头开仓均价
		position->open_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);

		//计算多头持仓均价
		position->position_price_long = position->position_cost_long / (position->volume_long * position->ins->volume_multiple);
	}
	else
	{
		position->open_price_long = 0;
		position->position_price_long = 0;
	}

	if (position->volume_short > 0)
	{
		//计算空头开仓均价
		position->open_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);

		//计算空头持仓均价
		position->position_price_short = position->position_cost_short / (position->volume_short * position->ins->volume_multiple);
	}
	else
	{
		position->open_price_short = 0;
		position->position_price_short = 0;
	}

	position->changed = true;
}

TransferLog& tradersim::GetTransferLog(const std::string& seq_id)
{
	return m_data.m_transfers[seq_id];
}

void tradersim::TryOrderMatch()
{
	for (auto it_order = m_alive_order_set.begin(); it_order != m_alive_order_set.end(); )
	{
		Order* order = *it_order;
		if (order->status == kOrderStatusFinished)
		{
			it_order = m_alive_order_set.erase(it_order);
			continue;
		}
		else
		{
			CheckOrderTrade(order);
			++it_order;
		}
	}
}

void tradersim::RecaculatePositionAndFloatProfit()
{
	//重算所有持仓项的持仓盈亏和浮动盈亏

	//总持仓盈亏
	double total_position_profit = 0;

	////总浮动盈亏
	double total_float_profit = 0;

	//总保证金
	double total_margin = 0;

	//总冻结保证金
	double total_frozen_margin = 0.0;

	for (auto it = m_data.m_positions.begin()
		; it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;
		Position& ps = it->second;
		if (nullptr == ps.ins)
		{
			ps.ins = GetInstrument(symbol);
		}

		if (nullptr == ps.ins)
		{
			Log().WithField("fun","RecaculatePositionAndFloatProfit")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("exchange_id",ps.exchange_id)
				.WithField("instrument_id",ps.instrument_id)				
				.Log(LOG_ERROR,"miss symbol when processing position");			
			continue;
		}

		//得到最新价
		double last_price = ps.ins->last_price;
		//开盘前
		if (!IsValid(ps.ins->last_price) && !IsValid(ps.ins->settlement))
		{
			if (ps.market_status != 0)
			{
				ps.market_status = 0;
				ps.changed = true;
			}
			last_price = ps.ins->pre_close;
			if (!IsValid(last_price))
				last_price = ps.ins->pre_settlement;
		}
		else if (IsValid(ps.ins->last_price) && !IsValid(ps.ins->settlement))
		{
			if (ps.market_status != 1)
			{
				ps.market_status = 1;
				ps.changed = true;
			}
			last_price = ps.ins->last_price;
		}
		//收盘后
		else if (IsValid(ps.ins->last_price) && IsValid(ps.ins->settlement))
		{
			if (ps.market_status != 2)
			{
				ps.market_status = 2;
				ps.changed = true;
			}
			last_price = ps.ins->last_price;
		}			

		if ((IsValid(last_price) && (last_price != ps.last_price)) || ps.changed)
		{
			ps.last_price = last_price;

			//计算多头持仓盈亏(多头现在市值-多头持仓成本)
			ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;

			//计算空头持仓盈亏(空头持仓成本-空头现在市值)
			ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;

			//计算总持仓盈亏(多头持仓盈亏+空头持仓盈亏)
			ps.position_profit = ps.position_profit_long + ps.position_profit_short;

			//计算多头浮动盈亏(多头现在市值-多头开仓市值)
			ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;

			//计算空头浮动盈亏(空头开仓市值-空头现在市值)
			ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;

			//计算总浮动盈亏(多头浮动盈亏+空头浮动盈亏)
			ps.float_profit = ps.float_profit_long + ps.float_profit_short;

			ps.changed = true;
			m_something_changed = true;
		}

		//计算总的持仓盈亏
		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;

		//计算总的浮动盈亏
		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;

		//计算总的保证金占用
		if (IsValid(ps.margin))
			total_margin += ps.margin;

		//计算总的冻结保证金
		total_frozen_margin += ps.frozen_margin;
	}

	//重算资金账户
	if (m_something_changed)
	{
		//持仓盈亏
		m_account->position_profit = total_position_profit;

		//浮动盈亏
		m_account->float_profit = total_float_profit;

		//当前权益(动态),静态权益+浮动盈亏+平仓盈亏-手续费
		m_account->balance = m_account->static_balance
			+ m_account->float_profit
			+ m_account->close_profit
			- m_account->commission;

		//保证金占用
		m_account->margin = total_margin;

		//冻结保证金
		m_account->frozen_margin = total_frozen_margin;

		//计算可用资金(动态权益-保证金占用-冻结保证金)
		m_account->available = m_account->balance - m_account->margin - m_account->frozen_margin;

		//计算风险度
		if (IsValid(m_account->margin) && IsValid(m_account->balance) && !IsZero(m_account->balance))
			m_account->risk_ratio = m_account->margin / m_account->balance;
		else
			m_account->risk_ratio = NAN;

		m_account->changed = true;
	}
}

void tradersim::CheckOrderTrade(Order* order)
{
	SerializerTradeBase nss;
	nss.FromVar(*order);
	std::string msg;
	nss.ToString(&msg);

	auto ins = GetInstrument(order->symbol());
	if (order->price_type == kPriceTypeLimit)
	{
		if (order->limit_price - 0.0001 > ins->upper_limit)
		{
			Log().WithField("fun","CheckOrderTrade")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithPack("order_pack",msg)
				.Log(LOG_INFO,u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格超出涨停板");

			OutputNotifyAllSycn(404,u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格超出涨停板","WARNING");

			order->status = kOrderStatusFinished;

			UpdateOrder(order);
			return;
		}

		if (order->limit_price + 0.0001 < ins->lower_limit)
		{
			Log().WithField("fun", "CheckOrderTrade")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithPack("order_pack", msg)
				.Log(LOG_INFO, u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格跌破跌停板");

			OutputNotifyAllSycn(405,u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格跌破跌停板","WARNING");

			order->status = kOrderStatusFinished;

			UpdateOrder(order);
			return;
		}
	}
	
	if (order->direction == kDirectionBuy && IsValid(ins->ask_price1)
		&& (order->price_type == kPriceTypeAny || order->limit_price >= ins->ask_price1))
	{
		//用卖一价成交
		Log().WithField("fun","CheckOrderTrade")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("order_pack",msg)
			.Log(LOG_INFO,u8"to do trade with ask price 1");
		DoTrade(order, order->volume_left, ins->ask_price1);
	}
		
	if (order->direction == kDirectionSell && IsValid(ins->bid_price1)
		&& (order->price_type == kPriceTypeAny || order->limit_price <= ins->bid_price1))
	{
		//用买一价成交
		Log().WithField("fun","CheckOrderTrade")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("order_pack",msg)
			.Log(LOG_INFO,u8"to do trade with bid price 1");
		DoTrade(order, order->volume_left, ins->bid_price1);
	}		
}

void tradersim::DoTrade(Order* order, int volume, double price)
{
	//创建成交记录
	std::string trade_id = std::to_string(m_last_seq_no++);
	Trade* trade = &(m_data.m_trades[trade_id]);
	trade->trade_id = trade_id;
	trade->seqno = m_last_seq_no++;
	trade->user_id = order->user_id;
	trade->order_id = order->order_id;
	trade->exchange_trade_id = trade_id;
	trade->exchange_id = order->exchange_id;
	trade->instrument_id = order->instrument_id;
	trade->direction = order->direction;
	trade->offset = order->offset;
	trade->volume = volume;
	trade->price = price;
	trade->trade_date_time = GetLocalEpochNano();

	SerializerTradeBase nss;
	nss.FromVar(*trade);
	std::string msg;
	nss.ToString(&msg);

	Log().WithField("fun","DoTrade")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithPack("trade_pack", msg)
		.Log(LOG_INFO,"trade info");

	//生成成交通知
	std::stringstream ss;
	ss << u8"成交通知,合约:" << trade->exchange_id
		<< u8"." << trade->instrument_id << u8",手数:" << trade->volume;
	OutputNotifyAllSycn(406,ss.str().c_str());

	//调整委托单数据
	assert(volume <= order->volume_left);
	assert(order->status == kOrderStatusAlive);
	order->volume_left -= volume;
	if (order->volume_left == 0)
	{
		order->status = kOrderStatusFinished;
	}
	order->seqno = m_last_seq_no++;
	order->changed = true;

	nss.FromVar(*order);	
	nss.ToString(&msg);
	Log().WithField("fun", "DoTrade")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithPack("order_pack", msg)
		.Log(LOG_INFO, "order info after ajust");

	//调整持仓数据
	Position* position = &(m_data.m_positions[order->symbol()]);
	double commission = position->ins->commission * volume;
	trade->commission = commission;
	//如果是开仓
	if (order->offset == kOffsetOpen)
	{
		//多开
		if (order->direction == kDirectionBuy)
		{
			//多头持仓手数
			position->volume_long_today += volume;

			position->volume_long = position->volume_long_his + position->volume_long_today;

			//多头开仓市值
			position->open_cost_long += price * volume * position->ins->volume_multiple;

			//多头持仓成本
			position->position_cost_long += price * volume * position->ins->volume_multiple;

			//多头开仓均价
			position->open_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);

			//多头持仓均价
			position->position_price_long = position->position_cost_long / (position->volume_long * position->ins->volume_multiple);

			//实际上的多头持仓(今仓)
			position->pos_long_today  += volume;
		}
		//空开
		else
		{
			//空头持仓手数
			position->volume_short_today += volume;

			position->volume_short = position->volume_short_his + position->volume_short_today;

			//空头开仓市值
			position->open_cost_short += price * volume * position->ins->volume_multiple;

			//空头持仓成本
			position->position_cost_short += price * volume * position->ins->volume_multiple;

			//空头开仓均价
			position->open_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
			
			//空头持仓均价
			position->position_price_short = position->position_cost_short / (position->volume_short * position->ins->volume_multiple);

			//实际上的空头持仓(今仓)
			position->pos_short_today += volume;
		}
	}
	//如果是平仓
	else
	{
		double close_profit = 0;
		//上期和原油交易所(区分平今平昨)
		if ((position->exchange_id == "SHFE" || position->exchange_id == "INE"))
		{
			//平今
			if (order->offset == kOffsetCloseToday)
			{
				if (order->direction == kDirectionBuy)
				{
					//空头开仓市值
					position->open_cost_short = position->open_cost_short * (position->volume_short - volume) / position->volume_short;

					//空头持仓成本
					position->position_cost_short = position->position_cost_short * (position->volume_short - volume) / position->volume_short;

					//平仓盈亏
					close_profit = (position->position_price_short - price) * volume * position->ins->volume_multiple;

					//更新空头持仓手数(今仓)
					position->volume_short_today -= volume;

					position->volume_short = position->volume_short_today + position->volume_short_his;

					//更新实际上的空头持仓(今仓)
					position->pos_short_today -= volume;
				}
				else
				{
					//多头开仓市值
					position->open_cost_long = position->open_cost_long * (position->volume_long - volume) / position->volume_long;

					//多头持仓成本
					position->position_cost_long = position->position_cost_long * (position->volume_long - volume) / position->volume_long;

					//平仓盈亏
					close_profit = (price - position->position_price_long) * volume * position->ins->volume_multiple;

					//更新多头持仓手数
					position->volume_long_today -= volume;

					position->volume_long = position->volume_long_today + position->volume_long_his;

					//更新实际上的多头持仓(今仓)
					position->pos_long_today -= volume;
				}
			}
			//平昨
			else if (order->offset == kOffsetClose)
			{
				if (order->direction == kDirectionBuy)
				{
					//空头开仓市值
					position->open_cost_short = position->open_cost_short * (position->volume_short - volume) / position->volume_short;

					//空头持仓成本
					position->position_cost_short = position->position_cost_short * (position->volume_short - volume) / position->volume_short;

					//平仓盈亏
					close_profit = (position->position_price_short - price) * volume * position->ins->volume_multiple;

					//更新空头持仓手数(昨仓)
					position->volume_short_his -= volume;

					position->volume_short = position->volume_short_his + position->volume_short_today;

					//更新实际上的空头持仓(昨仓)
					position->pos_short_his -= volume;
				}
				else
				{
					//多头开仓市值
					position->open_cost_long = position->open_cost_long * (position->volume_long - volume) / position->volume_long;

					//多头持仓成本
					position->position_cost_long = position->position_cost_long * (position->volume_long - volume) / position->volume_long;

					//平仓盈亏
					close_profit = (price - position->position_price_long) * volume * position->ins->volume_multiple;

					//更新多头持仓手数(昨仓)
					position->volume_long_his -= volume;

					position->volume_long = position->volume_long_his + position->volume_long_today;

					//更新实际上的多头持仓(昨仓)
					position->pos_long_his -= volume;
				}
			}
		}
		else
		//其它交易所(不区分平今平昨)
		{			
			if (order->direction == kDirectionBuy)
			{
				//空头开仓市值
				position->open_cost_short = position->open_cost_short * (position->volume_short - volume) / position->volume_short;
				
				//空头持仓成本
				position->position_cost_short = position->position_cost_short * (position->volume_short - volume) / position->volume_short;

				//平仓盈亏
				close_profit = (position->position_price_short - price) * volume * position->ins->volume_multiple;

				//更新空头持仓手数
				position->volume_short_today -= volume;

				position->volume_short = position->volume_short_today + position->volume_short_his;

				//更新实际上的空头持仓(今仓),优先平今算法
				position->pos_short_today -= volume;
				if (position->pos_short_today < 0)
				{
					position->pos_short_his += position->pos_short_today;
					position->pos_short_today = 0;
				}
			}
			else
			{
				//多头开仓市值
				position->open_cost_long = position->open_cost_long * (position->volume_long - volume) / position->volume_long;

				//多头持仓成本
				position->position_cost_long = position->position_cost_long * (position->volume_long - volume) / position->volume_long;

				//平仓盈亏
				close_profit = (price - position->position_price_long) * volume * position->ins->volume_multiple;
				
				//更新多头持仓手数
				position->volume_long_today -= volume;

				position->volume_long = position->volume_long_today + position->volume_long_his;

				//更新实际上的多头持仓(今仓),优先平今算法
				position->pos_long_today -= volume;
				if (position->pos_long_today < 0)
				{
					position->pos_long_his += position->pos_long_today;
					position->pos_long_today = 0;
				}
			}			
		}
		//调整账户资金(平仓盈亏)
		m_account->close_profit += close_profit;
	}
	
	//调整账户资金(手续费)
	m_account->commission += commission;

	//更新持仓信息
	UpdatePositionVolume(position);

	nss.FromVar(*position);	
	nss.ToString(&msg);
	Log().WithField("fun","DoTrade")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithPack("pos_pack",msg)
		.Log(LOG_INFO,"position info after ajust");
}

void tradersim::OnClientPeekMessage()
{
	m_peeking_message = true;
	SendUserData();
}

#pragma endregion

#pragma region client_request

void tradersim::OnClientReqInsertOrder(ActionOrder action_insert_order)
{
	std::string symbol = action_insert_order.exchange_id + "." + action_insert_order.ins_id;
	if (action_insert_order.order_id.empty())
	{
		action_insert_order.order_id =
			std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	SerializerSim ss;
	ss.FromVar(action_insert_order);
	std::string strMsg;
	ss.ToString(&strMsg);

	std::string order_key = action_insert_order.order_id;
	auto it = m_data.m_orders.find(order_key);
	if (it != m_data.m_orders.end())
	{
		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)	
			.WithPack("diff_req_pack",strMsg)
			.Log(LOG_INFO,u8"下单, 已被服务器拒绝,原因:单号重复");

		OutputNotifyAllSycn(407,u8"下单, 已被服务器拒绝,原因:单号重复","WARNING");
		return;
	}

	m_something_changed = true;
	const Instrument* ins = GetInstrument(symbol);
	Order* order = &(m_data.m_orders[order_key]);
	order->user_id = action_insert_order.user_id;
	order->exchange_id = action_insert_order.exchange_id;
	order->instrument_id = action_insert_order.ins_id;
	order->order_id = action_insert_order.order_id;
	order->exchange_order_id = order->order_id;
	order->direction = action_insert_order.direction;
	order->offset = action_insert_order.offset;
	order->price_type = action_insert_order.price_type;
	order->limit_price = action_insert_order.limit_price;
	order->volume_orign = action_insert_order.volume;
	order->volume_left = action_insert_order.volume;
	order->status = kOrderStatusAlive;
	order->volume_condition = action_insert_order.volume_condition;
	order->time_condition = action_insert_order.time_condition;
	order->insert_date_time = GetLocalEpochNano();
	order->seqno = m_last_seq_no++;

	if (action_insert_order.user_id.substr(0, m_user_id.size()) != m_user_id)
	{
		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单, 已被服务器拒绝, 原因:下单指令中的用户名错误");

		OutputNotifyAllSycn(408,u8"下单, 已被服务器拒绝, 原因:下单指令中的用户名错误","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	if (nullptr==ins)
	{
		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单, 已被服务器拒绝, 原因:合约不合法");

		OutputNotifyAllSycn(409,u8"下单, 已被服务器拒绝, 原因:合约不合法","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	if (ins->product_class != kProductClassFutures)
	{
		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单, 已被服务器拒绝, 原因:模拟交易只支持期货合约");

		OutputNotifyAllSycn(410,u8"下单, 已被服务器拒绝, 原因:模拟交易只支持期货合约","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	if (action_insert_order.volume <= 0)
	{
		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单, 已被服务器拒绝, 原因:下单手数应该大于0");

		OutputNotifyAllSycn(411,u8"下单, 已被服务器拒绝, 原因:下单手数应该大于0","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	double xs = action_insert_order.limit_price / ins->price_tick;
	if (xs - int(xs + 0.5) >= 0.001)
	{
		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单, 已被服务器拒绝, 原因:下单价格不是价格单位的整倍数");

		OutputNotifyAllSycn(412,u8"下单,已被服务器拒绝, 原因:下单价格不是价格单位的整倍数","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}
	
	Position* position = &(m_data.m_positions[symbol]);
	position->ins = ins;
	position->instrument_id = order->instrument_id;
	position->exchange_id = order->exchange_id;
	position->user_id = m_user_id;

	//如果是开仓
	if (action_insert_order.offset == kOffsetOpen)
	{
		if (position->ins->margin * action_insert_order.volume > m_account->available)
		{
			Log().WithField("fun","OnClientReqInsertOrder")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithPack("diff_req_pack",strMsg)
				.Log(LOG_INFO, u8"下单,已被服务器拒绝,原因:开仓保证金不足");

			OutputNotifyAllSycn(413,u8"下单,已被服务器拒绝,原因:开仓保证金不足","WARNING");
			order->status = kOrderStatusFinished;
			return;
		}
	}
	//如果是平仓
	else
	{
		//上期和原油交易所(区分平今平昨)
		if ((position->exchange_id == "SHFE" || position->exchange_id == "INE"))
		{
			//平今
			if (order->offset == kOffsetCloseToday)
			{
				if (
					(action_insert_order.direction == kDirectionBuy
						&& position->volume_short_today < action_insert_order.volume + position->volume_short_frozen_today)
					|| (action_insert_order.direction == kDirectionSell
						&& position->volume_long_today < action_insert_order.volume + position->volume_long_frozen_today)
					)
				{
					Log().WithField("fun","OnClientReqInsertOrder")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithPack("diff_req_pack",strMsg)
						.Log(LOG_INFO,u8"下单,已被服务器拒绝,原因:平今手数超过今仓持仓量");

					OutputNotifyAllSycn(414,u8"下单,已被服务器拒绝,原因:平今手数超过今仓持仓量","WARNING");
					order->status = kOrderStatusFinished;
					return;
				}
			}
			//平昨
			else if (order->offset == kOffsetClose)
			{
				if (
					(action_insert_order.direction == kDirectionBuy
						&& position->volume_short_his < action_insert_order.volume + position->volume_short_frozen_his)
					|| (action_insert_order.direction == kDirectionSell
						&& position->volume_long_his < action_insert_order.volume + position->volume_long_frozen_his)
					)
				{
					Log().WithField("fun", "OnClientReqInsertOrder")
						.WithField("key", _key)
						.WithField("bid", _req_login.bid)
						.WithField("user_name", _req_login.user_name)
						.WithPack("diff_req_pack", strMsg)
						.Log(LOG_INFO,u8"下单,已被服务器拒绝,原因:平昨手数超过昨仓持仓量");

					OutputNotifyAllSycn(415,u8"下单,已被服务器拒绝,原因:平昨手数超过昨仓持仓量","WARNING");
					order->status = kOrderStatusFinished;
					return;
				}
			}
		}
		else
		//其它交易所(不区分平今平昨)
		{
			if (
				(action_insert_order.direction == kDirectionBuy
					&& position->volume_short < action_insert_order.volume + position->volume_short_frozen_today)
				|| (action_insert_order.direction == kDirectionSell
					&& position->volume_long < action_insert_order.volume + position->volume_long_frozen_today)
				)
			{
				Log().WithField("fun", "OnClientReqInsertOrder")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithPack("diff_req_pack", strMsg)
					.Log(LOG_INFO,u8"下单,已被服务器拒绝,原因:平仓手数超过持仓量");

				OutputNotifyAllSycn(416,u8"下单,已被服务器拒绝,原因:平仓手数超过持仓量","WARNING");
				order->status = kOrderStatusFinished;
				return;
			}
		}		
	}

	m_alive_order_set.insert(order);

	UpdateOrder(order);
	
	Log().WithField("fun","OnClientReqInsertOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithPack("diff_req_pack",strMsg)
		.Log(LOG_INFO, u8"下单成功");

	OutputNotifyAllSycn(417,u8"下单成功");

	//尝试匹配成交
	TryOrderMatch();
	
	//重算所有持仓项的持仓盈亏和浮动盈亏
	RecaculatePositionAndFloatProfit();

	//保存用户文件
	SaveUserDataFile();

	SendUserData();
}

void tradersim::OnClientReqCancelOrder(ActionOrder action_cancel_order)
{
	if (action_cancel_order.user_id.substr(0, m_user_id.size()) != m_user_id)
	{
		Log().WithField("fun","OnClientReqCancelOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("order_id", action_cancel_order.order_id)			
			.Log(LOG_INFO, u8"撤单,已被服务器拒绝,原因:撤单指令中的用户名错误");

		OutputNotifyAllSycn(418,u8"撤单,已被服务器拒绝,原因:撤单指令中的用户名错误","WARNING");
		return;
	}

	for (auto it_order = m_alive_order_set.begin()
		; it_order != m_alive_order_set.end()
		; ++it_order)
	{
		Order* order = *it_order;
		if (order->order_id == action_cancel_order.order_id
			&& order->status == kOrderStatusAlive)
		{
			order->status = kOrderStatusFinished;
			UpdateOrder(order);
			m_something_changed = true;

			Log().WithField("fun", "OnClientReqCancelOrder")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", action_cancel_order.order_id)
				.Log(LOG_INFO, u8"撤单成功");

			OutputNotifyAllSycn(419,u8"撤单成功");
			SendUserData();
			return;
		}
	}

	Log().WithField("fun","OnClientReqCancelOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("order_id", action_cancel_order.order_id)
		.Log(LOG_INFO,u8"要撤销的单不存在");

	OutputNotifyAllSycn(420,u8"要撤销的单不存在","WARNING");	
}

void tradersim::OnClientReqTransfer(ActionTransfer action_transfer)
{
	m_transfer_seq++;
	TransferLog& d = GetTransferLog(std::to_string(m_transfer_seq));
	d.currency = action_transfer.currency;
	d.amount = action_transfer.amount;
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	DateTime dt;
	dt.date.year = now.date().year();
	dt.date.month = now.date().month();
	dt.date.day = now.date().day();
	dt.time.hour = now.time_of_day().hours();
	dt.time.minute = now.time_of_day().minutes();
	dt.time.second = now.time_of_day().seconds();
	dt.time.microsecond = 0;
	d.datetime = DateTimeToEpochNano(&dt);
	
	if (action_transfer.amount > 0)
	{
		m_account->deposit += action_transfer.amount;
	}
	else
	{
		if (m_account->available < -action_transfer.amount)
		{
			Log().WithField("fun", "OnClientReqTransfer")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("amount", action_transfer.amount)
				.WithField("available", m_account->available)
				.WithField("currency", action_transfer.currency)
				.Log(LOG_INFO,u8"转账失败");
			OutputNotifyAllSycn(422,u8"转账失败");
			d.error_id = 1;
			d.error_msg = u8"可转出资金不足";
			return;
		}
		m_account->withdraw -= action_transfer.amount;
	}
	
	d.error_id = 0;
	d.error_msg = u8"正确";

	m_account->available+= action_transfer.amount;
	m_account->static_balance += action_transfer.amount;
	m_account->balance+= action_transfer.amount;
	//计算风险度
	if (IsValid(m_account->margin) && IsValid(m_account->balance) && !IsZero(m_account->balance))
		m_account->risk_ratio = m_account->margin / m_account->balance;
	else
		m_account->risk_ratio = NAN;
	m_account->changed = true;
		
	Log().WithField("fun","OnClientReqTransfer")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("amount", action_transfer.amount)
		.WithField("currency", action_transfer.currency)		
		.Log(LOG_INFO, u8"转账成功");
	OutputNotifyAllSycn(421,u8"转账成功");

	m_something_changed = true;		
	SendUserData();
}

#pragma endregion

#pragma region ConditionOrderCallBack

void tradersim::CheckTimeConditionOrder()
{
	std::set<std::string>& os = m_condition_order_manager.GetTimeCoSet();
	if (os.empty())
	{
		return;
	}
	m_condition_order_manager.OnCheckTime();
}

void tradersim::SetExchangeTime()
{
	boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();

	DateTime dtLocalTime;
	dtLocalTime.date.year = tm.date().year();
	dtLocalTime.date.month = tm.date().month();
	dtLocalTime.date.day = tm.date().day();
	dtLocalTime.time.hour = tm.time_of_day().hours();
	dtLocalTime.time.minute = tm.time_of_day().minutes();
	dtLocalTime.time.second = tm.time_of_day().seconds();
	dtLocalTime.time.microsecond = 0;
	m_condition_order_manager.SetExchangeTime(DateTimeToEpochSeconds(dtLocalTime),
		DateTimeToEpochSeconds(dtLocalTime),
		DateTimeToEpochSeconds(dtLocalTime),
		DateTimeToEpochSeconds(dtLocalTime),
		DateTimeToEpochSeconds(dtLocalTime),
		DateTimeToEpochSeconds(dtLocalTime));
}

void tradersim::CheckPriceConditionOrder()
{
	TInstOrderIdListMap& om = m_condition_order_manager.GetPriceCoMap();
	if (om.empty())
	{
		return;
	}
	
	m_condition_order_manager.OnCheckPrice();
}

void tradersim::OnUserDataChange()
{
	Log().WithField("fun","OnUserDataChange")
		.Log(LOG_INFO,"tradersim OnUserDataChange");
	SaveUserDataFile();
	SendUserData();
}

void tradersim::OutputNotifyAll(long notify_code, const std::string& ret_msg
	, const char* level, const char* type)
{
	OutputNotifyAllSycn(notify_code, ret_msg, level, type);
}

void tradersim::OnTouchConditionOrder(const ConditionOrder& order)
{
	SerializerConditionOrderData ss;
	ss.FromVar(order);
	std::string strMsg;
	ss.ToString(&strMsg);
	
	Log().WithField("fun","OnTouchConditionOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("order_id", order.order_id)
		.WithPack("co_pack",strMsg)
		.Log(LOG_INFO,"condition order is touched");

	int nOrderIndex = 0;
	for (const ContingentOrder& co : order.order_list)
	{
		nOrderIndex++;
		std::string symbol = co.exchange_id + "." + co.instrument_id;
		Instrument* ins = GetInstrument(symbol);
		if (nullptr == ins)
		{
			Log().WithField("fun","OnTouchConditionOrder")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id",co.exchange_id)
				.WithField("instrument_id",co.instrument_id)
				.Log(LOG_WARNING,"symbol not exist");		
			continue;
		}

		bool flag = false;
		//如果是开仓
		if (EOrderOffset::open == co.offset)
		{
			flag = ConditionOrder_Open(order,co,*ins,nOrderIndex);
		}		
		else if (EOrderOffset::close == co.offset)
		{
			bool b_has_td_yd_distinct = (co.exchange_id == "SHFE") || (co.exchange_id == "INE");
			if (b_has_td_yd_distinct)
			{
				//如果是平掉指定数量
				if (EVolumeType::num == co.volume_type)
				{
					//如果是平今优先
					if (co.close_today_prior)
					{
						if (order.is_cancel_ori_close_order)
						{
							flag = ConditionOrder_CloseTodayPrior_NeedCancel(order, co, *ins,nOrderIndex);
						}
						else
						{
							flag = ConditionOrder_CloseTodayPrior_NotNeedCancel(order,co,*ins,nOrderIndex);
						}
					}
					//如果是平昨优先
					else
					{
						if (order.is_cancel_ori_close_order)
						{
							flag = ConditionOrder_CloseYesTodayPrior_NeedCancel(order, co, *ins,nOrderIndex);
						}
						else
						{
							flag = ConditionOrder_CloseYesTodayPrior_NotNeedCancel(order, co, *ins,nOrderIndex);
						}
					}
				}
				//如果是全平
				else if (EVolumeType::close_all == co.volume_type)
				{
					flag = ConditionOrder_CloseAll(order, co, *ins,nOrderIndex);
				}				
			}
			else
			{
				//不分今昨的平仓				
				flag = ConditionOrder_Close(order,co,*ins,nOrderIndex);
			}			
		}
		else if (EOrderOffset::reverse == co.offset)
		{
			//对空头进行反手操作
			if (co.direction == EOrderDirection::buy)
			{
				flag = ConditionOrder_Reverse_Short(order, co, *ins,nOrderIndex);
			}
			//对多头进行反手操作
			else if (co.direction == EOrderDirection::sell)
			{
				flag = ConditionOrder_Reverse_Long(order, co, *ins,nOrderIndex);
			}
		}		
	}	
}

bool tradersim::ConditionOrder_Open(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	ActionOrder action_insert_order;
	action_insert_order.aid = "insert_order";
	action_insert_order.user_id = _req_login.user_name;
	action_insert_order.exchange_id = co.exchange_id;
	action_insert_order.ins_id = co.instrument_id;

	//开仓
	action_insert_order.offset = kOffsetOpen;

	//开多
	if (EOrderDirection::buy == co.direction)
	{
		action_insert_order.direction = kDirectionBuy;
	}
	//开空
	else
	{
		action_insert_order.direction = kDirectionSell;
	}

	//数量类型
	if (EVolumeType::num == co.volume_type)
	{
		action_insert_order.volume= co.volume;
		action_insert_order.volume_condition = kOrderVolumeConditionAny;
	}
	else
	{
		//开仓时必须指定具体手数
		Log().WithField("fun","ConditionOrder_Open")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("order_id", order.order_id)
			.WithField("instrument_id",co.instrument_id)
			.Log(LOG_WARNING,"has bad volume_type");
		return false;
	}

	//价格类型
	//限价
	if (EPriceType::limit == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.limit_price= co.limit_price;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
	}
	//触发价
	else if (EPriceType::contingent == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		double last_price = ins.last_price;
		if (kProductClassCombination == ins.product_class)
		{
			if (EOrderDirection::buy == co.direction)
			{
				last_price = ins.ask_price1;
			}
			else
			{
				last_price = ins.bid_price1;
			}
		}
		action_insert_order.limit_price = last_price;
	}
	//对价
	else if (EPriceType::consideration == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		//开多
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.ask_price1;
		}
		//开空
		else
		{
			action_insert_order.limit_price = ins.bid_price1;
		}
	}
	//市价
	else if (EPriceType::market == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionIOC;		
		//开多
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.upper_limit;
		}
		//开空
		else
		{
			action_insert_order.limit_price = ins.lower_limit;
		}
	}
	//超价
	else if (EPriceType::over == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		//开多
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.ask_price1 + ins.price_tick;
			if (action_insert_order.limit_price > ins.upper_limit)
			{
				action_insert_order.limit_price = ins.upper_limit;
			}
		}
		//开空
		else
		{
			action_insert_order.limit_price = ins.bid_price1 - ins.price_tick;
			if (action_insert_order.limit_price < ins.lower_limit)
			{
				action_insert_order.limit_price = ins.lower_limit;
			}
		}
	}

	action_insert_order.hedge_flag = kHedgeFlagSpeculation;	
	action_insert_order.order_id= order.order_id + "_open_" + std::to_string(nOrderIndex);
	
	OnConditionOrderReqInsertOrder(action_insert_order);

	return true;
}

bool tradersim::ConditionOrder_CloseTodayPrior_NeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	Position& pos = GetPosition(co.exchange_id,co.instrument_id,_req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的今仓手数
		if (co.volume <= pos.pos_short_today - pos.volume_short_frozen_today)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);

				if (action_insert_order.volume>0)
				{
					OnConditionOrderReqInsertOrder(action_insert_order);
				}				

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)	
					.WithField("order_id", order.order_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");
				return false;
			}
		}
		//要平的手数小于等于今仓手数(包括冻结的手数)
		else if (co.volume <= pos.pos_short_today)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				//先撤掉所有平今仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionBuy)
						&& (order.offset == kOffsetCloseToday))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}

				}

				//再平今仓
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);	

				if (action_insert_order.volume > 0)
				{
					OnConditionOrderReqInsertOrder(action_insert_order);
				}						

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}			
		}
		//要平的手数小于等于所有空仓(不包手冻结的昨仓)
		else if (co.volume <= pos.pos_short_today + pos.pos_short_his - pos.volume_short_frozen_his)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//买
			action_insert_order_1.direction = kDirectionBuy;
			//平今
			action_insert_order_1.offset = kOffsetCloseToday;
			//数量
			action_insert_order_1.volume = pos.pos_short_today;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//买
			action_insert_order_2.direction = kDirectionBuy;
			//平昨
			action_insert_order_2.offset = kOffsetClose;
			//数量
			action_insert_order_2.volume = co.volume- pos.pos_short_today;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;
			
			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平今仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionBuy)
						&& (order.offset == kOffsetCloseToday))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//先平今仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//再平昨仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有空仓(包括冻结的手数)
		else if (co.volume <= pos.pos_short_today + pos.pos_short_his)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//买
			action_insert_order_1.direction = kDirectionBuy;
			//平今
			action_insert_order_1.offset = kOffsetCloseToday;
			//数量
			action_insert_order_1.volume = pos.pos_short_today;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//买
			action_insert_order_2.direction = kDirectionBuy;
			//平昨
			action_insert_order_2.offset = kOffsetClose;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_short_today;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionBuy)
						&& ((order.offset == kOffsetCloseToday)||(order.offset == kOffsetClose)))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//先平今仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//再平昨仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的今仓手数
		if (co.volume <= pos.pos_long_today - pos.volume_long_frozen_today)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);

				if (action_insert_order.volume > 0)
				{
					OnConditionOrderReqInsertOrder(action_insert_order);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于今仓手数(包括冻结的手数)
		else if (co.volume <= pos.pos_long_today)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				//先撤掉所有平今仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionSell)
						&& (order.offset == kOffsetCloseToday))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}

				}

				//平今仓
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有多仓(不包手冻结的昨仓)
		else if (co.volume <= pos.pos_long_today + pos.pos_long_his - pos.volume_long_frozen_his)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//卖
			action_insert_order_1.direction = kDirectionSell;
			//平今
			action_insert_order_1.offset = kOffsetCloseToday;
			//数量
			action_insert_order_1.volume = pos.pos_long_today;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//卖
			action_insert_order_2.direction = kDirectionSell;
			//平昨
			action_insert_order_2.offset = kOffsetClose;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_long_today;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平今仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionSell)
						&& (order.offset == kOffsetCloseToday))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//平今仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//平昨仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有多仓(包括冻结的手数)
		else if (co.volume <= pos.pos_long_today + pos.pos_long_his)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//卖
			action_insert_order_1.direction = kDirectionSell;
			//平今
			action_insert_order_1.offset = kOffsetCloseToday;
			//数量
			action_insert_order_1.volume = pos.pos_long_today;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//卖
			action_insert_order_2.direction = kDirectionSell;
			//平昨
			action_insert_order_2.offset = kOffsetClose;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_long_today;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionSell)
						&& ((order.offset == kOffsetCloseToday) || (order.offset == kOffsetClose)))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//平今仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//平昨仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}		
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool tradersim::ConditionOrder_CloseTodayPrior_NotNeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的今仓手数
		if (co.volume <= pos.pos_short_today - pos.volume_short_frozen_today)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//如果可平的手数小于所有可平的今昨仓手数
		else if (co.volume <= pos.pos_short_today - pos.volume_short_frozen_today + pos.pos_short_his - pos.volume_short_frozen_his)
		{
			ActionOrder action_insert_order1;
			action_insert_order1.aid = "insert_order";
			action_insert_order1.user_id = _req_login.user_name;
			action_insert_order1.exchange_id = co.exchange_id;
			action_insert_order1.ins_id = co.instrument_id;
			//买
			action_insert_order1.direction = kDirectionBuy;
			//平今
			action_insert_order1.offset = kOffsetCloseToday;
			//数量
			action_insert_order1.volume = pos.pos_short_today - pos.volume_short_frozen_today;
			action_insert_order1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order2;
			action_insert_order2.aid = "insert_order";
			action_insert_order2.user_id = _req_login.user_name;
			action_insert_order2.exchange_id = co.exchange_id;
			action_insert_order2.ins_id = co.instrument_id;
			//买
			action_insert_order2.direction = kDirectionBuy;
			//平昨
			action_insert_order2.offset = kOffsetClose;
			//数量
			action_insert_order2.volume = co.volume - action_insert_order1.volume;
			action_insert_order2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order2, order, co, ins))
			{
				//平今
				if (action_insert_order1.volume > 0)
				{
					action_insert_order1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order1.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order1);
				}
				
				//平昨
				if (action_insert_order2.volume > 0)
				{
					action_insert_order2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order2.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order2);
				}
				
				return true;
			}
			//价格设置不合法
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的今仓手数
		if (co.volume <= pos.pos_long_today - pos.volume_long_frozen_today)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}

		}
		//要平的手数小于等于可平的今昨仓手数
		else if (co.volume <= pos.pos_long_today - pos.volume_long_frozen_today + pos.pos_long_his - pos.volume_long_frozen_his)
		{
			ActionOrder action_insert_order1;
			action_insert_order1.aid = "insert_order";
			action_insert_order1.user_id = _req_login.user_name;
			action_insert_order1.exchange_id = co.exchange_id;
			action_insert_order1.ins_id = co.instrument_id;
			//卖
			action_insert_order1.direction = kDirectionSell;
			//平今
			action_insert_order1.offset = kOffsetCloseToday;
			//数量
			action_insert_order1.volume = pos.pos_long_today - pos.volume_long_frozen_today;
			action_insert_order1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order2;
			action_insert_order2.aid = "insert_order";
			action_insert_order2.user_id = _req_login.user_name;
			action_insert_order2.exchange_id = co.exchange_id;
			action_insert_order2.ins_id = co.instrument_id;
			//卖
			action_insert_order2.direction = kDirectionSell;
			//平昨
			action_insert_order2.offset = kOffsetClose;
			//数量
			action_insert_order2.volume = co.volume - action_insert_order1.volume;
			action_insert_order2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order2, order, co, ins))
			{
				//平今
				if (action_insert_order1.volume > 0)
				{
					action_insert_order1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order1.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order1);
				}
				
				//平昨
				if (action_insert_order2.volume > 0)
				{
					action_insert_order2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order2.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order2);
				}
				
				return true;
			}
			//价格设置不合法
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool tradersim::ConditionOrder_CloseYesTodayPrior_NeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的昨仓手数
		if (co.volume <= pos.pos_short_his - pos.volume_short_frozen_his)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于昨仓手数(包括冻结的手数)
		else if (co.volume <= pos.pos_short_his)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				//先撤掉所有平昨仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionBuy)
						&& (order.offset == kOffsetClose))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}

				}

				//再平昨仓
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有空仓(不包手冻结的昨仓)
		else if (co.volume <= pos.pos_short_his + pos.pos_short_today - pos.volume_short_frozen_today)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//买
			action_insert_order_1.direction = kDirectionBuy;
			//平昨
			action_insert_order_1.offset = kOffsetClose;
			//数量
			action_insert_order_1.volume = pos.pos_short_his;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//买
			action_insert_order_2.direction = kDirectionBuy;
			//平今
			action_insert_order_2.offset = kOffsetCloseToday;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_short_his;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平昨仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionBuy)
						&& (order.offset == kOffsetClose))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//平昨仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//平今仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有空仓(包括冻结的手数)
		else if (co.volume <= pos.pos_short_his + pos.pos_short_today)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//买
			action_insert_order_1.direction = kDirectionBuy;
			//平昨
			action_insert_order_1.offset = kOffsetClose;
			//数量
			action_insert_order_1.volume = pos.pos_short_his;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//买
			action_insert_order_2.direction = kDirectionBuy;
			//平今
			action_insert_order_2.offset = kOffsetCloseToday;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_short_his;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionBuy)
						&& ((order.offset == kOffsetCloseToday)||(order.offset == kOffsetClose)))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//平昨仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//平今仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的昨仓手数
		if (co.volume <= pos.pos_long_his - pos.volume_long_frozen_his)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}				

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于昨仓手数(包括冻结的手数)
		else if (co.volume <= pos.pos_long_his)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				//先撤掉所有平昨仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionSell)
						&& (order.offset == kOffsetClose))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}

				}

				//平昨仓
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}				

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有多仓(不包手冻结的昨仓)
		else if (co.volume <= pos.pos_long_his + pos.pos_long_today - pos.volume_long_frozen_today)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//卖
			action_insert_order_1.direction = kDirectionSell;
			//平昨
			action_insert_order_1.offset = kOffsetClose;
			//数量
			action_insert_order_1.volume = pos.pos_long_his;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//卖
			action_insert_order_2.direction = kDirectionSell;
			//平今
			action_insert_order_2.offset = kOffsetCloseToday;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_long_his;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平昨仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionSell)
						&& (order.offset == kOffsetClose))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//再平昨仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//再平今仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//要平的手数小于等于所有多仓(包括冻结的手数)
		else if (co.volume <= pos.pos_long_his + pos.pos_long_today)
		{
			ActionOrder action_insert_order_1;
			action_insert_order_1.aid = "insert_order";
			action_insert_order_1.user_id = _req_login.user_name;
			action_insert_order_1.exchange_id = co.exchange_id;
			action_insert_order_1.ins_id = co.instrument_id;
			//卖
			action_insert_order_1.direction = kDirectionSell;
			//平昨
			action_insert_order_1.offset = kOffsetClose;
			//数量
			action_insert_order_1.volume = pos.pos_long_his;
			action_insert_order_1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order_2;
			action_insert_order_2.aid = "insert_order";
			action_insert_order_2.user_id = _req_login.user_name;
			action_insert_order_2.exchange_id = co.exchange_id;
			action_insert_order_2.ins_id = co.instrument_id;
			//卖
			action_insert_order_2.direction = kDirectionSell;
			//平今
			action_insert_order_2.offset = kOffsetCloseToday;
			//数量
			action_insert_order_2.volume = co.volume - pos.pos_long_his;
			action_insert_order_2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order_1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order_2, order, co, ins))
			{
				//先撤掉所有平仓的单子
				for (auto it : m_data.m_orders)
				{
					const std::string& orderId = it.first;
					const Order& order = it.second;
					if (order.status == kOrderStatusFinished)
					{
						continue;
					}
					if (order.symbol() != symbol)
					{
						continue;
					}

					if ((order.direction == kDirectionSell)
						&& ((order.offset == kOffsetCloseToday) || (order.offset == kOffsetClose)))
					{
						ActionOrder action_cancel_order;
						action_cancel_order.aid = "cancel_order";
						action_cancel_order.order_id = orderId;
						action_cancel_order.user_id = _req_login.user_name.c_str();
						OnConditionOrderReqCancelOrder(action_cancel_order);
					}
				}

				//再平昨仓
				if (action_insert_order_1.volume > 0)
				{
					action_insert_order_1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_1.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_1);
				}
				
				//再平今仓
				if (action_insert_order_2.volume > 0)
				{
					action_insert_order_2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order_2.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order_2);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}		
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool tradersim::ConditionOrder_CloseYesTodayPrior_NotNeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的昨仓手数
		if (co.volume <= pos.pos_short_his - pos.volume_short_frozen_his)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}		

				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//如果可平的手数小于所有可平的今昨仓手数
		else if (co.volume <= pos.pos_short_his - pos.volume_short_frozen_his + pos.pos_short_today - pos.volume_short_frozen_today)
		{
			ActionOrder action_insert_order1;
			action_insert_order1.aid = "insert_order";
			action_insert_order1.user_id = _req_login.user_name;
			action_insert_order1.exchange_id = co.exchange_id;
			action_insert_order1.ins_id = co.instrument_id;
			//买
			action_insert_order1.direction = kDirectionBuy;
			//平昨
			action_insert_order1.offset = kOffsetClose;
			//数量
			action_insert_order1.volume = pos.pos_short_his - pos.volume_short_frozen_his;
			action_insert_order1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order2;
			action_insert_order2.aid = "insert_order";
			action_insert_order2.user_id = _req_login.user_name;
			action_insert_order2.exchange_id = co.exchange_id;
			action_insert_order2.ins_id = co.instrument_id;
			//买
			action_insert_order2.direction = kDirectionBuy;
			//平今
			action_insert_order2.offset = kOffsetCloseToday;
			//数量
			action_insert_order2.volume = co.volume - action_insert_order1.volume;
			action_insert_order2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order2, order, co, ins))
			{
				//平昨
				if (action_insert_order1.volume > 0)
				{
					action_insert_order1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order1.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order1);
				}
				
				//平今
				if (action_insert_order2.volume > 0)
				{
					action_insert_order2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order2.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order2);
				}
				
				return true;
			}
			//价格设置不合法
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//可平不足
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NotNeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的昨仓手数
		if (co.volume <= pos.pos_long_his - pos.volume_long_frozen_his)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = co.volume;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				if (action_insert_order.volume > 0)
				{
					action_insert_order.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order);
				}
				
				return true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		else if (co.volume <= pos.pos_long_his - pos.volume_long_frozen_his + pos.pos_long_today - pos.volume_long_frozen_today)
		{
			ActionOrder action_insert_order1;
			action_insert_order1.aid = "insert_order";
			action_insert_order1.user_id = _req_login.user_name;
			action_insert_order1.exchange_id = co.exchange_id;
			action_insert_order1.ins_id = co.instrument_id;
			//卖
			action_insert_order1.direction = kDirectionSell;
			//平昨
			action_insert_order1.offset = kOffsetClose;
			//数量
			action_insert_order1.volume = pos.pos_long_his - pos.volume_long_frozen_his;
			action_insert_order1.volume_condition = kOrderVolumeConditionAny;

			ActionOrder action_insert_order2;
			action_insert_order2.aid = "insert_order";
			action_insert_order2.user_id = _req_login.user_name;
			action_insert_order2.exchange_id = co.exchange_id;
			action_insert_order2.ins_id = co.instrument_id;
			//卖
			action_insert_order2.direction = kDirectionSell;
			//平今
			action_insert_order2.offset = kOffsetCloseToday;
			//数量
			action_insert_order2.volume = co.volume - action_insert_order1.volume;
			action_insert_order2.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order1, order, co, ins)
				&& SetConditionOrderPrice(action_insert_order2, order, co, ins))
			{
				//平昨
				if (action_insert_order1.volume > 0)
				{
					action_insert_order1.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order1.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order1);
				}
				
				//平今
				if (action_insert_order2.volume > 0)
				{
					action_insert_order2.hedge_flag = kHedgeFlagSpeculation;
					action_insert_order2.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					OnConditionOrderReqInsertOrder(action_insert_order2);
				}
				
				return true;
			}
			//价格设置不合法
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseYesTodayPrior_NotNeedCancel")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}
		//可平不足
		else
		{
			Log().WithField("fun","ConditionOrder_CloseYesTodayPrior_NotNeedCancel")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("order_id", order.order_id)
				.WithField("exchange_id",co.exchange_id)
				.WithField("instrument_id",co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool tradersim::ConditionOrder_CloseAll(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//先撤掉所有平昨仓的单子
		for (auto it : m_data.m_orders)
		{
			const std::string& orderId = it.first;
			const Order& order = it.second;
			if (order.status == kOrderStatusFinished)
			{
				continue;
			}
			if (order.symbol() != symbol)
			{
				continue;
			}

			if ((order.direction == kDirectionBuy)
				&& (order.offset == kOffsetClose))
			{
				ActionOrder action_cancel_order;
				action_cancel_order.aid = "cancel_order";
				action_cancel_order.order_id = orderId;
				action_cancel_order.user_id = _req_login.user_name.c_str();
				OnConditionOrderReqCancelOrder(action_cancel_order);
			}
		}

		bool flag = false;
		//如果今仓手数大于零
		if (pos.pos_short_today> 0)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = pos.pos_short_today;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
				OnConditionOrderReqInsertOrder(action_insert_order);
				flag = true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseAll")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}

		//如果昨仓手数大于零
		if (pos.pos_short_his> 0)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//买
			action_insert_order.direction = kDirectionBuy;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = pos.pos_short_his;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
				OnConditionOrderReqInsertOrder(action_insert_order);
				flag = true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseAll")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}

		return flag;
	}
	//卖平
	else
	{
			
		//先撤掉所有平仓的单子
		for (auto it : m_data.m_orders)
		{
			const std::string& orderId = it.first;
			const Order& order = it.second;
			if (order.status == kOrderStatusFinished)
			{
				continue;
			}
			if (order.symbol() != symbol)
			{
				continue;
			}

			if ((order.direction == kDirectionSell)
				&& ((order.offset == kOffsetCloseToday) || (order.offset == kOffsetClose)))
			{
				ActionOrder action_cancel_order;
				action_cancel_order.aid = "cancel_order";
				action_cancel_order.order_id = orderId;
				action_cancel_order.user_id = _req_login.user_name.c_str();
				OnConditionOrderReqCancelOrder(action_cancel_order);
			}
		}

		bool flag = false;
		//如果今仓手数大于零
		if (pos.pos_long_today > 0)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平今
			action_insert_order.offset = kOffsetCloseToday;
			//数量
			action_insert_order.volume = pos.pos_long_today;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
				OnConditionOrderReqInsertOrder(action_insert_order);

				flag = true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseAll")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}

		//如果昨仓手数大于零
		if (pos.pos_long_his > 0)
		{
			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;
			//卖
			action_insert_order.direction = kDirectionSell;
			//平昨
			action_insert_order.offset = kOffsetClose;
			//数量
			action_insert_order.volume = pos.pos_long_his;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			if (SetConditionOrderPrice(action_insert_order, order, co, ins))
			{
				action_insert_order.hedge_flag = kHedgeFlagSpeculation;
				action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
				OnConditionOrderReqInsertOrder(action_insert_order);

				flag = true;
			}
			//价格设置错误
			else
			{
				Log().WithField("fun", "ConditionOrder_CloseAll")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_INFO, "price is wrong!");

				return false;
			}
		}

		return flag;

	}
}

bool tradersim::SetConditionOrderPrice(ActionOrder& action_insert_order
	, const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins)
{
	//限价
	if (EPriceType::limit == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.limit_price = co.limit_price;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		return true;
	}
	//触发价
	else if (EPriceType::contingent == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;

		double last_price = ins.last_price;
		if (kProductClassCombination == ins.product_class)
		{
			if (EOrderDirection::buy == co.direction)
			{
				last_price = ins.ask_price1;
			}
			else
			{
				last_price = ins.bid_price1;
			}
		}
		action_insert_order.limit_price = last_price;

		return true;
	}
	//对价
	else if (EPriceType::consideration == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;

		//买平
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.ask_price1;
		}
		//卖平
		else
		{
			action_insert_order.limit_price = ins.bid_price1;
		}
		return true;
	}
	//市价
	else if (EPriceType::market == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionIOC;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.upper_limit;
		}
		//卖平
		else
		{
			action_insert_order.limit_price = ins.lower_limit;
		}
		return true;
	}
	//超价
	else if (EPriceType::over == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.ask_price1 + ins.price_tick;
			if (action_insert_order.limit_price > ins.upper_limit)
			{
				action_insert_order.limit_price = ins.upper_limit;
			}
		}
		//卖平
		else
		{
			action_insert_order.limit_price = ins.bid_price1 - ins.price_tick;
			if (action_insert_order.limit_price < ins.lower_limit)
			{
				action_insert_order.limit_price = ins.lower_limit;
			}
		}
		return true;
	}
	return false;
}

bool tradersim::ConditionOrder_Close(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins	
	, int nOrderIndex)
{
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	ActionOrder action_insert_order;
	action_insert_order.aid = "insert_order";
	action_insert_order.user_id = _req_login.user_name;
	action_insert_order.exchange_id = co.exchange_id;
	action_insert_order.ins_id = co.instrument_id;

	//平仓
	action_insert_order.offset = kOffsetClose;
	ENeedCancelOrderType needCancelOrderType = ENeedCancelOrderType::not_need;
	
	//买平
	if (EOrderDirection::buy == co.direction)
	{
		action_insert_order.direction = kDirectionBuy;		

		//数量类型
		if (EVolumeType::num == co.volume_type)
		{
			//要平的手数小于等于可平手数
			if (co.volume <= pos.pos_short_his+pos.pos_short_today-pos.volume_short_frozen)
			{
				action_insert_order.volume = co.volume;
				action_insert_order.volume_condition = kOrderVolumeConditionAny;
			}
			//要平的手数小于等于可平手数(包括冻结的手数)
			else if (co.volume <= pos.pos_short_his + pos.pos_short_today)
			{
				if (order.is_cancel_ori_close_order)
				{
					action_insert_order.volume = co.volume;
					action_insert_order.volume_condition = kOrderVolumeConditionAny;
					needCancelOrderType = ENeedCancelOrderType::all_buy;
				}
				else
				{
					Log().WithField("fun","ConditionOrder_Close")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithField("order_id", order.order_id)
						.WithField("exchange_id", co.exchange_id)
						.WithField("instrument_id",co.instrument_id)
						.Log(LOG_WARNING,"can close short is less than will close short");					
					return false;
				}
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name",_req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id",co.instrument_id)
					.Log(LOG_WARNING,"can close short is less than will close short");				
				return false;
			}
		}
		else if (EVolumeType::close_all == co.volume_type)
		{
			//先要撤掉所有平仓挂单
			needCancelOrderType = ENeedCancelOrderType::all_buy;
						
			//如果可平手数大于零
			if (pos.pos_short_his + pos.pos_short_today  > 0)
			{
				action_insert_order.volume = pos.pos_short_his + pos.pos_short_today;
				action_insert_order.volume_condition = kOrderVolumeConditionAny;
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name",_req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id",co.instrument_id)
					.Log(LOG_WARNING,"have no need close short because of short position is zero");			
				return false;
			}
		}
	}
	//卖平
	else
	{
		action_insert_order.direction = kDirectionSell;
		
		//数量类型
		if (EVolumeType::num == co.volume_type)
		{
			//要平的手数小于等于可平的手数
			if (co.volume <= pos.pos_long_his+ pos.pos_long_today - pos.volume_long_frozen)
			{
				action_insert_order.volume = co.volume;
				action_insert_order.volume_condition = kOrderVolumeConditionAny;
			}
			//要平的手数小于等于可平手数(包括冻结的手数)
			else if (co.volume <= pos.pos_long_his + pos.pos_long_today)
			{
				if (order.is_cancel_ori_close_order)
				{
					action_insert_order.volume = co.volume;
					action_insert_order.volume_condition = kOrderVolumeConditionAny;
					needCancelOrderType = ENeedCancelOrderType::all_sell;
				}
				else
				{
					Log().WithField("fun","ConditionOrder_Close")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithField("order_id", order.order_id)
						.WithField("exchange_id", co.exchange_id)
						.WithField("instrument_id",co.instrument_id)
						.Log(LOG_WARNING,"can close long is less than will close long");				
					return false;
				}
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name",_req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id",co.instrument_id)
					.Log(LOG_WARNING, "can close long is less than will close long");

				return false;
			}
		}
		else if (EVolumeType::close_all == co.volume_type)
		{
			needCancelOrderType = ENeedCancelOrderType::all_sell;

			//如果可平手数大于零
			if (pos.pos_long_his + pos.pos_long_today> 0)
			{
				action_insert_order.volume = pos.pos_long_his + pos.pos_long_today;
				action_insert_order.volume_condition = kOrderVolumeConditionAny;
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name",_req_login.user_name)
					.WithField("order_id", order.order_id)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id",co.instrument_id)
					.Log(LOG_WARNING,"have no need close long because of long position is zero");				
				return false;
			}
		}
	}

	//价格类型

	//限价
	if (EPriceType::limit == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.limit_price = co.limit_price;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
	}
	//触发价
	else if (EPriceType::contingent == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		double last_price = ins.last_price;
		if (kProductClassCombination == ins.product_class)
		{
			if (EOrderDirection::buy == co.direction)
			{
				last_price = ins.ask_price1;
			}
			else
			{
				last_price = ins.bid_price1;
			}
		}
		action_insert_order.limit_price = last_price;
	}
	//对价
	else if (EPriceType::consideration == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.ask_price1;
		}
		//卖平
		else
		{
			action_insert_order.limit_price = ins.bid_price1;
		}
	}
	//市价
	else if (EPriceType::market == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionIOC;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.upper_limit;
		}
		//卖平
		else
		{
			action_insert_order.limit_price = ins.lower_limit;
		}
	}
	//超价
	else if (EPriceType::over == co.price_type)
	{
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionGFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			action_insert_order.limit_price = ins.ask_price1 + ins.price_tick;
			if (action_insert_order.limit_price > ins.upper_limit)
			{
				action_insert_order.limit_price = ins.upper_limit;
			}
		}
		//卖平
		else
		{
			action_insert_order.limit_price = ins.bid_price1 - ins.price_tick;
			if (action_insert_order.limit_price < ins.lower_limit)
			{
				action_insert_order.limit_price = ins.lower_limit;
			}
		}
	}

	action_insert_order.hedge_flag = kHedgeFlagSpeculation;
	action_insert_order.order_id = order.order_id + "_close_" + std::to_string(nOrderIndex);

	if (ENeedCancelOrderType::all_buy == needCancelOrderType)
	{
		for (auto it : m_data.m_orders)
		{
			const std::string& orderId = it.first;
			const Order& order = it.second;
			if (order.status == kOrderStatusFinished)
			{
				continue;
			}

			if (order.symbol() != symbol)
			{
				continue;
			}

			if ((order.direction == kDirectionBuy)
				&& ((order.offset == kOffsetClose) || (order.offset == kOffsetCloseToday)))
			{
				ActionOrder action_cancel_order;
				action_cancel_order.aid = "cancel_order";
				action_cancel_order.order_id = orderId;
				action_cancel_order.user_id = _req_login.user_name.c_str();
				OnConditionOrderReqCancelOrder(action_cancel_order);
			}
		}
	}
	else if (ENeedCancelOrderType::all_sell == needCancelOrderType)
	{
		for (auto it : m_data.m_orders)
		{
			const std::string& orderId = it.first;
			const Order& order = it.second;
			if (order.status == kOrderStatusFinished)
			{
				continue;
			}

			if (order.symbol() != symbol)
			{
				continue;
			}

			if ((order.direction == kDirectionSell)
				&& ((order.offset == kOffsetClose) || (order.offset == kOffsetCloseToday)))
			{
				ActionOrder action_cancel_order;
				action_cancel_order.aid = "cancel_order";
				action_cancel_order.order_id = orderId;
				action_cancel_order.user_id = _req_login.user_name.c_str();
				OnConditionOrderReqCancelOrder(action_cancel_order);
			}
		}
	}

	if (action_insert_order.volume > 0)
	{
		OnConditionOrderReqInsertOrder(action_insert_order);
	}
	
	return true;
}

bool tradersim::ConditionOrder_Reverse_Long(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	bool b_has_td_yd_distinct = (co.exchange_id == "SHFE") || (co.exchange_id == "INE");
	std::string symbol = co.exchange_id + "." + co.instrument_id;

	//如果有平多单,先撤掉
	for (auto it : m_data.m_orders)
	{
		const std::string& orderId = it.first;
		const Order& order = it.second;
		if (order.status == kOrderStatusFinished)
		{
			continue;
		}

		if (order.symbol() != symbol)
		{
			continue;
		}

		if ((order.direction == kDirectionSell)
			&& ((order.offset == kOffsetClose) || (order.offset == kOffsetCloseToday)))
		{
			ActionOrder action_cancel_order;
			action_cancel_order.aid = "cancel_order";
			action_cancel_order.order_id = orderId;
			action_cancel_order.user_id = _req_login.user_name.c_str();
			OnConditionOrderReqCancelOrder(action_cancel_order);
		}
	}

	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);
	   
	//重新生成平多单
	int volume_long = 0;

	//如果分今昨
	if (b_has_td_yd_distinct)
	{
		//如果有昨仓
		if (pos.pos_long_his > 0)
		{
			volume_long += pos.pos_long_his;

			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;

			//平仓
			action_insert_order.offset = kOffsetClose;
			
			//卖平
			action_insert_order.direction = kDirectionSell;
			
			//数量
			action_insert_order.volume = pos.pos_long_his;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			//价格类型(反手一定用市价平仓)
			action_insert_order.price_type = kPriceTypeLimit;
			action_insert_order.time_condition = kOrderTimeConditionIOC;
			action_insert_order.limit_price = ins.lower_limit;

			action_insert_order.hedge_flag = kHedgeFlagSpeculation;
			action_insert_order.order_id= order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);	

			OnConditionOrderReqInsertOrder(action_insert_order);
		}

		if (pos.pos_long_today > 0)
		{
			volume_long += pos.pos_long_today;

			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;

			//平仓
			action_insert_order.offset = kOffsetCloseToday;			

			//卖平
			action_insert_order.direction = kDirectionSell;

			//数量
			action_insert_order.volume = pos.pos_long_today;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;
			
			//价格类型(反手一定用市价平仓)
			action_insert_order.price_type = kPriceTypeLimit;
			action_insert_order.time_condition = kOrderTimeConditionIOC;
			action_insert_order.limit_price = ins.lower_limit;

			action_insert_order.hedge_flag = kHedgeFlagSpeculation;
			action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);

			OnConditionOrderReqInsertOrder(action_insert_order);
		}
	}
	//如果不分今昨
	else
	{
		//如果有多仓
		if (pos.pos_long_his+pos.pos_long_today > 0)
		{
			volume_long = pos.pos_long_his + pos.pos_long_today;

			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;

			//平仓
			action_insert_order.offset = kOffsetClose;

			//卖平
			action_insert_order.direction = kDirectionSell;

			//数量
			action_insert_order.volume = volume_long;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			//价格类型(反手一定用市价平仓)
			action_insert_order.price_type = kPriceTypeLimit;
			action_insert_order.time_condition = kOrderTimeConditionIOC;
			action_insert_order.limit_price = ins.lower_limit;

			action_insert_order.hedge_flag = kHedgeFlagSpeculation;
			action_insert_order.order_id = order.order_id + "_close_" + std::to_string(nOrderIndex);

			OnConditionOrderReqInsertOrder(action_insert_order);			
		}
	}

	//生成开空单	
	//如果有多仓
	if (volume_long > 0)
	{
		ActionOrder action_insert_order;
		action_insert_order.aid = "insert_order";
		action_insert_order.user_id = _req_login.user_name;
		action_insert_order.exchange_id = co.exchange_id;
		action_insert_order.ins_id = co.instrument_id;

		//开仓
		action_insert_order.offset = kOffsetOpen;
		
		//开空
		action_insert_order.direction = kDirectionSell;

		//数量
		action_insert_order.volume = volume_long;
		action_insert_order.volume_condition = kOrderVolumeConditionAny;

		//价格(反手一定用市价开仓)
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionIOC;
		action_insert_order.limit_price = ins.lower_limit;

		action_insert_order.hedge_flag = kHedgeFlagSpeculation;
		action_insert_order.order_id = order.order_id + "_open_" + std::to_string(nOrderIndex);	

		OnConditionOrderReqInsertOrder(action_insert_order);
	}

	return true;
}

bool tradersim::ConditionOrder_Reverse_Short(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, int nOrderIndex)
{
	bool b_has_td_yd_distinct = (co.exchange_id == "SHFE") || (co.exchange_id == "INE");
	std::string symbol = co.exchange_id + "." + co.instrument_id;

	//如果有平空单,先撤掉
	for (auto it : m_data.m_orders)
	{
		const std::string& orderId = it.first;
		const Order& order = it.second;
		if (order.status == kOrderStatusFinished)
		{
			continue;
		}

		if (order.symbol() != symbol)
		{
			continue;
		}

		if ((order.direction == kDirectionBuy)
			&& ((order.offset == kOffsetClose) || (order.offset == kOffsetCloseToday)))
		{
			ActionOrder action_cancel_order;
			action_cancel_order.aid = "cancel_order";
			action_cancel_order.order_id = orderId;
			action_cancel_order.user_id = _req_login.user_name.c_str();
			OnConditionOrderReqCancelOrder(action_cancel_order);
		}

	}

	//重新生成平空单
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	int volume_short = 0;

	//如果分今昨
	if (b_has_td_yd_distinct)
	{
		//如果有昨仓
		if (pos.pos_short_his > 0)
		{
			volume_short += pos.pos_short_his;

			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;

			//平仓
			action_insert_order.offset = kOffsetClose;
			
			//买平
			action_insert_order.direction = kDirectionBuy;
			
			//数量
			action_insert_order.volume= pos.pos_short_his;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;
			
			//价格类型(反手一定用市价平仓)
			action_insert_order.price_type = kPriceTypeLimit;
			action_insert_order.time_condition = kOrderTimeConditionIOC;
			action_insert_order.limit_price = ins.upper_limit;

			
			action_insert_order.hedge_flag = kHedgeFlagSpeculation;
			action_insert_order.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);

			OnConditionOrderReqInsertOrder(action_insert_order);
		}

		//如果有今仓
		if (pos.pos_short_today > 0)
		{
			volume_short += pos.pos_short_today;

			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;

			//平仓
			action_insert_order.offset = kOffsetCloseToday;
			
			//买平
			action_insert_order.direction = kDirectionBuy;

			//数量
			action_insert_order.volume = pos.pos_short_today;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;
			
			//价格类型(反手一定用市价平仓)
			action_insert_order.price_type = kPriceTypeLimit;
			action_insert_order.time_condition = kOrderTimeConditionIOC;
			action_insert_order.limit_price = ins.upper_limit;

			action_insert_order.hedge_flag = kHedgeFlagSpeculation;
			action_insert_order.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);	

			OnConditionOrderReqInsertOrder(action_insert_order);
		}
	}
	//如果不分今昨
	else
	{
		//如果有空仓
		if (pos.pos_short_his+pos.pos_short_today > 0)
		{
			volume_short = pos.pos_short_his + pos.pos_short_today;

			ActionOrder action_insert_order;
			action_insert_order.aid = "insert_order";
			action_insert_order.user_id = _req_login.user_name;
			action_insert_order.exchange_id = co.exchange_id;
			action_insert_order.ins_id = co.instrument_id;

			//平仓
			action_insert_order.offset = kOffsetClose;

			//买平
			action_insert_order.direction = kDirectionBuy;

			//数量
			action_insert_order.volume = volume_short;
			action_insert_order.volume_condition = kOrderVolumeConditionAny;

			//价格类型(反手一定用市价平仓)
			action_insert_order.price_type = kPriceTypeLimit;
			action_insert_order.time_condition = kOrderTimeConditionIOC;
			action_insert_order.limit_price = ins.upper_limit;

			action_insert_order.hedge_flag = kHedgeFlagSpeculation;
			action_insert_order.order_id = order.order_id + "_close_" + std::to_string(nOrderIndex);

			OnConditionOrderReqInsertOrder(action_insert_order);		
		}
	}

	//生成开多单
	//如果有空仓
	if (volume_short > 0)
	{
		ActionOrder action_insert_order;
		action_insert_order.aid = "insert_order";
		action_insert_order.user_id = _req_login.user_name;
		action_insert_order.exchange_id = co.exchange_id;
		action_insert_order.ins_id = co.instrument_id;
		
		//开仓
		action_insert_order.offset = kOffsetOpen;

		//开多
		action_insert_order.direction = kDirectionBuy;

		//数量
		action_insert_order.volume = volume_short;
		action_insert_order.volume_condition = kOrderVolumeConditionAny;

		//价格(反手一定用市价开仓)
		action_insert_order.price_type = kPriceTypeLimit;
		action_insert_order.time_condition = kOrderTimeConditionIOC;
		action_insert_order.limit_price = ins.upper_limit;
			
		action_insert_order.hedge_flag = kHedgeFlagSpeculation;
		action_insert_order.order_id = order.order_id + "_open_" + std::to_string(nOrderIndex);

		OnConditionOrderReqInsertOrder(action_insert_order);		
	}

	return true;
}

void tradersim::OnConditionOrderReqInsertOrder(ActionOrder action_insert_order)
{
	std::string symbol = action_insert_order.exchange_id + "." + action_insert_order.ins_id;
	if (action_insert_order.order_id.empty())
	{
		action_insert_order.order_id =
			std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	SerializerSim ss;
	ss.FromVar(action_insert_order);
	std::string strMsg;
	ss.ToString(&strMsg);

	std::string order_key = action_insert_order.order_id;
	auto it = m_data.m_orders.find(order_key);
	if (it != m_data.m_orders.end())
	{
		Log().WithField("fun","OnConditionOrderReqInsertOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("diff_req_pack",strMsg)
			.Log(LOG_INFO,u8"下单,已被服务器拒绝,原因:单号重复");

		OutputNotifyAllSycn(407, u8"下单, 已被服务器拒绝,原因:单号重复","WARNING");
		return;
	}

	m_something_changed = true;
	const Instrument* ins = GetInstrument(symbol);
	Order* order = &(m_data.m_orders[order_key]);
	order->user_id = action_insert_order.user_id;
	order->exchange_id = action_insert_order.exchange_id;
	order->instrument_id = action_insert_order.ins_id;
	order->order_id = action_insert_order.order_id;
	order->exchange_order_id = order->order_id;
	order->direction = action_insert_order.direction;
	order->offset = action_insert_order.offset;
	order->price_type = action_insert_order.price_type;
	order->limit_price = action_insert_order.limit_price;
	order->volume_orign = action_insert_order.volume;
	order->volume_left = action_insert_order.volume;
	order->status = kOrderStatusAlive;
	order->volume_condition = action_insert_order.volume_condition;
	order->time_condition = action_insert_order.time_condition;
	order->insert_date_time = GetLocalEpochNano();
	order->seqno = m_last_seq_no++;

	if (action_insert_order.user_id.substr(0, m_user_id.size()) != m_user_id)
	{
		Log().WithField("fun","OnConditionOrderReqInsertOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("diff_req_pack",strMsg)
			.Log(LOG_INFO, u8"下单,已被服务器拒绝,原因:下单指令中的用户名错误");

		OutputNotifyAllSycn(408,u8"下单,已被服务器拒绝, 原因:下单指令中的用户名错误","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	if (nullptr==ins)
	{
		Log().WithField("fun","OnConditionOrderReqInsertOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("diff_req_pack",strMsg)
			.Log(LOG_INFO, u8"下单,已被服务器拒绝, 原因:合约不合法");

		OutputNotifyAllSycn(409,u8"下单,已被服务器拒绝, 原因:合约不合法","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	if (ins->product_class != kProductClassFutures)
	{
		Log().WithField("fun", "OnConditionOrderReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单,已被服务器拒绝, 原因:模拟交易只支持期货合约");

		OutputNotifyAllSycn(410,u8"下单,已被服务器拒绝, 原因:模拟交易只支持期货合约","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	if (action_insert_order.volume <= 0)
	{
		Log().WithField("fun", "OnConditionOrderReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单,已被服务器拒绝, 原因:下单手数应该大于0");

		OutputNotifyAllSycn(411,u8"下单,已被服务器拒绝, 原因:下单手数应该大于0","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	double xs = action_insert_order.limit_price / ins->price_tick;
	if (xs - int(xs + 0.5) >= 0.001)
	{
		Log().WithField("fun", "OnConditionOrderReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithPack("diff_req_pack", strMsg)
			.Log(LOG_INFO, u8"下单,已被服务器拒绝, 原因:下单价格不是价格单位的整倍数");

		OutputNotifyAllSycn(412,u8"下单,已被服务器拒绝, 原因:下单价格不是价格单位的整倍数","WARNING");
		order->status = kOrderStatusFinished;
		return;
	}

	Position* position = &(m_data.m_positions[symbol]);
	position->ins = ins;
	position->instrument_id = order->instrument_id;
	position->exchange_id = order->exchange_id;
	position->user_id = m_user_id;

	//如果是开仓
	if (action_insert_order.offset == kOffsetOpen)
	{
		if (position->ins->margin * action_insert_order.volume > m_account->available)
		{
			Log().WithField("fun","OnConditionOrderReqInsertOrder")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithPack("diff_req_pack", strMsg)
				.Log(LOG_INFO,u8"下单,已被服务器拒绝,原因:开仓保证金不足");

			OutputNotifyAllSycn(413,u8"下单, 已被服务器拒绝, 原因:开仓保证金不足","WARNING");
			order->status = kOrderStatusFinished;
			return;
		}
	}
	//如果是平仓
	else
	{
		//上期和原油交易所(区分平今平昨)
		if ((position->exchange_id == "SHFE" || position->exchange_id == "INE"))
		{
			//平今
			if (order->offset == kOffsetCloseToday)
			{
				if (
					(action_insert_order.direction == kDirectionBuy
						&& position->volume_short_today < action_insert_order.volume + position->volume_short_frozen_today)
					|| (action_insert_order.direction == kDirectionSell
						&& position->volume_long_today < action_insert_order.volume + position->volume_long_frozen_today)
					)
				{
					Log().WithField("fun", "OnConditionOrderReqInsertOrder")
						.WithField("key", _key)
						.WithField("bid", _req_login.bid)
						.WithField("user_name", _req_login.user_name)
						.WithPack("diff_req_pack", strMsg)
						.Log(LOG_INFO, u8"下单,已被服务器拒绝,原因:平今手数超过今仓持仓量");

					OutputNotifyAllSycn(414
						, u8"下单,已被服务器拒绝,原因:平今手数超过今仓持仓量", "WARNING");
					order->status = kOrderStatusFinished;
					return;
				}
			}
			//平昨
			else if (order->offset == kOffsetClose)
			{
				if (
					(action_insert_order.direction == kDirectionBuy
						&& position->volume_short_his < action_insert_order.volume + position->volume_short_frozen_his)
					|| (action_insert_order.direction == kDirectionSell
						&& position->volume_long_his < action_insert_order.volume + position->volume_long_frozen_his)
					)
				{
					Log().WithField("fun","OnConditionOrderReqInsertOrder")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithPack("diff_req_pack",strMsg)
						.Log(LOG_INFO, u8"下单,已被服务器拒绝,原因:平昨手数超过昨仓持仓量");

					OutputNotifyAllSycn(415,u8"下单,已被服务器拒绝,原因:平昨手数超过昨仓持仓量","WARNING");
					order->status = kOrderStatusFinished;
					return;
				}
			}
		}
		else
			//其它交易所(不区分平今平昨)
		{
			if (
				(action_insert_order.direction == kDirectionBuy
					&& position->volume_short < action_insert_order.volume + position->volume_short_frozen_today)
				|| (action_insert_order.direction == kDirectionSell
					&& position->volume_long < action_insert_order.volume + position->volume_long_frozen_today)
				)
			{
				Log().WithField("fun", "OnConditionOrderReqInsertOrder")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithPack("diff_req_pack", strMsg)
					.Log(LOG_INFO, u8"下单,已被服务器拒绝,原因:平仓手数超过持仓量");

				OutputNotifyAllSycn(416,u8"下单,已被服务器拒绝,原因:平仓手数超过持仓量","WARNING");
				order->status = kOrderStatusFinished;
				return;
			}
		}
	}

	m_alive_order_set.insert(order);

	UpdateOrder(order);
	
	Log().WithField("fun", "OnConditionOrderReqInsertOrder")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithPack("diff_req_pack", strMsg)
		.Log(LOG_INFO, u8"下单成功");

	OutputNotifyAllSycn(417, u8"下单成功");

	//尝试匹配成交
	TryOrderMatch();

	//重算所有持仓项的持仓盈亏和浮动盈亏
	RecaculatePositionAndFloatProfit();	
}

void tradersim::OnConditionOrderReqCancelOrder(ActionOrder action_cancel_order)
{
	if (action_cancel_order.user_id.substr(0, m_user_id.size()) != m_user_id)
	{
		Log().WithField("fun","OnConditionOrderReqCancelOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("order_id", action_cancel_order.order_id)
			.Log(LOG_INFO, u8"撤单,已被服务器拒绝,原因:撤单指令中的用户名错误");

		OutputNotifyAllSycn(418,u8"撤单,已被服务器拒绝, 原因:撤单指令中的用户名错误","WARNING");
		return;
	}

	for (auto it_order = m_alive_order_set.begin(); it_order != m_alive_order_set.end(); ++it_order)
	{
		Order* order = *it_order;
		if (order->order_id == action_cancel_order.order_id
			&& order->status == kOrderStatusAlive)
		{
			order->status = kOrderStatusFinished;
			UpdateOrder(order);
			m_something_changed = true;

			Log().WithField("fun", "OnConditionOrderReqCancelOrder")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("order_id", action_cancel_order.order_id)
				.Log(LOG_INFO,u8"撤单成功");

			OutputNotifyAllSycn(419,u8"撤单成功");			
			return;
		}
	}
	
	Log().WithField("fun", "OnConditionOrderReqCancelOrder")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("order_id", action_cancel_order.order_id)
		.Log(LOG_INFO, u8"要撤销的单不存在");

	OutputNotifyAllSycn(420,u8"要撤销的单不存在","WARNING");
}

void tradersim::SendDataDirect(int connId, const std::string& msg)
{
	std::shared_ptr<std::string> msg_ptr(new std::string(msg));
	_ios.post(boost::bind(&tradersim::SendMsg,this,connId,msg_ptr));
}

#pragma endregion




