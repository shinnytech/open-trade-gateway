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

#include <iostream>
#include <string>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

tradersim::tradersim(boost::asio::io_context& ios
	, const std::string& key)
:m_b_login(false)
, _key(key)
,_ios(ios)
,_out_mq_ptr()
,_out_mq_name(_key + "_msg_out")
,_in_mq_ptr()
,_in_mq_name(_key + "_msg_in")
,_thread_ptr()
,_req_login()
,m_user_file_path("")
,m_alive_order_set()
,m_loging_connectId(-1)
,m_logined_connIds()
,m_account(NULL)
,m_something_changed(false)
,m_notify_seq(0)
,m_data_seq(0)
,m_last_seq_no(0)
,m_peeking_message(false)
,m_next_send_dt(0)
,m_transfer_seq(0)
,m_run_receive_msg(false)
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
		Log(LOG_ERROR,nullptr
			, "msg=tradersim open message queue exception;errmsg=%s;key=%s"
			, ex.what()
			,_key.c_str());
	}

	try
	{
		m_run_receive_msg.store(true);
		_thread_ptr = boost::make_shared<boost::thread>(
			boost::bind(&tradersim::ReceiveMsg, this, _key));
	}
	catch (const std::exception& ex)
	{
		Log(LOG_ERROR,nullptr
			, "msg=tradersim start ReceiveMsg thread;errmsg=%s;key=%s"
			, ex.what()
			, _key.c_str());
	}
}

void tradersim::Stop()
{
	if (nullptr != _thread_ptr)
	{
		m_run_receive_msg.store(false);
		_thread_ptr->join();
		//_thread_ptr->detach();
		//_thread_ptr.reset();
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
				Log(LOG_WARNING,nullptr
					, "msg=tradersim ReceiveMsg is invalid!;key=%s;msgcontent=%s"
					, _key.c_str()
					, line.c_str());
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
			Log(LOG_ERROR,nullptr
				, "msg=ReceiveMsg exception;errmsg=%s;key=%s"
				, ex.what()
				, _key.c_str());
		}
	}
}

void tradersim::ProcessReqLogIn(int connId, ReqLogin& req)
{
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
			OutputNotifySycn(connId, 0, u8"重复发送登录请求!");
			return;
		}

		//简单比较登陆凭证,判断是否能否成功登录
		if ((_req_login.bid == req.bid)
			&& (_req_login.user_name == req.user_name)
			&& (_req_login.password == req.password))
		{
			OutputNotifySycn(connId, 0, u8"登录成功");
			Log(LOG_INFO, nullptr
				, "fun=ProcessReqLogIn;key=%s;bid=%s;user_name=%s;loginresult=true"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
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

			//加入登录客户端列表
			m_logined_connIds.push_back(connId);
		}
		else
		{
			OutputNotifySycn(connId, 0, u8"用户登录失败!");
			Log(LOG_INFO, nullptr
				, "fun=ProcessReqLogIn;key=%s;bid=%s;user_name=%s;loginresult=false"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
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
		}
		m_user_id = _req_login.user_name;
		m_data.user_id = m_user_id;
		LoadUserDataFile();
		m_loging_connectId = connId;
		m_b_login = WaitLogIn();
		if (m_b_login.load())
		{
			OutputNotifySycn(connId, 0, u8"登录成功");
			Log(LOG_INFO, nullptr
				, "fun=ProcessReqLogIn;key=%s;bid=%s;user_name=%s;loginresult=true"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
			OnInit();
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
			SendMsg(connId, msg_ptr);
			//加入登录客户端列表
			m_logined_connIds.push_back(connId);
		}
		else
		{
			OutputNotifySycn(connId, 0, u8"用户登录失败!");
			Log(LOG_INFO, nullptr
				, "fun=ProcessReqLogIn;key=%s;bid=%s;user_name=%s;loginresult=false"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
		}
	}
}

bool tradersim::WaitLogIn()
{
	return true;
}

void tradersim::CloseConnection(int nId)
{
	Log(LOG_INFO,nullptr
		, "msg=tradersim CloseConnection;key=%s;bid=%s;user_name=%s;connid=%d"
		, _key.c_str()
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, nId);

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
		Log(LOG_WARNING,nullptr
			, "msg=tradersim parse json fail;key=%s;bid=%s;user_name=%s;connid=%d;msgcontent=%s"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, connId
			, msg.c_str());
		return;
	}

	ReqLogin req;
	ss.ToVar(req);
	if (req.aid == "req_login")
	{
		ProcessReqLogIn(connId, req);
	}
	else
	{
		if (!m_b_login)
		{
			Log(LOG_WARNING,msg.c_str()
				, "msg=trade sim receive other msg before login;key=%s;bid=%s;user_name=%s;connid=%d"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			return;
		}

		if (!IsConnectionLogin(connId))
		{
			Log(LOG_WARNING,msg.c_str()
				, "msg=trade sim receive other msg which from not login connecion;key=%s;bid=%s;user_name=%s;connid=%d"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			return;
		}

		SerializerSim ss;
		if (!ss.FromString(msg.c_str()))
			return;
		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ss.m_doc));
		if (!dt || !dt->IsString())
			return;
		std::string aid = dt->GetString();
		if (aid == "insert_order")
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
		else if (aid == "peek_message")
		{
			OnClientPeekMessage();
		}
	}
}

void tradersim::OnIdle()
{
	long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (m_peeking_message && (m_next_send_dt < now))
	{
		m_next_send_dt = now + 100;
		SendUserData();
	}
}

void tradersim::OutputNotifyAsych(int connId, long notify_code, const std::string& notify_msg
	, const char* level, const char* type)
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
	rapidjson::Pointer("/data/0/notify/N" + std::to_string(m_notify_seq++)).Set(*nss.m_doc, node_message);
	std::string json_str;
	nss.ToString(&json_str);
	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	_ios.post(boost::bind(&tradersim::SendMsg, this, connId, msg_ptr));
}

