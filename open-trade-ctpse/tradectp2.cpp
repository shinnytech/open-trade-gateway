/////////////////////////////////////////////////////////////////////////
///@file tradectp2.cpp
///@brief	CTP交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "tradectp.h"
#include "ctp_define.h"
#include "SerializerTradeBase.h"
#include "config.h"
#include "utility.h"

#include <iostream>
#include <string>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

using namespace trader_dll;

traderctp::traderctp(boost::asio::io_context& ios
	,const std::string& logFileName)
	:m_b_login(false)
	,_logFileName(logFileName)
	,m_settlement_info("")
	,_ios(ios)
	,_out_mq_ptr()
	,_out_mq_name(logFileName+"_msg_out")
	,_in_mq_ptr()
	,_in_mq_name(logFileName + "_msg_in")
	,_thread_ptr()
	,m_notify_seq(0)
	,m_data_seq(0)
	,_req_login()
	,m_broker_id("")
	,m_pTdApi(NULL)		
	,m_trading_day("")
	,m_front_id(0)
	,m_session_id(0)
	,m_order_ref(0)	
	,m_input_order_key_map()
	,m_action_order_map()
	,_logIn(false)
	,_logInmutex()
	,_logInCondition()
	,m_loging_connectId(-1)
	,m_logined_connIds()
	,m_user_file_path("")
	,m_ordermap_local_remote()
	,m_ordermap_remote_local()	
	,m_data()
	,m_Algorithm_Type(THOST_FTDC_AG_None)
	,m_banks()
	,m_try_req_authenticate_times(0)
	,m_try_req_login_times(0)
{
	_requestID.store(0);

	m_req_login_dt = 0;
	m_next_qry_dt = 0;
	m_next_send_dt = 0;
	
	m_need_query_settlement.store(false);
	m_confirm_settlement_status.store(0);
	m_req_account_id.store(0);

	m_req_position_id.store(0);	
	m_rsp_position_id.store(0);

	m_rsp_account_id.store(0);
	m_need_query_bank.store(false);
	m_need_query_register.store(false);	
	m_position_ready.store(false);

	m_something_changed = false;
	m_peeking_message = false;

	m_need_save_file.store(false);

	m_need_query_broker_trading_params.store(false);
}

void traderctp::Start()
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
		Log(LOG_ERROR,"msg=Open message_queue;Erro=%s;mq_name=%s"
			, ex.what(), _out_mq_name.c_str());
	}

	try
	{

		_thread_ptr = boost::make_shared<boost::thread>(
			boost::bind(&traderctp::ReceiveMsg, this));
	}
	catch (const std::exception& ex)
	{
		Log(LOG_ERROR, "msg=trade ctpse start ReceiveMsg thread fail;errmsg=%s"
			, ex.what());
	}
}

void traderctp::ReceiveMsg()
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
			bool flag=_in_mq_ptr->timed_receive(buf, sizeof(buf),recvd_size, priority,tm);
			if (!flag)
			{
				_ios.post(boost::bind(&traderctp::OnIdle,this));				
				continue;
			}
			std::string line=buf;
			if (line.empty())
			{				
				continue;
			}			
			
			int connId = -1;
			std::string msg = "";
			int nPos = line.find_first_of('|');
			if ((nPos <= 0) || (nPos+1 >= line.length()))
			{
				Log(LOG_WARNING,"msg=traderctp ReceiveMsg,%s is invalid!"
					, line.c_str());
				continue;
			}
			else
			{
				std::string strId = line.substr(0, nPos);
				connId = atoi(strId.c_str());
				msg = line.substr(nPos + 1);
				std::shared_ptr<std::string> msg_ptr(new std::string(msg));
				_ios.post(boost::bind(&traderctp::ProcessInMsg
					, this, connId, msg_ptr));
			}
		}
		catch (const std::exception& ex)
		{
			Log(LOG_ERROR,"msg=ReceiveMsg_i;errmsg=%s", ex.what());
			break;
		}
	}	
}

void traderctp::Stop()
{
	if (nullptr != _thread_ptr)
	{
		_thread_ptr->detach();
		_thread_ptr.reset();
	}
	
	StopTdApi();
}

bool traderctp::IsConnectionLogin(int nId)
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

std::string traderctp::GetConnectionStr()
{
	std::string str = "";
	if (m_logined_connIds.empty())
	{
		return str;
	}

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
	str = ss.str();
	return str;
}

