/////////////////////////////////////////////////////////////////////////
///@file tradersim1.cpp
///@brief	Sim交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "tradersim.h"
#include "SerializerTradeBase.h"
#include "config.h"
#include "utility.h"
#include "ins_list.h"

#include <iostream>
#include <string>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

using namespace std::chrono;

tradersim::tradersim(boost::asio::io_context& ios
	, const std::string& logFileName)
:m_b_login(false)
,_logFileName(logFileName)
,_ios(ios)
,_out_mq_ptr()
,_out_mq_name(logFileName + "_msg_out")
,_in_mq_ptr()
,_in_mq_name(logFileName + "_msg_in")
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
{
}

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
		Log(LOG_ERROR, NULL, "Open message_queue Erro:%s,mq_name:%s"
			,ex.what(),_out_mq_name.c_str());
	}

	_thread_ptr = boost::make_shared<boost::thread>(
		boost::bind(&tradersim::ReceiveMsg, this));
}

void tradersim::Stop()
{
	if (nullptr != _thread_ptr)
	{
		_thread_ptr->detach();
		_thread_ptr.reset();
	}
	
	SaveUserDataFile();
}

void tradersim::ReceiveMsg()
{
	char buf[MAX_MSG_LENTH+1];
	unsigned int priority=0;
	boost::interprocess::message_queue::size_type recvd_size=0;
	while (true)
	{
		try
		{
			memset(buf, 0, sizeof(buf));
			boost::posix_time::ptime tm = boost::get_system_time()
				+ boost::posix_time::milliseconds(100);
			bool flag = _in_mq_ptr->timed_receive(buf, sizeof(buf), recvd_size, priority, tm);
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
			if ((nPos <= 0) || (nPos+1 >= line.length()))
			{
				Log(LOG_WARNING
					, NULL
					, "tradersim ReceiveMsg:%s is invalid!"
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
			Log(LOG_ERROR, NULL, "ReceiveMsg_i Erro:%s", ex.what());
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
			char json_str[1024];
			sprintf(json_str, (u8"{"\
				"\"aid\": \"rtn_data\","\
				"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
				"\"user_id\" : \"%s\","\
				"\"trading_day\" : \"%s\""
				"}}}}]}")
				, _req_login.user_name.c_str()
				, _req_login.user_name.c_str()
				, g_config.trading_day.c_str()
			);
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
				, g_config.trading_day.c_str()
			);
			std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
			SendMsg(connId, msg_ptr);
			//加入登录客户端列表
			m_logined_connIds.push_back(connId);
		}		
		else
		{
			OutputNotifySycn(connId, 0, u8"用户登录失败!");
		}
	}
}

bool tradersim::WaitLogIn()
{
	return true;
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
	Log(LOG_INFO, NULL,"sim OnInit, UserID=%s",m_user_id.c_str());
}

void tradersim::LoadUserDataFile()
{
	if (m_user_file_path.empty())
		return;

	//加载存档文件
	std::string fn = m_user_file_path + "/" + m_user_id;

	//加载存档文件
	SerializerTradeBase nss;
	nss.FromFile(fn.c_str());
	nss.ToVar(m_data);
	//重建内存中的索引表和指针
	for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end();) 
	{
		Position& position = it->second;
		position.ins = GetInstrument(position.symbol());
		if (position.ins && !position.ins->expired)
			++it;
		else
			it = m_data.m_positions.erase(it);
	}
	/*如果不是当天的存档文件, 则需要调整
		委托单和成交记录全部清空
		用户权益转为昨权益, 平仓盈亏
		持仓手数全部移动到昨仓, 持仓均价调整到昨结算价
	*/
	if (m_data.trading_day != g_config.trading_day) 
	{
		m_data.m_orders.clear();
		m_data.m_trades.clear();
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
			item.volume_short_today = 0;
			item.volume_short_frozen_today = 0;
			item.changed = true;
		}
		m_data.trading_day = g_config.trading_day;
	}
	else 
	{
		for (auto it = m_data.m_orders.begin(); it != m_data.m_orders.end(); ++it)
		{
			m_alive_order_set.insert(&(it->second));
		}
	}
}

void tradersim::SaveUserDataFile()
{
	if (m_user_file_path.empty())
	{
		return;
	}		
	std::string fn = m_user_file_path + "/" + m_user_id;
	SerializerTradeBase nss;
	nss.dump_all = true;
	m_data.trading_day = g_config.trading_day;
	nss.FromVar(m_data);
	nss.ToFile(fn.c_str());
}

void tradersim::CloseConnection(int nId)
{
	Log(LOG_INFO, NULL, "tradersim CloseConnection,instance=%p,UserID=%s,conn id:%d"
		, this
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

void tradersim::ProcessInMsg(int connId,std::shared_ptr<std::string> msg_ptr)
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
		Log(LOG_WARNING, NULL, "tradersim parse json(%s) fail,instance=%p,UserID=%s,conn id:%d"
			, msg.c_str()
			, this
			, _req_login.user_name.c_str()
			, connId);
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
			Log(LOG_WARNING, NULL, "trade sim receive other msg before login,instance=%p,UserID=%s,conn id:%d"
				, this
				, _req_login.user_name.c_str()
				, connId);
			return;
		}

		if (!IsConnectionLogin(connId))
		{
			Log(LOG_WARNING, NULL, "trade sim receive other msg which from not login connecion:%s,instance=%p,UserID=%s,conn id:%d"
				, msg.c_str()
				, this
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
	long long now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
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
	SendMsg(connId,msg_ptr);
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
	_ios.post(boost::bind(&tradersim::SendMsgAll,this, msg_ptr));
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
			Log(LOG_ERROR, NULL, "SendMsg Erro:%s,length:%d"
				, ex.what(), msg.length());
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
			Log(LOG_ERROR, NULL, "SendMsg Erro:%s,msg:%s,length:%d"
				, ex.what(), msg.c_str(), totalLength);
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
			Log(LOG_ERROR, NULL, "SendMsg Erro:%s,length:%d"
				, ex.what(), msg.length());
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
			Log(LOG_ERROR, NULL, "SendMsg Erro:%s,msg:%s,length:%d"
				, ex.what(), msg.c_str(), totalLength);
		}
	}
}