void tradersim::OutputNotifySycn(int connId, long notify_code
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
	rapidjson::Pointer("/data/0/notify/N" + std::to_string(m_notify_seq++)).Set(*nss.m_doc, node_message);
	std::string json_str;
	nss.ToString(&json_str);
	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	SendMsg(connId, msg_ptr);
}

void tradersim::OutputNotifyAllAsych(long notify_code
	, const std::string& ret_msg, const char* level
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
	node_message.AddMember("content", rapidjson::Value(ret_msg.c_str(), nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/notify/N" + std::to_string(m_notify_seq++)).Set(*nss.m_doc, node_message);
	std::string json_str;
	nss.ToString(&json_str);
	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	_ios.post(boost::bind(&tradersim::SendMsgAll, this, msg_ptr));
}

void tradersim::OutputNotifyAllSycn(long notify_code
	, const std::string& ret_msg, const char* level
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
	node_message.AddMember("content", rapidjson::Value(ret_msg.c_str(), nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/notify/N" + std::to_string(m_notify_seq++)).Set(*nss.m_doc, node_message);
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
			Log(LOG_ERROR,nullptr
				, "msg=SendMsg exception;errmsg=%s;length=%d;key=%s"
				, ex.what()
				, msg.length()
				,_key.c_str());
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
			Log(LOG_ERROR,msg.c_str()
				, "msg=SendMsg exception;errmsg=%s;length=%d;key=%s"
				, ex.what()				
				, totalLength
				, _key.c_str());
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
			Log(LOG_ERROR, nullptr
				,"msg=SendMsg exception;errmsg=%s;length=%d;key=%s"
				,ex.what()
				,msg.length()
				,_key.c_str());
		}
	}
	else
	{
		try
		{
			_out_mq_ptr->send(msg.c_str(), totalLength, 0);
		}
		catch (std::exception& ex)
		{
			Log(LOG_ERROR,msg.c_str()
				,"msg=SendMsg exception;errmsg=%s;length=%d;key=%s"
				, ex.what()
				,totalLength
				,_key.c_str());
		}
	}
}

void tradersim::SendUserDataImd(int connectId)
{
	//构建数据包		
	SerializerTradeBase nss;
	nss.dump_all = true;
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
	SendMsg(connectId, msg_ptr);
}

void tradersim::SendUserData()
{
	if (!m_peeking_message)
		return;
	//尝试匹配成交
	TryOrderMatch();
	//重算所有持仓项的持仓盈亏和浮动盈亏
	double total_position_profit = 0;
	double total_float_profit = 0;
	double total_margin = 0;
	double total_frozen_margin = 0.0;
	for (auto it = m_data.m_positions.begin();
		it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;
		Position& ps = it->second;
		if (!ps.ins)
			ps.ins = GetInstrument(symbol);
		if (!ps.ins)
		{
			Log(LOG_ERROR,nullptr
				, "msg=miss symbol %s when processing position;key=%s"
				, symbol.c_str()
				,_key.c_str());
			continue;
		}
		double last_price = ps.ins->last_price;
		if (!IsValid(last_price))
			last_price = ps.ins->pre_settlement;
		if ((IsValid(last_price) && (last_price != ps.last_price)) || ps.changed) {
			ps.last_price = last_price;
			ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
			ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
			ps.position_profit = ps.position_profit_long + ps.position_profit_short;
			ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
			ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
			ps.float_profit = ps.float_profit_long + ps.float_profit_short;
			ps.changed = true;
			m_something_changed = true;
		}
		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;
		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;
		if (IsValid(ps.margin))
			total_margin += ps.margin;
		total_frozen_margin += ps.frozen_margin;
	}
	//重算资金账户
	if (m_something_changed)
	{
		m_account->position_profit = total_position_profit;
		m_account->float_profit = total_float_profit;
		m_account->balance = m_account->static_balance + m_account->float_profit + m_account->close_profit - m_account->commission;
		m_account->margin = total_margin;
		m_account->frozen_margin = total_frozen_margin;
		m_account->available = m_account->balance - m_account->margin - m_account->frozen_margin;
		if (IsValid(m_account->available) && IsValid(m_account->balance) && !IsZero(m_account->balance))
			m_account->risk_ratio = 1.0 - m_account->available / m_account->balance;
		else
			m_account->risk_ratio = NAN;
		m_account->changed = true;
	}
	if (!m_something_changed)
		return;
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
	m_something_changed = false;
	m_peeking_message = false;
}

#pragma endregion


#pragma region businesslogic

void SerializerSim::DefineStruct(ActionOrder& d)
{
	AddItem(d.aid, "aid");
	AddItem(d.order_id, "order_id");
	AddItem(d.user_id, "user_id");
	AddItem(d.exchange_id, "exchange_id");
	AddItem(d.ins_id, "instrument_id");
	AddItemEnum(d.direction, "direction", {
		{ kDirectionBuy, "BUY" },
		{ kDirectionSell, "SELL" },
		});
	AddItemEnum(d.offset, "offset", {
		{ kOffsetOpen, "OPEN" },
		{ kOffsetClose, "CLOSE" },
		{ kOffsetCloseToday, "CLOSETODAY" },
		});
	AddItemEnum(d.price_type, "price_type", {
		{ kPriceTypeLimit, "LIMIT" },
		{ kPriceTypeAny, "ANY" },
		{ kPriceTypeBest, "BEST" },
		{ kPriceTypeFiveLevel, "FIVELEVEL" },
		});
	AddItemEnum(d.volume_condition, "volume_condition", {
		{ kOrderVolumeConditionAny, "ANY" },
		{ kOrderVolumeConditionMin, "MIN" },
		{ kOrderVolumeConditionAll, "ALL" },
		});
	AddItemEnum(d.time_condition, "time_condition", {
		{ kOrderTimeConditionIOC, "IOC" },
		{ kOrderTimeConditionGFS, "GFS" },
		{ kOrderTimeConditionGFD, "GFD" },
		{ kOrderTimeConditionGTD, "GTD" },
		{ kOrderTimeConditionGTC, "GTC" },
		{ kOrderTimeConditionGFA, "GFA" },
		});
	AddItem(d.volume, "volume");
	AddItem(d.limit_price, "limit_price");
}

void SerializerSim::DefineStruct(ActionTransfer& d)
{
	AddItem(d.currency, "currency");
	AddItem(d.amount, "amount");
}

void tradersim::OnInit()
{
	m_account = &(m_data.m_accounts["CNY"]);
	if (m_account->static_balance < 1.0)
	{
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
	Log(LOG_INFO,nullptr 
		,"msg=sim OnInit;key=%s"
		, _key.c_str());
}

void tradersim::LoadUserDataFile()
{
	Log(LOG_INFO, nullptr
		, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;msg=ready to load user data file"
		, _key.c_str()
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str());

	if (m_user_file_path.empty())
	{
		Log(LOG_INFO, nullptr
			, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;msg=m_user_file_path is empty"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str());
		return;
	}
	
	//加载存档文件
	std::string fn = m_user_file_path + "/" + m_user_id;

	Log(LOG_INFO, nullptr
		, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s"
		, _key.c_str()
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, fn.c_str());

	//加载存档文件
	SerializerTradeBase nss;
	bool loadfile=nss.FromFile(fn.c_str());
	if (!loadfile)
	{
		Log(LOG_INFO, nullptr
			, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s;msg=load file failed!"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, fn.c_str());
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
		else
		{
			Log(LOG_INFO, nullptr
				, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s;msg=miss sysmbol in position!;symbol=%s"
				, _key.c_str()
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, fn.c_str()
				, position.symbol().c_str());
			it = m_data.m_positions.erase(it);
		}			
	}
	/*如果不是当天的存档文件, 则需要调整
		委托单和成交记录全部清空
		用户权益转为昨权益, 平仓盈亏
		持仓手数全部移动到昨仓, 持仓均价调整到昨结算价
	*/
	if (m_data.trading_day != g_config.trading_day)
	{
		Log(LOG_INFO, nullptr
			, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s;msg=diffrent trading day!;old_trading_day=%s;trading_day=%s"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, fn.c_str()
			, m_data.trading_day.c_str()
			, g_config.trading_day.c_str());

		m_data.m_orders.clear();
		m_data.m_trades.clear();
		m_data.m_transfers.clear();
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
		}
		for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end(); ++it)
		{
			Position& item = it->second;
			item.volume_long_his = item.volume_long_his + item.volume_long_today;
			item.volume_long_today = 0;
			item.volume_long_frozen_today = 0;
			item.volume_short_his = item.volume_short_his + item.volume_short_today;
			item.volume_long_yd = item.volume_long_his;
			item.volume_short_yd= item.volume_short_his;
			item.volume_short_today = 0;
			item.volume_short_frozen_today = 0;
			item.changed = true;
		}
		m_data.trading_day = g_config.trading_day;
	}
	else
	{
		Log(LOG_INFO, nullptr
			, "fun=LoadUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s;msg=same trading day!;trading_day=%s"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, fn.c_str()
			, m_data.trading_day.c_str());

		for (auto it = m_data.m_orders.begin(); it != m_data.m_orders.end(); ++it)
		{
			m_alive_order_set.insert(&(it->second));
		}
	}
}

void tradersim::SaveUserDataFile()
{
	Log(LOG_INFO, nullptr
		, "fun=SaveUserDataFile;key=%s;bid=%s;user_name=%s;msg=ready to save user data file"
		, _key.c_str()
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str());

	if (m_user_file_path.empty())
	{
		Log(LOG_INFO, nullptr
			, "fun=SaveUserDataFile;key=%s;bid=%s;user_name=%s;msg=user file path is empty"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str());
		return;
	}
	std::string fn = m_user_file_path + "/" + m_user_id;
	Log(LOG_INFO, nullptr
		, "fun=SaveUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s;msg=user file path is empty"
		, _key.c_str()
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, fn.c_str());
	SerializerTradeBase nss;
	nss.dump_all = true;
	m_data.trading_day = g_config.trading_day;
	nss.FromVar(m_data);
	bool saveFile=nss.ToFile(fn.c_str());
	if (!saveFile)
	{
		Log(LOG_INFO, nullptr
			, "fun=SaveUserDataFile;key=%s;bid=%s;user_name=%s;fn=%s;msg=save file failed!"
			, _key.c_str()
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, fn.c_str());
	}
}


void tradersim::UpdateOrder(Order* order)
{
	order->seqno = m_last_seq_no++;
	order->changed = true;
	Position& position = GetPosition(order->symbol());
	assert(position.ins);
	UpdatePositionVolume(&position);
}

Position& tradersim::GetPosition(const std::string symbol)
{
	Position& position = m_data.m_positions[symbol];
	return position;
}

void tradersim::UpdatePositionVolume(Position* position)
{
	position->frozen_margin = 0;
	position->volume_long_frozen_today = 0;
	position->volume_short_frozen_today = 0;
	for (auto it_order = m_alive_order_set.begin()
		; it_order != m_alive_order_set.end()
		; ++it_order)
	{
		Order* order = *it_order;
		if (order->status != kOrderStatusAlive)
			continue;
		if (position->instrument_id != order->instrument_id)
			continue;
		if (order->offset == kOffsetOpen)
		{
			position->frozen_margin += position->ins->margin * order->volume_left;
		}
		else
		{
			if (order->direction == kDirectionBuy)
				position->volume_short_frozen_today += order->volume_left;
			else
				position->volume_long_frozen_today += order->volume_left;
		}
	}
	position->volume_long_frozen = position->volume_long_frozen_his + position->volume_long_frozen_today;
	position->volume_short_frozen = position->volume_short_frozen_his + position->volume_short_frozen_today;
	position->volume_long = position->volume_long_his + position->volume_long_today;
	position->volume_short = position->volume_short_his + position->volume_short_today;
	position->margin_long = position->ins->margin * position->volume_long;
	position->margin_short = position->ins->margin * position->volume_short;
	position->margin = position->margin_long + position->margin_short;
	if (position->volume_long > 0)
	{
		position->open_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);
		position->position_price_long = position->position_cost_long / (position->volume_long * position->ins->volume_multiple);
	}
	if (position->volume_short > 0)
	{
		position->open_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
		position->position_price_short = position->position_cost_short / (position->volume_short * position->ins->volume_multiple);
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

void tradersim::CheckOrderTrade(Order* order)
{
	auto ins = GetInstrument(order->symbol());
	if (order->price_type == kPriceTypeLimit)
	{
		if (order->limit_price - 0.0001 > ins->upper_limit)
		{
			OutputNotifyAllSycn(1
				, u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格超出涨停板", "WARNING");
			order->status = kOrderStatusFinished;
			UpdateOrder(order);
			return;
		}
		if (order->limit_price + 0.0001 < ins->lower_limit)
		{
			OutputNotifyAllSycn(1
				, u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格跌破跌停板"
				, "WARNING");
			order->status = kOrderStatusFinished;
			UpdateOrder(order);
			return;
		}
	}
	if (order->direction == kDirectionBuy && IsValid(ins->ask_price1)
		&& (order->price_type == kPriceTypeAny || order->limit_price >= ins->ask_price1))
		DoTrade(order, order->volume_left, ins->ask_price1);
	if (order->direction == kDirectionSell && IsValid(ins->bid_price1)
		&& (order->price_type == kPriceTypeAny || order->limit_price <= ins->bid_price1))
		DoTrade(order, order->volume_left, ins->bid_price1);
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

	//生成成交通知
	std::stringstream ss;
	ss << u8"成交通知,合约:" << trade->exchange_id
		<< u8"." << trade->instrument_id << u8",手数:" << trade->volume << "!";
	OutputNotifyAllSycn(1, ss.str().c_str());

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
	//调整持仓数据
	Position* position = &(m_data.m_positions[order->symbol()]);
	double commission = position->ins->commission * volume;
	trade->commission = commission;
	if (order->offset == kOffsetOpen)
	{
		if (order->direction == kDirectionBuy)
		{
			position->volume_long_today += volume;
			position->open_cost_long += price * volume * position->ins->volume_multiple;
			position->position_cost_long += price * volume * position->ins->volume_multiple;
			position->open_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);
			position->position_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);
		}
		else
		{
			position->volume_short_today += volume;
			position->open_cost_short += price * volume * position->ins->volume_multiple;
			position->position_cost_short += price * volume * position->ins->volume_multiple;
			position->open_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
			position->position_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
		}
	}
	else
	{
		double close_profit = 0;
		if (order->direction == kDirectionBuy)
		{
			position->open_cost_short = position->open_cost_short * (position->volume_short - volume) / position->volume_short;
			position->position_cost_short = position->position_cost_short * (position->volume_short - volume) / position->volume_short;
			close_profit = (position->position_price_short - price) * volume * position->ins->volume_multiple;
			position->volume_short_today -= volume;
		}
		else
		{
			position->open_cost_long = position->open_cost_long * (position->volume_long - volume) / position->volume_long;
			position->position_cost_long = position->position_cost_long * (position->volume_long - volume) / position->volume_long;
			close_profit = (price - position->position_price_long) * volume * position->ins->volume_multiple;
			position->volume_long_today -= volume;
		}
		m_account->close_profit += close_profit;
	}
	//调整账户资金
	m_account->commission += commission;
	UpdatePositionVolume(position);
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

	std::string order_key = action_insert_order.order_id;
	auto it = m_data.m_orders.find(order_key);
	if (it != m_data.m_orders.end())
	{
		OutputNotifyAllSycn(1
			, u8"下单, 已被服务器拒绝,原因:单号重复", "WARNING");
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
		OutputNotifyAllSycn(1
			, u8"下单, 已被服务器拒绝, 原因:下单指令中的用户名错误", "WARNING");
		order->status = kOrderStatusFinished;
		return;
	}
	if (!ins)
	{
		OutputNotifyAllSycn(1
			, u8"下单, 已被服务器拒绝, 原因:合约不合法", "WARNING");
		order->status = kOrderStatusFinished;
		return;
	}
	if (ins->product_class != kProductClassFutures)
	{
		OutputNotifyAllSycn(1
			, u8"下单, 已被服务器拒绝, 原因:模拟交易只支持期货合约", "WARNING");
		order->status = kOrderStatusFinished;
		return;
	}
	if (action_insert_order.volume <= 0)
	{
		OutputNotifyAllSycn(1
			, u8"下单, 已被服务器拒绝, 原因:下单手数应该大于0", "WARNING");
		order->status = kOrderStatusFinished;
		return;
	}
	double xs = action_insert_order.limit_price / ins->price_tick;
	if (xs - int(xs + 0.5) >= 0.001)
	{
		OutputNotifyAllSycn(1
			, u8"下单, 已被服务器拒绝, 原因:下单价格不是价格单位的整倍数", "WARNING");
		order->status = kOrderStatusFinished;
		return;
	}
	Position* position = &(m_data.m_positions[symbol]);
	position->ins = ins;
	position->instrument_id = order->instrument_id;
	position->exchange_id = order->exchange_id;
	position->user_id = m_user_id;
	if (action_insert_order.offset == kOffsetOpen)
	{
		if (position->ins->margin * action_insert_order.volume > m_account->available)
		{
			OutputNotifyAllSycn(1
				, u8"下单, 已被服务器拒绝, 原因:开仓保证金不足", "WARNING");
			order->status = kOrderStatusFinished;
			return;
		}
	}
	else
	{
		if ((action_insert_order.direction == kDirectionBuy && position->volume_short < action_insert_order.volume + position->volume_short_frozen_today)
			|| (action_insert_order.direction == kDirectionSell && position->volume_long < action_insert_order.volume + position->volume_long_frozen_today))
		{
			OutputNotifyAllSycn(1
				, u8"下单, 已被服务器拒绝, 原因:平仓手数超过持仓量", "WARNING");
			order->status = kOrderStatusFinished;
			return;
		}
	}
	m_alive_order_set.insert(order);
	UpdateOrder(order);
	SaveUserDataFile();
	OutputNotifyAllSycn(1, u8"下单成功");
	return;
}

void tradersim::OnClientReqCancelOrder(ActionOrder action_cancel_order)
{
	if (action_cancel_order.user_id.substr(0, m_user_id.size()) != m_user_id)
	{
		OutputNotifyAllSycn(1
			, u8"撤单, 已被服务器拒绝, 原因:撤单指令中的用户名错误", "WARNING");
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
			OutputNotifyAllSycn(1, u8"撤单成功");
			return;
		}
	}
	OutputNotifyAllSycn(1, u8"要撤销的单不存在", "WARNING");
	return;
}

void tradersim::OnClientReqTransfer(ActionTransfer action_transfer)
{
	if (action_transfer.amount > 0)
	{
		m_account->deposit += action_transfer.amount;
	}
	else
	{
		m_account->withdraw -= action_transfer.amount;
	}
	m_account->static_balance += action_transfer.amount;
	m_account->changed = true;

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
	d.error_id = 0;
	d.error_msg = u8"正确";

	m_something_changed = true;
	OutputNotifyAllSycn(0, u8"转账成功");

	SendUserData();
}

#pragma endregion