void traderctp::CloseConnection(int nId)
{
	Log(LOG_INFO,"msg=ctpse CloseConnection;instance=%p;bid=%s;UserID=%s;conn id=%d"
		, this
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

void traderctp::ProcessInMsg(int connId,std::shared_ptr<std::string> msg_ptr)
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
		Log(LOG_WARNING,"msg=ctpse parse json(%s) fail;instance=%p;bid=%s;UserID=%s;conn id=%d"
			, msg.c_str()
			, this
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, connId);
		return;
	}

	ReqLogin req;
	ss.ToVar(req);
	if (req.aid == "req_login")
	{		
		ProcessReqLogIn(connId,req);
	}
	else if (req.aid == "change_password")
	{
		if (nullptr == m_pTdApi)
		{
			Log(LOG_ERROR,"msg=trade ctpse receive change_password msg before receive login msg;instance=%p;bid=%s;UserID=%s;conn id=%d"
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			return;
		}

		if ((!m_b_login.load()) && (m_loging_connectId != connId))
		{
			Log(LOG_ERROR,"msg=trade ctpse receive change_password msg from	a diffrent connection before login suceess;instance=%p;bid=%s;UserID=%s;conn id=%d"
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			return;
		}

		m_loging_connectId = connId;

		SerializerCtp ss;
		if (!ss.FromString(msg.c_str()))
		{
			return;
		}

		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ss.m_doc));
		if (!dt || !dt->IsString())
		{
			return;
		}

		std::string aid = dt->GetString();
		if (aid == "change_password")
		{
			CThostFtdcUserPasswordUpdateField f;
			memset(&f, 0, sizeof(f));
			ss.ToVar(f);
			OnClientReqChangePassword(f);
		}
	}
	else
	{
		if (!m_b_login)
		{
			Log(LOG_WARNING,"msg=trade ctpse receive other msg before login;instance=%p;bid=%s;UserID=%s;conn id=%d"
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			return;
		}
		
		if (!IsConnectionLogin(connId))
		{
			Log(LOG_WARNING,"msg=trade ctpse receive other msg which from not login	connecion,%s;instance=%p;bid=%s;UserID=%s;conn id=%d"
				, msg.c_str()
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			return;
		}

		SerializerCtp ss;
		if (!ss.FromString(msg.c_str()))
		{
			return;
		}			

		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ss.m_doc));
		if (!dt || !dt->IsString())
		{
			return;
		}			

		std::string aid = dt->GetString();
		if (aid == "peek_message") 
		{
			OnClientPeekMessage();
		}
		else if (aid == "insert_order") 
		{
			CtpActionInsertOrder d;
			ss.ToVar(d);
			OnClientReqInsertOrder(d);
		}
		else if (aid == "cancel_order") 
		{
			CtpActionCancelOrder d;
			ss.ToVar(d);
			OnClientReqCancelOrder(d);
		}
		else if (aid == "req_transfer") 
		{
			CThostFtdcReqTransferField f;
			memset(&f, 0, sizeof(f));
			ss.ToVar(f);
			OnClientReqTransfer(f);
		}
		else if (aid == "confirm_settlement") 
		{			
			Log(LOG_INFO,"msg=trade ctpse receive confirm_settlement msg,%s;instance=%p;bid=%s;UserID=%s;conn id=%d"
				, msg.c_str()
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
				, connId);
			if (0 == m_confirm_settlement_status.load())
			{
				m_confirm_settlement_status.store(1);
			}
			ReqConfirmSettlement();
		}		
	}	
}

void traderctp::OnClientReqChangePassword(CThostFtdcUserPasswordUpdateField f)
{
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqUserPasswordUpdate(&f, 0);
	Log(LOG_INFO,"msg=ctpse ReqUserPasswordUpdate;instance=%p;bid=%s;UserID=%s;ret=%d"
		, this
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, r);
}

void traderctp::OnClientReqTransfer(CThostFtdcReqTransferField f)
{
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	strcpy_x(f.AccountID,_req_login.user_name.c_str());
	strcpy_x(f.BankBranchID, "0000");
	f.SecuPwdFlag = THOST_FTDC_BPWDF_BlankCheck;	// 核对密码
	f.BankPwdFlag = THOST_FTDC_BPWDF_NoCheck;	// 核对密码
	f.VerifyCertNoFlag = THOST_FTDC_YNI_No;
	if (f.TradeAmount >= 0) 
	{
		strcpy_x(f.TradeCode, "202001");
		int r = m_pTdApi->ReqFromBankToFutureByFuture(&f, 0);
		Log(LOG_INFO,"msg=ctpse ReqFromBankToFutureByFuture;instance=%p;bid=%s;UserID=%s;TradeAmount=%f;ret=%d"
			, this
			, _req_login.bid.c_str()
			, f.UserID
			, f.TradeAmount
			, r);
	}
	else
	{
		strcpy_x(f.TradeCode, "202002");
		f.TradeAmount = -f.TradeAmount;
		int r = m_pTdApi->ReqFromFutureToBankByFuture(&f, 0);
		Log(LOG_INFO,"msg=ctpse ReqFromFutureToBankByFuture;instance=%p;bid=%s;UserID=%s;TradeAmount=%f;ret=%d"
			, this
			, _req_login.bid.c_str()
			, f.UserID
			, f.TradeAmount
			, r);
	}
}

void traderctp::OnClientReqCancelOrder(CtpActionCancelOrder d)
{
	if (d.local_key.user_id.substr(0, _req_login.user_name.size()) != _req_login.user_name)
	{
		OutputNotifyAllSycn(1,u8"撤单user_id错误，不能撤单", "WARNING");
		return;
	}

	RemoteOrderKey rkey;
	if (!OrderIdLocalToRemote(d.local_key, &rkey)) 
	{
		OutputNotifyAllSycn(1, u8"撤单指定的order_id不存在，不能撤单", "WARNING");
		return;
	}
	strcpy_x(d.f.BrokerID, m_broker_id.c_str());
	strcpy_x(d.f.UserID, _req_login.user_name.c_str());
	strcpy_x(d.f.InvestorID, _req_login.user_name.c_str());
	strcpy_x(d.f.OrderRef, rkey.order_ref.c_str());
	strcpy_x(d.f.ExchangeID, rkey.exchange_id.c_str());
	strcpy_x(d.f.InstrumentID, rkey.instrument_id.c_str());
	d.f.SessionID = rkey.session_id;
	d.f.FrontID = rkey.front_id;
	d.f.ActionFlag = THOST_FTDC_AF_Delete;
	d.f.LimitPrice = 0;
	d.f.VolumeChange = 0;
	{		
		m_cancel_order_set.insert(d.local_key.order_id);
	}

	std::stringstream ss;
	ss << m_front_id << m_session_id << d.f.OrderRef;
	std::string strKey = ss.str();
	m_action_order_map.insert(
		std::map<std::string, std::string>::value_type(strKey, strKey));
	   	
	int r = m_pTdApi->ReqOrderAction(&d.f,0);
	Log(LOG_INFO,"msg=ctpse ReqOrderAction;instance=%p;bid=%s;UserID=%s;InstrumentID=%s;OrderRef=%s;ret=%d"
		, this
		, _req_login.bid.c_str()
		, d.f.InvestorID
		, d.f.InstrumentID
		, d.f.OrderRef
		, r);
}

void traderctp::OnClientReqInsertOrder(CtpActionInsertOrder d)
{
	if (d.local_key.user_id.substr(0,_req_login.user_name.size()) != _req_login.user_name)
	{
		OutputNotifyAllSycn(1,u8"报单user_id错误，不能下单", "WARNING");
		return;
	}

	strcpy_x(d.f.BrokerID,m_broker_id.c_str());
	strcpy_x(d.f.UserID,_req_login.user_name.c_str());
	strcpy_x(d.f.InvestorID,_req_login.user_name.c_str());
	RemoteOrderKey rkey;
	rkey.exchange_id = d.f.ExchangeID;
	rkey.instrument_id = d.f.InstrumentID;
	if (OrderIdLocalToRemote(d.local_key, &rkey)) 
	{
		OutputNotifyAllSycn(1,u8"报单单号重复，不能下单","WARNING");
		return;
	}
	
	strcpy_x(d.f.OrderRef,rkey.order_ref.c_str());
	{		
		m_insert_order_set.insert(d.f.OrderRef);
	}

	std::stringstream ss;
	ss << m_front_id << m_session_id << d.f.OrderRef;
	std::string strKey = ss.str();
	ServerOrderInfo serverOrder;
	serverOrder.InstrumentId = rkey.instrument_id;
	serverOrder.ExchangeId = rkey.exchange_id;
	serverOrder.VolumeOrigin = d.f.VolumeTotalOriginal;
	serverOrder.VolumeLeft = d.f.VolumeTotalOriginal;
	m_input_order_key_map.insert(std::map<std::string
		, ServerOrderInfo>::value_type(strKey, serverOrder));
	
	int r = m_pTdApi->ReqOrderInsert(&d.f, 0);
	Log(LOG_INFO,"msg=ctpse ReqOrderInsert;instance=%p;bid=%s;UserID=%s;InstrumentID=%s;OrderRef=%s;ret=%d;OrderPriceType=%c;Direction=%c;CombOffsetFlag=%c;LimitPrice=%f;VolumeTotalOriginal=%d;VolumeCondition=%c;TimeCondition=%c"
		, this
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, d.f.InstrumentID
		, d.f.OrderRef
		, r
		, d.f.OrderPriceType
		, d.f.Direction
		, d.f.CombHedgeFlag[0]
		, d.f.LimitPrice
		, d.f.VolumeTotalOriginal
		, d.f.VolumeCondition
		, d.f.TimeCondition);

	m_need_save_file.store(true);
}

void traderctp::OnClientPeekMessage()
{
	m_peeking_message = true;
	//向客户端发送账户信息
	SendUserData();
}

void traderctp::ProcessReqLogIn(int connId,ReqLogin& req)
{
	//如果CTP已经登录成功
	if (m_b_login.load())
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
			//加入登录客户端列表
			m_logined_connIds.push_back(connId);

			OutputNotifySycn(connId,0,u8"登录成功");
			char json_str[1024];
			sprintf(json_str, (u8"{"\
				"\"aid\": \"rtn_data\","\
				"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
				"\"user_id\" : \"%s\","\
				"\"trading_day\" : \"%s\""
				"}}}}]}")
				, _req_login.user_name.c_str()
				, _req_login.user_name.c_str()
				, m_trading_day.c_str()
			);

			std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
			_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));
						
			//发送用户数据
			SendUserDataImd(connId);
			
			//重发结算结果确认信息
			ReSendSettlementInfo(connId);
		}
		else
		{
			OutputNotifySycn(connId,0,u8"用户登录失败!");
		}
	}
	else
	{
		_req_login = req;

		Log(LOG_INFO,"msg=ctpse _req_login;instance=%p;bid=%s;UserId=%s;client_system_info=%s;client_app_id=%s"
			,this
			,_req_login.bid.c_str()
			,_req_login.user_name.c_str()
			,_req_login.client_system_info.c_str(),
			_req_login.client_app_id.c_str());

		auto it = g_config.brokers.find(_req_login.bid);
		_req_login.broker = it->second;

		//为了支持次席而添加的功能
		if ((!_req_login.broker_id.empty()) &&
			(!_req_login.front.empty()))
		{
			Log(LOG_INFO,"msg=ctpse;broker_id=%s;front=%s"
				, req.broker_id.c_str()
				, req.front.c_str());
			_req_login.broker.ctp_broker_id = _req_login.broker_id;
			_req_login.broker.trading_fronts.clear();
			_req_login.broker.trading_fronts.push_back(_req_login.front);
		}

		if (!g_config.user_file_path.empty())
		{
			m_user_file_path = g_config.user_file_path + "/" + _req_login.bid;
		}			
		m_data.user_id = _req_login.user_name;
		LoadFromFile();
		m_loging_connectId = connId;
		if (nullptr != m_pTdApi)
		{
			StopTdApi();
		}
		InitTdApi();	
		bool login = WaitLogIn();
		m_b_login.store(login);
		if (m_b_login.load())
		{
			//加入登录客户端列表
			m_logined_connIds.push_back(connId);					
			char json_str[1024];
			sprintf(json_str, (u8"{"\
				"\"aid\": \"rtn_data\","\
				"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
				"\"user_id\" : \"%s\","\
				"\"trading_day\" : \"%s\""
				"}}}}]}")
				,_req_login.user_name.c_str()
				,_req_login.user_name.c_str()
				,m_trading_day.c_str()
			);
			std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
			_ios.post(boost::bind(&traderctp::SendMsg,this,connId, msg_ptr));
		}
		else
		{
			m_loging_connectId = connId;
			OutputNotifySycn(connId,0,u8"用户登录失败!");
		}
	}	
}

bool traderctp::WaitLogIn()
{
	boost::unique_lock<boost::mutex> lock(_logInmutex);
	_logIn = false;
	m_pTdApi->Init();
	bool notify = _logInCondition.timed_wait(lock, boost::posix_time::seconds(15));
	if (!_logIn)
	{
		if (!notify)
		{
			Log(LOG_WARNING,"msg=ctpse login timeout,trading fronts is closed or trading fronts config is error;instance=%p;bid=%s;UserID=%s"
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
		}
	}
	return _logIn;
}

void traderctp::InitTdApi()
{
	m_try_req_authenticate_times = 0;
	m_try_req_login_times = 0;
	std::string flow_file_name = GenerateUniqFileName();
	m_pTdApi = CThostFtdcTraderApi::CreateFtdcTraderApi(flow_file_name.c_str());
	m_pTdApi->RegisterSpi(this);
	m_pTdApi->SubscribePrivateTopic(THOST_TERT_RESUME);
	m_pTdApi->SubscribePublicTopic(THOST_TERT_RESUME);
	m_broker_id = _req_login.broker.ctp_broker_id;

	if (_req_login.broker.is_fens)
	{
		Log(LOG_INFO
			, "msg=fens address is used;instance=%p;bid=%s;UserID=%s"
			, this
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str());

		CThostFtdcFensUserInfoField field;
		memset(&field, 0, sizeof(field));
		strcpy_x(field.BrokerID, _req_login.broker.ctp_broker_id.c_str());
		strcpy_x(field.UserID, _req_login.user_name.c_str());
		field.LoginMode = THOST_FTDC_LM_Trade;
		m_pTdApi->RegisterFensUserInfo(&field);

		for (auto it = _req_login.broker.trading_fronts.begin()
			; it != _req_login.broker.trading_fronts.end(); ++it)
		{
			std::string& f = *it;
			m_pTdApi->RegisterNameServer((char*)(f.c_str()));
		}
	}
	else
	{
		for (auto it = _req_login.broker.trading_fronts.begin()
			; it != _req_login.broker.trading_fronts.end(); ++it)
		{
			std::string& f = *it;
			m_pTdApi->RegisterFront((char*)(f.c_str()));
		}
	}	
}

void traderctp::StopTdApi()
{
	if (nullptr != m_pTdApi)
	{
		Log(LOG_INFO,"msg=ctpse OnFinish;instance=%p;bid=%s;UserId=%s"
			, this
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str());
		m_pTdApi->RegisterSpi(NULL);
		m_pTdApi->Release();
		m_pTdApi = NULL;		
	}
}


void traderctp::OutputNotifyAsych(int connId, long notify_code, const std::string& notify_msg
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
	_ios.post(boost::bind(&traderctp::SendMsg,this,connId, msg_ptr));
}

void traderctp::OutputNotifySycn(int connId, long notify_code
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
	_ios.post(boost::bind(&traderctp::SendMsg, this,connId, msg_ptr));
}

void traderctp::OutputNotifyAllAsych(long notify_code
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
	std::string str = GetConnectionStr();
	if (!str.empty())
	{
		std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
		std::shared_ptr<std::string> conn_ptr(new std::string(str));
		_ios.post(boost::bind(&traderctp::SendMsgAll,this,conn_ptr,msg_ptr));
	}	
}

void traderctp::OutputNotifyAllSycn(long notify_code
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
	std::string str = GetConnectionStr();
	if (!str.empty())
	{
		std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
		std::shared_ptr<std::string> conn_ptr(new std::string(str));
		_ios.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
	}
}

void traderctp::SendMsgAll(std::shared_ptr<std::string> conn_str_ptr,std::shared_ptr<std::string> msg_ptr)
{
	if (nullptr == msg_ptr)
	{
		return;
	}

	if (nullptr == conn_str_ptr)
	{
		return;
	}

	if (nullptr == _out_mq_ptr)
	{
		return;
	}

	std::string& msg = *msg_ptr;
	std::string& conn_str = *conn_str_ptr;
	msg = conn_str + "#" + msg;

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
			Log(LOG_ERROR,"msg=SendMsg Erro,%s;length=%d;instance=%p;bid=%s;UserId=%s"
				, ex.what()
				, msg.length()
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
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
			Log(LOG_ERROR,"msg=SendMsg Erro,%s,msg,%s;length=%d;instance=%p;bid=%s;UserId=%s"
				, ex.what()
				, msg.c_str()
				, totalLength
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
		}
	}
}

void traderctp::SendMsg(int connId,std::shared_ptr<std::string> msg_ptr)
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
			Log(LOG_ERROR,"msg=SendMsg Erro,%s;length=%d;instance=%p;bid=%s;UserId=%s"
				, ex.what()
				, msg.length()
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
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
			Log(LOG_ERROR,"msg=SendMsg Erro,%s,msg,%s;length=%d;instance=%p;bid=%s;UserId=%s"
				, ex.what()
				, msg.c_str()
				, totalLength
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str());
		}
	}
}