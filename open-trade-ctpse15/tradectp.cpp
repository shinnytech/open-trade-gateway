/////////////////////////////////////////////////////////////////////////
///@file tradectp.cpp
///@brief	CTP交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "tradectp.h"
#include "utility.h"
#include "config.h"
#include "ins_list.h"
#include "numset.h"
#include "SerializerTradeBase.h"
#include "condition_order_serializer.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

traderctp::traderctp(boost::asio::io_context& ios
	, const std::string& key)
	:m_b_login(false)
	, _key(key)
	, m_settlement_info("")
	, _ios(ios)
	,_out_mq_ptr()
	,_out_mq_name(_key + "_msg_out")
	,_in_mq_ptr()
	,_in_mq_name(_key + "_msg_in")
	,_thread_ptr()	
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
	,m_req_transfer_list()
	,_logIn_status(ECTPLoginStatus::init)
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
	,m_run_receive_msg(false)
	,m_continue_process_msg(false)
	,m_continue_process_msg_mutex()
	,m_continue_process_msg_condition()
	,m_rtn_order_log_map()
	,m_rtn_trade_log_map()
	,m_err_rtn_future_to_bank_by_future_log_map()
	,m_err_rtn_bank_to_future_by_future_log_map()
	,m_rtn_from_bank_to_future_by_future_log_map()
	,m_rtn_from_future_to_bank_by_future_log_map()
	,m_err_rtn_order_insert_log_map()
	,m_err_rtn_order_action_log_map()
	,m_condition_order_data()
	,m_condition_order_his_data()
	,m_condition_order_manager(_key
	,m_condition_order_data
	,m_condition_order_his_data,*this)
	,m_condition_order_task()
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
	m_position_inited.store(false);

	m_something_changed = false;
	m_peeking_message = false;

	m_need_save_file.store(false);

	m_need_query_broker_trading_params.store(false);

	m_is_qry_his_settlement_info.store(false);
	m_his_settlement_info = "";
	m_qry_his_settlement_info_trading_days.clear();
}

#pragma region spicallback

void traderctp::ProcessOnFrontConnected()
{
	OutputNotifyAllSycn(320,u8"已经重新连接到交易前置");
	int ret = ReqAuthenticate();
	if (0 != ret)
	{
		Log().WithField("fun","ProcessOnFrontConnected")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("ret",ret)
			.Log(LOG_WARNING,"ctp ReqAuthenticate fail");		
	}
}

void traderctp::OnFrontConnected()
{
	Log().WithField("fun","OnFrontConnected")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)		
		.Log(LOG_INFO,"ctp OnFrontConnected");
	
	//还在等待登录阶段
	if (!m_b_login.load())
	{
		//这时是安全的
		OutputNotifySycn(m_loging_connectId,321,u8"已经连接到交易前置");
		int ret = ReqAuthenticate();
		if (0 != ret)
		{
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			_logIn_status = ECTPLoginStatus::reqAuthenFail;
			_logInCondition.notify_all();
		}		
	}
	else
	{
		//这时不能直接调用
		_ios.post(boost::bind(&traderctp::ProcessOnFrontConnected
			, this));
	}
}

void traderctp::ProcessOnFrontDisconnected(int nReason)
{	
	OutputNotifyAllSycn(322,u8"已经断开与交易前置的连接");

	Log().WithField("fun", "ProcessOnFrontDisconnected")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("reason", nReason)
		.WithField("co_size",(int)m_condition_order_data.condition_orders.size())
		.Log(LOG_INFO, "ctp OnFrontDisconnected");
}

void traderctp::OnFrontDisconnected(int nReason)
{
	//还在等待登录阶段
	if (!m_b_login.load())
	{		
		Log().WithField("fun", "OnFrontDisconnected")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("reason", nReason)
			.Log(LOG_INFO, "ctp OnFrontDisconnected");

		OutputNotifySycn(m_loging_connectId,322,u8"已经断开与交易前置的连接");
	}
	else
	{
		//这时不能直接调用
		_ios.post(boost::bind(&traderctp::ProcessOnFrontDisconnected
			, this, nReason));
	}
}

void traderctp::ProcessOnRspAuthenticate(std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if ((nullptr != pRspInfo) && (pRspInfo->ErrorID != 0))
	{
		//如果是未初始化
		if (7 == pRspInfo->ErrorID)
		{
			_ios.post(boost::bind(&traderctp::ReinitCtp,this));
		}
		return;
	}
	else
	{
		m_try_req_authenticate_times = 0;
		SendLoginRequest();
	}
}

void traderctp::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	
	if (nullptr != pRspAuthenticateField)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pRspAuthenticateField);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspAuthenticate")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspAuthenticate msg");
	}
	else
	{
		Log().WithField("fun","OnRspAuthenticate")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO, "ctp OnRspAuthenticate msg");
	}
	
	//还在等待登录阶段
	if (!m_b_login.load())
	{
		if ((nullptr != pRspInfo) && (pRspInfo->ErrorID != 0))
		{			
			OutputNotifySycn(m_loging_connectId
				, pRspInfo->ErrorID,
				u8"交易服务器认证失败," + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			_logIn_status = ECTPLoginStatus::rspAuthenFail;
			_logInCondition.notify_all();
			return;
		}
		else
		{			
			m_try_req_authenticate_times = 0;
			SendLoginRequest();
		}
	}
	else
	{
		std::shared_ptr<CThostFtdcRspInfoField> ptr = nullptr;
		ptr = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
		_ios.post(boost::bind(&traderctp::ProcessOnRspAuthenticate, this, ptr));
	}
}

void traderctp::ProcessOnRspUserLogin(std::shared_ptr<CThostFtdcRspUserLoginField> pRspUserLogin
	, std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	m_position_ready = false;
	m_req_login_dt.store(0);
	if (nullptr != pRspInfo && pRspInfo->ErrorID != 0)
	{
		OutputNotifyAllSycn(pRspInfo->ErrorID,
			u8"交易服务器重登录失败," + GBKToUTF8(pRspInfo->ErrorMsg),"WARNING");
		//如果是未初始化
		if (7 == pRspInfo->ErrorID)
		{
			_ios.post(boost::bind(&traderctp::ReinitCtp, this));
		}
		return;
	}
	else
	{
		m_try_req_login_times = 0;
		std::string trading_day = pRspUserLogin->TradingDay;
		if (m_trading_day != trading_day)
		{
			//一个新交易日的重新连接,需要重新初始化所有变量
			m_ordermap_local_remote.clear();
			m_ordermap_remote_local.clear();

			m_input_order_key_map.clear();
			m_action_order_map.clear();
			m_req_transfer_list.clear();
			
			m_data.m_accounts.clear();
			m_data.m_banks.clear();
			m_data.m_orders.clear();
			m_data.m_positions.clear();
			m_data.m_trades.clear();
			m_data.m_transfers.clear();
			m_data.m_trade_more_data = false;
			m_data.trading_day = trading_day;

			m_banks.clear();

			m_settlement_info = "";
						
			m_data_seq.store(0);
			_requestID.store(0);

			m_trading_day = "";
			m_front_id = 0;
			m_session_id = 0;
			m_order_ref = 0;

			m_req_login_dt = 0;
			m_next_qry_dt = 0;
			m_next_send_dt = 0;

			m_need_query_settlement.store(false);
			m_confirm_settlement_status.store(0);

			m_req_account_id.store(0);
			m_rsp_account_id.store(0);

			m_req_position_id.store(0);
			m_rsp_position_id.store(0);
			m_position_ready.store(false);
			m_position_inited.store(false);

			m_need_query_bank.store(false);
			m_need_query_register.store(false);

			m_something_changed = false;
			m_peeking_message = false;

			m_need_save_file.store(false);

			m_need_query_broker_trading_params.store(false);
			m_Algorithm_Type = THOST_FTDC_AG_None;

			m_trading_day = trading_day;
			m_front_id = pRspUserLogin->FrontID;
			m_session_id = pRspUserLogin->SessionID;
			m_order_ref = atoi(pRspUserLogin->MaxOrderRef);
			m_insert_order_set.clear();
			m_cancel_order_set.clear();

			m_is_qry_his_settlement_info.store(false);
			m_his_settlement_info = "";
			m_qry_his_settlement_info_trading_days.clear();

			m_condition_order_task.clear();

			AfterLogin();
			SetExchangeTime(*pRspUserLogin);
		}
		else
		{
			//正常的断开重连成功
			m_front_id = pRspUserLogin->FrontID;
			m_session_id = pRspUserLogin->SessionID;
			m_order_ref = atoi(pRspUserLogin->MaxOrderRef);
			m_insert_order_set.clear();
			m_cancel_order_set.clear();
			OutputNotifyAllSycn(323,u8"交易服务器重登录成功");

			m_req_position_id++;
			m_req_account_id++;
		}
	}
}

void traderctp::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin
	, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pRspUserLogin)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pRspUserLogin);
		std::string strMsg = "";
		nss.ToString(&strMsg);
		
		Log().WithField("fun","OnRspUserLogin")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspUserLogin msg");		
	}
	else
	{
		Log().WithField("fun","OnRspUserLogin")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO, "ctp OnRspUserLogin msg");
	}

	//还在等待登录阶段
	if (!m_b_login.load())
	{
		m_position_ready = false;
		m_req_login_dt.store(0);
		if (pRspInfo->ErrorID != 0)
		{
			OutputNotifySycn(m_loging_connectId
				, pRspInfo->ErrorID,
				u8"交易服务器登录失败," + GBKToUTF8(pRspInfo->ErrorMsg),"WARNING");
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			if ((pRspInfo->ErrorID == 140)
				|| (pRspInfo->ErrorID == 131)
				|| (pRspInfo->ErrorID == 141))
			{
				_logIn_status = ECTPLoginStatus::rspLoginFailNeedModifyPassword;
			}
			else
			{
				_logIn_status = ECTPLoginStatus::rspLoginFail;
			}
			_logInCondition.notify_all();
			return;
		}
		else
		{
			m_try_req_login_times = 0;
			std::string trading_day = pRspUserLogin->TradingDay;
			if (m_trading_day != trading_day)
			{
				m_ordermap_local_remote.clear();
				m_ordermap_remote_local.clear();
			}
			m_trading_day = trading_day;
			m_front_id = pRspUserLogin->FrontID;
			m_session_id = pRspUserLogin->SessionID;
			m_order_ref = atoi(pRspUserLogin->MaxOrderRef);
			m_insert_order_set.clear();
			m_cancel_order_set.clear();
			OutputNotifySycn(m_loging_connectId,324,u8"登录成功");
			AfterLogin();
			SetExchangeTime(*pRspUserLogin);
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			_logIn_status = ECTPLoginStatus::rspLoginSuccess;
			_logInCondition.notify_all();
		}
	}
	else
	{
		std::shared_ptr<CThostFtdcRspUserLoginField> ptr1 = nullptr;
		ptr1 = std::make_shared<CThostFtdcRspUserLoginField>(CThostFtdcRspUserLoginField(*pRspUserLogin));
		std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
		_ios.post(boost::bind(&traderctp::ProcessOnRspUserLogin, this, ptr1, ptr2));
	}
}

void traderctp::ProcessQrySettlementInfoConfirm(std::shared_ptr<CThostFtdcSettlementInfoConfirmField> pSettlementInfoConfirm)
{
	if ((nullptr != pSettlementInfoConfirm)
		&& (std::string(pSettlementInfoConfirm->ConfirmDate) >= m_trading_day))
	{
		//已经确认过结算单
		m_confirm_settlement_status.store(2);
		return;
	}	
	m_need_query_settlement.store(true);
	m_confirm_settlement_status.store(0);
}

void traderctp::OnRspQrySettlementInfoConfirm(
	CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pSettlementInfoConfirm)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pSettlementInfoConfirm);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQrySettlementInfoConfirm")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQrySettlementInfoConfirm msg");		
	}
	else
	{
		Log().WithField("fun","OnRspQrySettlementInfoConfirm")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQrySettlementInfoConfirm msg");		
	}

	std::shared_ptr<CThostFtdcSettlementInfoConfirmField> ptr = nullptr;
	if (nullptr != pSettlementInfoConfirm)
	{
		ptr = std::make_shared<CThostFtdcSettlementInfoConfirmField>(
			CThostFtdcSettlementInfoConfirmField(*pSettlementInfoConfirm));
	}
	_ios.post(boost::bind(&traderctp::ProcessQrySettlementInfoConfirm, this, ptr));
}

void  traderctp::ProcessQrySettlementInfo(std::shared_ptr<CThostFtdcSettlementInfoField> pSettlementInfo, bool bIsLast)
{
	if (m_is_qry_his_settlement_info.load())
	{
		if (bIsLast)
		{
			m_is_qry_his_settlement_info.store(false);
			std::string str = GBKToUTF8(pSettlementInfo->Content);
			m_his_settlement_info += str;
			NotifyClientHisSettlementInfo(m_his_settlement_info);
		}
		else
		{
			std::string str = GBKToUTF8(pSettlementInfo->Content);
			m_his_settlement_info += str;
		}
	}
	else
	{
		if (bIsLast)
		{
			m_need_query_settlement.store(false);
			std::string str = GBKToUTF8(pSettlementInfo->Content);
			m_settlement_info += str;
			if (0 == m_confirm_settlement_status.load())
			{
				Log().WithField("fun","ProcessQrySettlementInfo")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)					
					.Log(LOG_INFO, "send settlement info to client");
				OutputNotifyAllSycn(325,m_settlement_info,"INFO","SETTLEMENT");
			}
		}
		else
		{
			std::string str = GBKToUTF8(pSettlementInfo->Content);
			m_settlement_info += str;
		}
	}	
}

void traderctp::ProcessEmptySettlementInfo()
{	
	if (m_is_qry_his_settlement_info.load())
	{
		m_is_qry_his_settlement_info.store(false);
		NotifyClientHisSettlementInfo("");
	}
	else
	{
		m_need_query_settlement.store(false);
		if (0 == m_confirm_settlement_status.load())
		{
			Log().WithField("fun", "ProcessEmptySettlementInfo")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.Log(LOG_INFO, "send empty settlement info to client");
			OutputNotifyAllSycn(325,"","INFO","SETTLEMENT");
		}
	}	
}

void traderctp::NotifyClientHisSettlementInfo(const std::string& hisSettlementInfo)
{
	if (m_qry_his_settlement_info_trading_days.empty())
	{
		return;
	}
	int trading_day = m_qry_his_settlement_info_trading_days.front();
	m_qry_his_settlement_info_trading_days.pop_front();

	//构建数据包
	qry_settlement_info settle;
	settle.aid = "qry_settlement_info";
	settle.trading_day = trading_day;
	settle.user_name = _req_login.user_name;
	settle.settlement_info = hisSettlementInfo;

	SerializerTradeBase nss;
	nss.FromVar(settle);
	std::string strMsg = "";
	nss.ToString(&strMsg);
	std::string str = GetConnectionStr();
	if (!str.empty())
	{
		std::shared_ptr<std::string> msg_ptr(new std::string(strMsg));
		std::shared_ptr<std::string> conn_ptr(new std::string(str));
		_ios.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
	}	
}

void traderctp::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pSettlementInfo)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pSettlementInfo);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQrySettlementInfo")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQrySettlementInfo msg");		
	}
	else
	{
		Log().WithField("fun","OnRspQrySettlementInfo")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQrySettlementInfo msg");
	}

	if (nullptr == pSettlementInfo)
	{
		if ((nullptr == pRspInfo) && (bIsLast))
		{
			_ios.post(boost::bind(&traderctp::ProcessEmptySettlementInfo,this));
		}
		return;
	}
	else
	{
		std::shared_ptr<CThostFtdcSettlementInfoField> ptr
			= std::make_shared<CThostFtdcSettlementInfoField>
			(CThostFtdcSettlementInfoField(*pSettlementInfo));
		_ios.post(boost::bind(&traderctp::ProcessQrySettlementInfo,this,ptr,bIsLast));
	}
}

void traderctp::ProcessSettlementInfoConfirm(std::shared_ptr<CThostFtdcSettlementInfoConfirmField> pSettlementInfoConfirm
	, bool bIsLast)
{
	if (nullptr == pSettlementInfoConfirm)
	{
		return;
	}
		
	if (bIsLast)
	{
		m_confirm_settlement_status.store(2);
	}
}

void traderctp::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pSettlementInfoConfirm)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pSettlementInfoConfirm);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspSettlementInfoConfirm")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspSettlementInfoConfirm msg");		
	}
	else
	{
		Log().WithField("fun","OnRspSettlementInfoConfirm")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)		
			.Log(LOG_INFO,"ctp OnRspSettlementInfoConfirm msg");

		return;
	}

	std::shared_ptr<CThostFtdcSettlementInfoConfirmField> ptr
		= std::make_shared<CThostFtdcSettlementInfoConfirmField>
		(CThostFtdcSettlementInfoConfirmField(*pSettlementInfoConfirm));
	_ios.post(boost::bind(&traderctp::ProcessSettlementInfoConfirm, this, ptr, bIsLast));
}

void traderctp::ProcessUserPasswordUpdateField(std::shared_ptr<CThostFtdcUserPasswordUpdateField> pUserPasswordUpdate,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (nullptr==pRspInfo)
	{
		return;
	}
		
	if (pRspInfo->ErrorID == 0)
	{
		std::string strOldPassword = GBKToUTF8(pUserPasswordUpdate->OldPassword);
		std::string strNewPassword = GBKToUTF8(pUserPasswordUpdate->NewPassword);
		OutputNotifySycn(m_loging_connectId,326, u8"修改密码成功");
		if (_req_login.password == strOldPassword)
		{
			_req_login.password = strNewPassword;
			m_condition_order_manager.NotifyPasswordUpdate(strOldPassword,strNewPassword);
		}
	}
	else
	{
		OutputNotifySycn(m_loging_connectId,pRspInfo->ErrorID
			, u8"修改密码失败," + GBKToUTF8(pRspInfo->ErrorMsg)
			, "WARNING");
	}
}

void traderctp::OnRspUserPasswordUpdate(
	CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pUserPasswordUpdate)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pUserPasswordUpdate);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspUserPasswordUpdate")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspUserPasswordUpdate msg");		
	}
	else
	{
		Log().WithField("fun","OnRspUserPasswordUpdate")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspUserPasswordUpdate msg");
	}

	std::shared_ptr<CThostFtdcUserPasswordUpdateField> ptr1 = nullptr;
	if (nullptr != pUserPasswordUpdate)
	{
		ptr1 = std::make_shared<CThostFtdcUserPasswordUpdateField>(
			CThostFtdcUserPasswordUpdateField(*pUserPasswordUpdate));
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(
			CThostFtdcRspInfoField(*pRspInfo));
	}

	_ios.post(boost::bind(&traderctp::ProcessUserPasswordUpdateField
		, this, ptr1, ptr2));
}

void traderctp::ProcessRspOrderInsert(std::shared_ptr<CThostFtdcInputOrderField> pInputOrder
	, std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{	
	if (pRspInfo && pRspInfo->ErrorID != 0)
	{
		std::stringstream ss;
		ss << m_front_id << m_session_id << pInputOrder->OrderRef;
		std::string strKey = ss.str();
		auto it = m_input_order_key_map.find(strKey);
		if (it != m_input_order_key_map.end())
		{
			//找到委托单
			RemoteOrderKey remote_key;
			remote_key.exchange_id = pInputOrder->ExchangeID;
			remote_key.instrument_id = pInputOrder->InstrumentID;
			remote_key.front_id = m_front_id;
			remote_key.session_id = m_session_id;
			remote_key.order_ref = pInputOrder->OrderRef;

			LocalOrderKey local_key;
			auto it2 = m_ordermap_remote_local.find(remote_key);
			//不是OTG发的单子
			if (it2 == m_ordermap_remote_local.end())
			{
				char buf[1024];
				sprintf(buf, "SERVER.%s.%08x.%d"
					, remote_key.order_ref.c_str()
					, remote_key.session_id
					, remote_key.front_id);
				local_key.order_id = buf;
				local_key.user_id = _req_login.user_name;
				m_ordermap_local_remote[local_key] = remote_key;
				m_ordermap_remote_local[remote_key] = local_key;
				m_need_save_file.store(true);
			}
			else
			{
				local_key = it2->second;
				RemoteOrderKey& r = const_cast<RemoteOrderKey&>(it2->first);
				if (!remote_key.order_sys_id.empty())
				{
					r.order_sys_id = remote_key.order_sys_id;
				}
				m_need_save_file.store(true);
			}
			Order& order = GetOrder(local_key.order_id);
			//委托单初始属性(由下单者在下单前确定, 不再改变)
			order.seqno = 0;
			order.user_id = local_key.user_id;
			order.order_id = local_key.order_id;
			order.exchange_id = pInputOrder->ExchangeID;
			order.instrument_id = pInputOrder->InstrumentID;
			switch (pInputOrder->Direction)
			{
			case THOST_FTDC_D_Buy:
				order.direction = kDirectionBuy;
				break;
			case THOST_FTDC_D_Sell:
				order.direction = kDirectionSell;
				break;
			default:
				break;
			}
			switch (pInputOrder->CombOffsetFlag[0])
			{
			case THOST_FTDC_OF_Open:
				order.offset = kOffsetOpen;
				break;
			case THOST_FTDC_OF_CloseToday:
				order.offset = kOffsetCloseToday;
				break;
			case THOST_FTDC_OF_Close:
			case THOST_FTDC_OF_CloseYesterday:
			case THOST_FTDC_OF_ForceOff:
			case THOST_FTDC_OF_LocalForceClose:
				order.offset = kOffsetClose;
				break;
			default:
				break;
			}
			order.volume_orign = pInputOrder->VolumeTotalOriginal;
			switch (pInputOrder->OrderPriceType)
			{
			case THOST_FTDC_OPT_AnyPrice:
				order.price_type = kPriceTypeAny;
				break;
			case THOST_FTDC_OPT_LimitPrice:
				order.price_type = kPriceTypeLimit;
				break;
			case THOST_FTDC_OPT_BestPrice:
				order.price_type = kPriceTypeBest;
				break;
			case THOST_FTDC_OPT_FiveLevelPrice:
				order.price_type = kPriceTypeFiveLevel;
				break;
			default:
				break;
			}
			order.limit_price = pInputOrder->LimitPrice;
			switch (pInputOrder->TimeCondition)
			{
			case THOST_FTDC_TC_IOC:
				order.time_condition = kOrderTimeConditionIOC;
				break;
			case THOST_FTDC_TC_GFS:
				order.time_condition = kOrderTimeConditionGFS;
				break;
			case THOST_FTDC_TC_GFD:
				order.time_condition = kOrderTimeConditionGFD;
				break;
			case THOST_FTDC_TC_GTD:
				order.time_condition = kOrderTimeConditionGTD;
				break;
			case THOST_FTDC_TC_GTC:
				order.time_condition = kOrderTimeConditionGTC;
				break;
			case THOST_FTDC_TC_GFA:
				order.time_condition = kOrderTimeConditionGFA;
				break;
			default:
				break;
			}
			switch (pInputOrder->VolumeCondition)
			{
			case THOST_FTDC_VC_AV:
				order.volume_condition = kOrderVolumeConditionAny;
				break;
			case THOST_FTDC_VC_MV:
				order.volume_condition = kOrderVolumeConditionMin;
				break;
			case THOST_FTDC_VC_CV:
				order.volume_condition = kOrderVolumeConditionAll;
				break;
			default:
				break;
			}
			//委托单当前状态
			order.volume_left = pInputOrder->VolumeTotalOriginal;
			order.status = kOrderStatusFinished;
			order.last_msg = GBKToUTF8(pRspInfo->ErrorMsg);
			order.changed = true;
			m_something_changed = true;
			SendUserData();

			OutputNotifyAllSycn(pRspInfo->ErrorID
				, u8"下单失败," + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");

			m_input_order_key_map.erase(it);
		}
	}
}

void traderctp::OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder
	, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pInputOrder)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pInputOrder);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspOrderInsert")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspOrderInsert msg");		
	}
	else
	{
		Log().WithField("fun","OnRspOrderInsert")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO, "ctp OnRspOrderInsert msg");
	}

	std::shared_ptr<CThostFtdcInputOrderField> ptr1 = nullptr;
	if (nullptr != pInputOrder)
	{
		ptr1 = std::make_shared<CThostFtdcInputOrderField>(CThostFtdcInputOrderField(*pInputOrder));
	}
	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
	}
	_ios.post(boost::bind(&traderctp::ProcessRspOrderInsert, this, ptr1, ptr2));
}


void traderctp::ProcessOrderAction(std::shared_ptr<CThostFtdcInputOrderActionField> pInputOrderAction,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (pRspInfo->ErrorID != 0)
	{
		OutputNotifyAllSycn(pRspInfo->ErrorID
			, u8"撤单失败," + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
	}
}

void traderctp::OnRspOrderAction(CThostFtdcInputOrderActionField* pInputOrderAction
	, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pInputOrderAction)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pInputOrderAction);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspOrderAction")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspOrderAction msg");
	}
	else
	{
		Log().WithField("fun","OnRspOrderAction")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspOrderAction msg");
	}

	std::shared_ptr<CThostFtdcInputOrderActionField> ptr1 = nullptr;
	if (nullptr != pInputOrderAction)
	{
		ptr1 = std::make_shared<CThostFtdcInputOrderActionField>(CThostFtdcInputOrderActionField(*pInputOrderAction));
	}
	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
	}
	_ios.post(boost::bind(&traderctp::ProcessOrderAction, this, ptr1, ptr2));
}

void traderctp::ProcessErrRtnOrderInsert(std::shared_ptr<CThostFtdcInputOrderField> pInputOrder,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (pInputOrder && pRspInfo && pRspInfo->ErrorID != 0)
	{
		std::stringstream ss;
		ss << m_front_id << m_session_id << pInputOrder->OrderRef;
		std::string strKey = ss.str();
		auto it = m_input_order_key_map.find(strKey);
		if (it != m_input_order_key_map.end())
		{
			OutputNotifyAllSycn(pRspInfo->ErrorID
				, u8"下单失败," + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
			m_input_order_key_map.erase(it);

			//找到委托单
			RemoteOrderKey remote_key;
			remote_key.exchange_id = pInputOrder->ExchangeID;
			remote_key.instrument_id = pInputOrder->InstrumentID;
			remote_key.front_id = m_front_id;
			remote_key.session_id = m_session_id;
			remote_key.order_ref = pInputOrder->OrderRef;

			LocalOrderKey local_key;
			auto it2 = m_ordermap_remote_local.find(remote_key);
			//不是OTG发的单子
			if (it2 == m_ordermap_remote_local.end())
			{
				char buf[1024];
				sprintf(buf, "SERVER.%s.%08x.%d"
					, remote_key.order_ref.c_str()
					, remote_key.session_id
					, remote_key.front_id);
				local_key.order_id = buf;
				local_key.user_id = _req_login.user_name;
				m_ordermap_local_remote[local_key] = remote_key;
				m_ordermap_remote_local[remote_key] = local_key;
				m_need_save_file.store(true);
			}
			else
			{
				local_key = it2->second;
				RemoteOrderKey& r = const_cast<RemoteOrderKey&>(it2->first);
				if (!remote_key.order_sys_id.empty())
				{
					r.order_sys_id = remote_key.order_sys_id;
				}
				m_need_save_file.store(true);
			}
			
			Order& order = GetOrder(local_key.order_id);

			//委托单初始属性(由下单者在下单前确定, 不再改变)
			order.seqno = 0;
			order.user_id = local_key.user_id;
			order.order_id = local_key.order_id;
			order.exchange_id = pInputOrder->ExchangeID;
			order.instrument_id = pInputOrder->InstrumentID;

			switch (pInputOrder->Direction)
			{
			case THOST_FTDC_D_Buy:
				order.direction = kDirectionBuy;
				break;
			case THOST_FTDC_D_Sell:
				order.direction = kDirectionSell;
				break;
			default:
				break;
			}

			switch (pInputOrder->CombOffsetFlag[0])
			{
			case THOST_FTDC_OF_Open:
				order.offset = kOffsetOpen;
				break;
			case THOST_FTDC_OF_CloseToday:
				order.offset = kOffsetCloseToday;
				break;
			case THOST_FTDC_OF_Close:
			case THOST_FTDC_OF_CloseYesterday:
			case THOST_FTDC_OF_ForceOff:
			case THOST_FTDC_OF_LocalForceClose:
				order.offset = kOffsetClose;
				break;
			default:
				break;
			}

			order.volume_orign = pInputOrder->VolumeTotalOriginal;
			switch (pInputOrder->OrderPriceType)
			{
			case THOST_FTDC_OPT_AnyPrice:
				order.price_type = kPriceTypeAny;
				break;
			case THOST_FTDC_OPT_LimitPrice:
				order.price_type = kPriceTypeLimit;
				break;
			case THOST_FTDC_OPT_BestPrice:
				order.price_type = kPriceTypeBest;
				break;
			case THOST_FTDC_OPT_FiveLevelPrice:
				order.price_type = kPriceTypeFiveLevel;
				break;
			default:
				break;
			}

			order.limit_price = pInputOrder->LimitPrice;
			switch (pInputOrder->TimeCondition)
			{
			case THOST_FTDC_TC_IOC:
				order.time_condition = kOrderTimeConditionIOC;
				break;
			case THOST_FTDC_TC_GFS:
				order.time_condition = kOrderTimeConditionGFS;
				break;
			case THOST_FTDC_TC_GFD:
				order.time_condition = kOrderTimeConditionGFD;
				break;
			case THOST_FTDC_TC_GTD:
				order.time_condition = kOrderTimeConditionGTD;
				break;
			case THOST_FTDC_TC_GTC:
				order.time_condition = kOrderTimeConditionGTC;
				break;
			case THOST_FTDC_TC_GFA:
				order.time_condition = kOrderTimeConditionGFA;
				break;
			default:
				break;
			}

			switch (pInputOrder->VolumeCondition)
			{
			case THOST_FTDC_VC_AV:
				order.volume_condition = kOrderVolumeConditionAny;
				break;
			case THOST_FTDC_VC_MV:
				order.volume_condition = kOrderVolumeConditionMin;
				break;
			case THOST_FTDC_VC_CV:
				order.volume_condition = kOrderVolumeConditionAll;
				break;
			default:
				break;
			}

			//委托单当前状态
			order.volume_left = pInputOrder->VolumeTotalOriginal;
			order.status = kOrderStatusFinished;
			order.last_msg = GBKToUTF8(pRspInfo->ErrorMsg);
			order.changed = true;
			m_something_changed = true;
			SendUserData();
		}
	}
}

void traderctp::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder
	, CThostFtdcRspInfoField *pRspInfo)
{
	if (nullptr != pInputOrder)
	{
		std::stringstream ss;
		ss << pInputOrder->InstrumentID
			<< pInputOrder->OrderRef
			<< pInputOrder->OrderPriceType
			<< pInputOrder->CombOffsetFlag
			<< pInputOrder->LimitPrice
			<< pInputOrder->VolumeTotalOriginal
			<< pInputOrder->TimeCondition
			<< pInputOrder->VolumeCondition;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_err_rtn_order_insert_log_map.find(strKey);
		if (it == m_err_rtn_order_insert_log_map.end())
		{
			m_err_rtn_order_insert_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pInputOrder);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnErrRtnOrderInsert")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
				.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")			
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO,"ctp OnErrRtnOrderInsert msg");
		}
	}
	else
	{
		Log().WithField("fun","OnErrRtnOrderInsert")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")			
			.Log(LOG_INFO,"ctp OnErrRtnOrderInsert msg");
	}

	std::shared_ptr<CThostFtdcInputOrderField> ptr1 = nullptr;
	if (nullptr != pInputOrder)
	{
		ptr1 = std::make_shared<CThostFtdcInputOrderField>
			(CThostFtdcInputOrderField(*pInputOrder));
	}
	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>
			(CThostFtdcRspInfoField(*pRspInfo));
	}
	_ios.post(boost::bind(&traderctp::ProcessErrRtnOrderInsert
		, this, ptr1, ptr2));
}

void traderctp::ProcessErrRtnOrderAction(std::shared_ptr<CThostFtdcOrderActionField> pOrderAction,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (pOrderAction && pRspInfo && pRspInfo->ErrorID != 0)
	{
		std::stringstream ss;
		ss << pOrderAction->FrontID << pOrderAction->SessionID << pOrderAction->OrderRef;
		std::string strKey = ss.str();
		auto it = m_action_order_map.find(strKey);
		if (it != m_action_order_map.end())
		{
			OutputNotifyAllSycn(pRspInfo->ErrorID
				, u8"撤单失败," + GBKToUTF8(pRspInfo->ErrorMsg)
				, "WARNING");
			m_action_order_map.erase(it);
		}
	}
}

void traderctp::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction
	, CThostFtdcRspInfoField *pRspInfo)
{
	if (nullptr != pOrderAction)
	{
		std::stringstream ss;
		ss << pOrderAction->FrontID
			<< pOrderAction->SessionID
			<< pOrderAction->OrderRef
			<< pOrderAction->OrderActionStatus;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_err_rtn_order_action_log_map.find(strKey);
		if (it == m_err_rtn_order_action_log_map.end())
		{
			m_err_rtn_order_action_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pOrderAction);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnErrRtnOrderAction")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
				.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO,"ctp OnErrRtnOrderAction msg");
		}
	}
	else
	{
		Log().WithField("fun","OnErrRtnOrderAction")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")			
			.Log(LOG_INFO,"ctp OnErrRtnOrderAction msg");
	}

	std::shared_ptr<CThostFtdcOrderActionField> ptr1 = nullptr;
	if (nullptr != pOrderAction)
	{
		ptr1 = std::make_shared<CThostFtdcOrderActionField>
			(CThostFtdcOrderActionField(*pOrderAction));
	}
	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>
			(CThostFtdcRspInfoField(*pRspInfo));
	}
	_ios.post(boost::bind(&traderctp::ProcessErrRtnOrderAction, this, ptr1, ptr2));
}

void traderctp::ProcessQryInvestorPosition(
	std::shared_ptr<CThostFtdcInvestorPositionField> pRspInvestorPosition,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInvestorPosition)
	{
		std::string exchange_id = GuessExchangeId(pRspInvestorPosition->InstrumentID);
		std::string symbol = exchange_id + "." + pRspInvestorPosition->InstrumentID;
		auto ins = GetInstrument(symbol);
		if (nullptr==ins)
		{
			Log().WithField("fun","ProcessQryInvestorPosition")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("exchange_id", exchange_id)
				.WithField("instrument_id", pRspInvestorPosition->InstrumentID)
				.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
				.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
				.WithField("IsLast",bIsLast)
				.WithField("RequestID",nRequestID)							
				.Log(LOG_WARNING,"instrument is null when ProcessQryInvestorPosition");			
		}
		else
		{
			bool b_has_td_yd_distinct = (exchange_id == "SHFE") || (exchange_id == "INE");
			Position& position = GetPosition(exchange_id,pRspInvestorPosition->InstrumentID, pRspInvestorPosition->InvestorID);		
			if (pRspInvestorPosition->PosiDirection == THOST_FTDC_PD_Long)
			{
				if (!b_has_td_yd_distinct)
				{
					position.volume_long_yd = pRspInvestorPosition->YdPosition;
				}
				else
				{
					if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_History)
					{
						position.volume_long_yd = pRspInvestorPosition->YdPosition;
					}
				}				
				if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today)
				{
					position.volume_long_today = pRspInvestorPosition->Position;
					position.volume_long_frozen_today = pRspInvestorPosition->ShortFrozen;
					position.position_cost_long_today = pRspInvestorPosition->PositionCost;
					position.open_cost_long_today = pRspInvestorPosition->OpenCost;
					position.margin_long_today = pRspInvestorPosition->UseMargin;
				}
				else
				{
					position.volume_long_his = pRspInvestorPosition->Position;
					position.volume_long_frozen_his = pRspInvestorPosition->ShortFrozen;
					position.position_cost_long_his = pRspInvestorPosition->PositionCost;
					position.open_cost_long_his = pRspInvestorPosition->OpenCost;
					position.margin_long_his = pRspInvestorPosition->UseMargin;
				}
				position.position_cost_long = position.position_cost_long_today + position.position_cost_long_his;
				position.open_cost_long = position.open_cost_long_today + position.open_cost_long_his;
				position.margin_long = position.margin_long_today + position.margin_long_his;
			}
			else
			{
				if (!b_has_td_yd_distinct)
				{
					position.volume_short_yd = pRspInvestorPosition->YdPosition;
				}
				else
				{
					if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_History)
					{
						position.volume_short_yd = pRspInvestorPosition->YdPosition;
					}
				}
				if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today)
				{
					position.volume_short_today = pRspInvestorPosition->Position;
					position.volume_short_frozen_today = pRspInvestorPosition->LongFrozen;
					position.position_cost_short_today = pRspInvestorPosition->PositionCost;
					position.open_cost_short_today = pRspInvestorPosition->OpenCost;
					position.margin_short_today = pRspInvestorPosition->UseMargin;
				}
				else
				{
					position.volume_short_his = pRspInvestorPosition->Position;
					position.volume_short_frozen_his = pRspInvestorPosition->LongFrozen;
					position.position_cost_short_his = pRspInvestorPosition->PositionCost;
					position.open_cost_short_his = pRspInvestorPosition->OpenCost;
					position.margin_short_his = pRspInvestorPosition->UseMargin;
				}
				position.position_cost_short = position.position_cost_short_today + position.position_cost_short_his;
				position.open_cost_short = position.open_cost_short_today + position.open_cost_short_his;
				position.margin_short = position.margin_short_today + position.margin_short_his;
			}
			position.changed = true;
		}
	}
	if (bIsLast)
	{
		m_rsp_position_id.store(nRequestID);
		m_something_changed = true;
		m_position_ready = true;
		if(!m_position_inited)
		{
			InitPositionVolume();
			m_position_inited.store(true);
		}
		SendUserData();
	}
}

void traderctp::InitPositionVolume()
{
	for (auto it = m_data.m_positions.begin();
		it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;
		Position& position = it->second;
		position.pos_long_today = 0;
		position.pos_long_his = position.volume_long_yd;
		position.pos_short_today = 0;
		position.pos_short_his = position.volume_short_yd;
		position.changed=true;
	}
	std::map<long, Trade*> sorted_trade;
	for (auto it = m_data.m_trades.begin();
		it != m_data.m_trades.end(); ++it)
	{
		Trade& trade = it->second;
		sorted_trade[trade.seqno] = &trade;
	}
	for(auto it = sorted_trade.begin(); it!= sorted_trade.end(); ++it)
	{
		Trade& trade = *(it->second);
		AdjustPositionByTrade(trade);
	}

	for (auto it = m_data.m_positions.begin();
		it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;
		Position& position = it->second;

		Log().WithField("fun", "InitPositionVolume")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("exchange_id", position.exchange_id.c_str())
			.WithField("instrument_id", position.instrument_id.c_str())			
			.WithField("pos_short_today", position.pos_short_today)
			.WithField("pos_short_his", position.pos_short_his)
			.WithField("pos_long_today", position.pos_long_today)
			.WithField("pos_long_his", position.pos_long_his)
			.WithField("volume_long_his", position.volume_long_his)
			.WithField("volume_long_frozen_his", position.volume_long_frozen_his)
			.WithField("volume_long_today", position.volume_long_today)
			.WithField("volume_long_frozen_today", position.volume_long_frozen_today)
			.WithField("volume_short_his", position.volume_short_his)
			.WithField("volume_short_frozen_his", position.volume_short_frozen_his)
			.WithField("volume_short_today", position.volume_short_today)
			.WithField("volume_short_frozen_today", position.volume_short_frozen_today)
			.Log(LOG_INFO,"InitPositionVolume Finish");
	}
}

void traderctp::AdjustPositionByTrade(const Trade& trade)
{
	Position& pos = GetPosition(trade.exchange_id, trade.instrument_id,trade.user_id);
	
	if(trade.offset == kOffsetOpen)
	{
		if(trade.direction == kDirectionBuy)
		{
			pos.pos_long_today += trade.volume;
		}
		else
		{
			pos.pos_short_today += trade.volume;
		}
	} 
	else
	{
		if((trade.exchange_id == "SHFE" || trade.exchange_id == "INE") 
			&& trade.offset != kOffsetCloseToday)
		{
			if(trade.direction == kDirectionBuy)
			{
				pos.pos_short_his -= trade.volume;
			}
			else
			{
				pos.pos_long_his -= trade.volume;
			}
		}
		else
		{
			if(trade.direction == kDirectionBuy)
			{
				pos.pos_short_today -= trade.volume;
			}
			else
			{
				pos.pos_long_today -= trade.volume;
			}
		}
		if (pos.pos_short_today + pos.pos_short_his < 0
			||pos.pos_long_today + pos.pos_long_his < 0)
		{
			Log().WithField("fun","AdjustPositionByTrade")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("exchange_id",trade.exchange_id.c_str())
				.WithField("instrument_id",trade.instrument_id.c_str())
				.WithField("direction", trade.direction)
				.WithField("offset", trade.offset)
				.WithField("order_id", trade.order_id)
				.WithField("trade_id", trade.trade_id)
				.WithField("pos_short_today",pos.pos_short_today)
				.WithField("pos_short_his",pos.pos_short_his)
				.WithField("pos_long_today",pos.pos_long_today)
				.WithField("pos_long_his",pos.pos_long_his)
				.Log(LOG_ERROR,"ctp AdjustPositionByTrade error");			
			return;
		}
		if (pos.pos_short_today < 0)
		{
			pos.pos_short_his += pos.pos_short_today;
			pos.pos_short_today = 0;
		}
		if (pos.pos_short_his < 0)
		{
			pos.pos_short_today += pos.pos_short_his;
			pos.pos_short_his = 0;
		}
		if (pos.pos_long_today < 0)
		{
			pos.pos_long_his += pos.pos_long_today;
			pos.pos_long_today = 0;
		}
		if (pos.pos_long_his < 0)
		{
			pos.pos_long_today += pos.pos_long_his;
			pos.pos_long_his = 0;
		}
	}
	pos.changed = true;

	if (m_position_inited)
	{
		Log().WithField("fun","AdjustPositionByTrade")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("exchange_id",pos.exchange_id.c_str())
			.WithField("instrument_id",pos.instrument_id.c_str())
			.WithField("pos_short_today",pos.pos_short_today)
			.WithField("pos_short_his",pos.pos_short_his)
			.WithField("pos_long_today",pos.pos_long_today)
			.WithField("pos_long_his", pos.pos_long_his)
			.WithField("volume_long_his",pos.volume_long_his)
			.WithField("volume_long_frozen_his",pos.volume_long_frozen_his)
			.WithField("volume_long_today",pos.volume_long_today)
			.WithField("volume_long_frozen_today",pos.volume_long_frozen_today)
			.WithField("volume_short_his",pos.volume_short_his)
			.WithField("volume_short_frozen_his",pos.volume_short_frozen_his)
			.WithField("volume_short_today",pos.volume_short_today)
			.WithField("volume_short_frozen_today",pos.volume_short_frozen_today)
			.Log(LOG_INFO, "AdjustPositionByTrade Finish");
	}
}

void traderctp::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition
	, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pInvestorPosition)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pInvestorPosition);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQryInvestorPosition")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQryInvestorPosition msg");
	}
	else
	{
		Log().WithField("fun","OnRspQryInvestorPosition")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQryInvestorPosition msg");
	}

	std::shared_ptr<CThostFtdcInvestorPositionField> ptr1 = nullptr;
	if (nullptr != pInvestorPosition)
	{
		ptr1 = std::make_shared<CThostFtdcInvestorPositionField>
			(CThostFtdcInvestorPositionField(*pInvestorPosition));
	}
	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>
			(CThostFtdcRspInfoField(*pRspInfo));
	}
	_ios.post(boost::bind(&traderctp::ProcessQryInvestorPosition
		, this, ptr1, ptr2, nRequestID, bIsLast));

}

void traderctp::ProcessQryBrokerTradingParams(std::shared_ptr<CThostFtdcBrokerTradingParamsField> pBrokerTradingParams,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast)
{
	if (bIsLast)
	{
		m_need_query_broker_trading_params.store(false);
	}

	if (nullptr==pBrokerTradingParams)
	{
		return;
	}

	m_Algorithm_Type = pBrokerTradingParams->Algorithm;
}

void traderctp::OnRspQryBrokerTradingParams(CThostFtdcBrokerTradingParamsField
	*pBrokerTradingParams, CThostFtdcRspInfoField *pRspInfo
	, int nRequestID, bool bIsLast)
{
	if (nullptr != pBrokerTradingParams)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pBrokerTradingParams);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQryBrokerTradingParams")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO, "ctp OnRspQryBrokerTradingParams msg");
	}
	else
	{
		Log().WithField("fun","OnRspQryBrokerTradingParams")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQryBrokerTradingParams msg");
	}

	std::shared_ptr<CThostFtdcBrokerTradingParamsField> ptr1 = nullptr;
	if (nullptr != pBrokerTradingParams)
	{
		ptr1 = std::make_shared<CThostFtdcBrokerTradingParamsField>
			(CThostFtdcBrokerTradingParamsField(*pBrokerTradingParams));
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
	}

	_ios.post(boost::bind(&traderctp::ProcessQryBrokerTradingParams, this
		, ptr1, ptr2, nRequestID, bIsLast));
}

void traderctp::ProcessQryTradingAccount(std::shared_ptr<CThostFtdcTradingAccountField> pRspInvestorAccount,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast)
{
	if (bIsLast)
	{
		m_rsp_account_id.store(nRequestID);
	}

	if (nullptr==pRspInvestorAccount)
	{
		return;
	}

	std::string strCurrencyID= GBKToUTF8(pRspInvestorAccount->CurrencyID);

	Account& account = GetAccount(strCurrencyID);

	//账号及币种
	account.user_id = GBKToUTF8(pRspInvestorAccount->AccountID);

	account.currency = strCurrencyID;

	//本交易日开盘前状态
	account.pre_balance = pRspInvestorAccount->PreBalance;

	//本交易日内已发生事件的影响
	account.deposit = pRspInvestorAccount->Deposit;

	account.withdraw = pRspInvestorAccount->Withdraw;

	account.close_profit = pRspInvestorAccount->CloseProfit;

	account.commission = pRspInvestorAccount->Commission;

	account.premium = pRspInvestorAccount->CashIn;

	account.static_balance = pRspInvestorAccount->PreBalance
		- pRspInvestorAccount->PreCredit
		- pRspInvestorAccount->PreMortgage
		+ pRspInvestorAccount->Mortgage
		- pRspInvestorAccount->Withdraw
		+ pRspInvestorAccount->Deposit;

	//当前持仓盈亏
	account.position_profit = pRspInvestorAccount->PositionProfit;

	account.float_profit = 0;
	//当前权益
	account.balance = pRspInvestorAccount->Balance;

	//保证金占用, 冻结及风险度
	account.margin = pRspInvestorAccount->CurrMargin;

	account.frozen_margin = pRspInvestorAccount->FrozenMargin;

	account.frozen_commission = pRspInvestorAccount->FrozenCommission;

	account.frozen_premium = pRspInvestorAccount->FrozenCash;

	account.available = pRspInvestorAccount->Available;

	account.changed = true;
	if (bIsLast)
	{
		m_something_changed = true;
		SendUserData();
	}
}

void traderctp::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pRspInvestorAccount
	, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pRspInvestorAccount)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pRspInvestorAccount);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQryTradingAccount")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQryTradingAccount msg");
	}
	else
	{
		Log().WithField("fun","OnRspQryTradingAccount")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQryTradingAccount msg");
	}

	std::shared_ptr<CThostFtdcTradingAccountField> ptr1 = nullptr;
	if (nullptr != pRspInvestorAccount)
	{
		ptr1 = std::make_shared<CThostFtdcTradingAccountField>(CThostFtdcTradingAccountField(*pRspInvestorAccount));
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
	}

	_ios.post(boost::bind(&traderctp::ProcessQryTradingAccount, this
		, ptr1, ptr2, nRequestID, bIsLast));
}

void traderctp::ProcessQryContractBank(std::shared_ptr<CThostFtdcContractBankField> pContractBank,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pContractBank)
	{
		m_need_query_bank.store(false);
		return;
	}

	std::string strBankID = GBKToUTF8(pContractBank->BankID);

	Bank& bank = GetBank(strBankID);
	bank.bank_id = strBankID;
	bank.bank_name = GBKToUTF8(pContractBank->BankName);
	
	if (bIsLast)
	{
		m_need_query_bank.store(false);
	}
}

void traderctp::OnRspQryContractBank(CThostFtdcContractBankField *pContractBank
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pContractBank)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pContractBank);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQryContractBank")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQryContractBank msg");
	}
	else
	{
		Log().WithField("fun","OnRspQryContractBank")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQryContractBank msg");
	}

	std::shared_ptr<CThostFtdcContractBankField> ptr1 = nullptr;
	if (nullptr != pContractBank)
	{
		ptr1 = std::make_shared<CThostFtdcContractBankField>
			(CThostFtdcContractBankField(*pContractBank));
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>
			(CThostFtdcRspInfoField(*pRspInfo));
	}

	_ios.post(boost::bind(&traderctp::ProcessQryContractBank, this
		, ptr1, ptr2, nRequestID, bIsLast));
}

void traderctp::ProcessQryAccountregister(std::shared_ptr<CThostFtdcAccountregisterField> pAccountregister,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pAccountregister)
	{
		m_need_query_register.store(false);
		m_data.m_banks.clear();
		m_data.m_banks = m_banks;
		return;
	}

	std::string strBankID = GBKToUTF8(pAccountregister->BankID);

	Bank& bank = GetBank(strBankID);
	bank.changed = true;
	std::map<std::string, Bank>::iterator it = m_banks.find(bank.bank_id);
	if (it == m_banks.end())
	{
		m_banks.insert(std::map<std::string, Bank>::value_type(bank.bank_id, bank));
	}
	else
	{
		it->second = bank;
	}
	if (bIsLast)
	{
		m_need_query_register.store(false);
		m_data.m_banks.clear();
		m_data.m_banks = m_banks;
		m_something_changed = true;
		SendUserData();
	}
}

void traderctp::OnRspQryAccountregister(CThostFtdcAccountregisterField *pAccountregister
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pAccountregister)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pAccountregister);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQryAccountregister")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQryAccountregister msg");
	}
	else
	{
		Log().WithField("fun","OnRspQryAccountregister")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQryAccountregister msg");
	}

	std::shared_ptr<CThostFtdcAccountregisterField> ptr1 = nullptr;
	if (nullptr != pAccountregister)
	{
		ptr1 = std::make_shared<CThostFtdcAccountregisterField>(CThostFtdcAccountregisterField(*pAccountregister));
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
	}

	_ios.post(boost::bind(&traderctp::ProcessQryAccountregister, this
		, ptr1, ptr2, nRequestID, bIsLast));
}

void traderctp::ProcessQryTransferSerial(std::shared_ptr<CThostFtdcTransferSerialField> pTransferSerial,
	std::shared_ptr<CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast)
{
	if (!pTransferSerial)
	{
		return;
	}
	   
	TransferLog& d = GetTransferLog(std::to_string(pTransferSerial->PlateSerial));

	std::string strCurrencyID = GBKToUTF8(pTransferSerial->CurrencyID);

	d.currency = strCurrencyID;
	d.amount = pTransferSerial->TradeAmount;
	if (pTransferSerial->TradeCode == std::string("202002"))
		d.amount = 0 - d.amount;
	DateTime dt;
	dt.time.microsecond = 0;
	sscanf(pTransferSerial->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
	sscanf(pTransferSerial->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
	d.datetime = DateTimeToEpochNano(&dt);
	d.error_id = pTransferSerial->ErrorID;
	d.error_msg = GBKToUTF8(pTransferSerial->ErrorMsg);
	if (bIsLast)
	{
		m_something_changed = true;
		m_req_account_id++;
		SendUserData();
	}
}

void traderctp::OnRspQryTransferSerial(CThostFtdcTransferSerialField *pTransferSerial
	, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (nullptr != pTransferSerial)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pTransferSerial);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRspQryTransferSerial")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRspQryTransferSerial msg");
	}
	else
	{
		Log().WithField("fun","OnRspQryTransferSerial")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
			.WithField("IsLast",bIsLast)
			.WithField("RequestID",nRequestID)			
			.Log(LOG_INFO,"ctp OnRspQryTransferSerial msg");
	}

	std::shared_ptr<CThostFtdcTransferSerialField> ptr1 = nullptr;
	if (nullptr != pTransferSerial)
	{
		ptr1 = std::make_shared<CThostFtdcTransferSerialField>
			(CThostFtdcTransferSerialField(*pTransferSerial));
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = nullptr;
	if (nullptr != pRspInfo)
	{
		ptr2 = std::make_shared<CThostFtdcRspInfoField>
			(CThostFtdcRspInfoField(*pRspInfo));
	}

	_ios.post(boost::bind(&traderctp::ProcessQryTransferSerial, this
		, ptr1, ptr2, nRequestID, bIsLast));
}

void traderctp::ProcessFromBankToFutureByFuture(
	std::shared_ptr<CThostFtdcRspTransferField> pRspTransfer)
{
	if (!pRspTransfer)
	{
		return;
	}

	if (pRspTransfer->ErrorID == 0)
	{
		TransferLog& d = GetTransferLog(std::to_string(pRspTransfer->PlateSerial));

		std::string strCurrencyID = GBKToUTF8(pRspTransfer->CurrencyID);

		d.currency = strCurrencyID;
		d.amount = pRspTransfer->TradeAmount;
		if (pRspTransfer->TradeCode == std::string("202002"))
			d.amount = 0 - d.amount;
		DateTime dt;
		dt.time.microsecond = 0;
		sscanf(pRspTransfer->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
		sscanf(pRspTransfer->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
		d.datetime = DateTimeToEpochNano(&dt);
		d.error_id = pRspTransfer->ErrorID;
		d.error_msg = GBKToUTF8(pRspTransfer->ErrorMsg);

		if (!m_req_transfer_list.empty())
		{
			OutputNotifyAllSycn(327,u8"转账成功");
			m_req_transfer_list.pop_front();
		}

		m_something_changed = true;
		SendUserData();
		m_req_account_id++;
	}
	else
	{
		if (!m_req_transfer_list.empty())
		{
			OutputNotifyAllSycn(pRspTransfer->ErrorID
				, u8"银期错误," + GBKToUTF8(pRspTransfer->ErrorMsg)
				, "WARNING");
			m_req_transfer_list.pop_front();
		}

	}
}

void traderctp::OnRtnFromBankToFutureByFuture(
	CThostFtdcRspTransferField *pRspTransfer)
{
	if (nullptr != pRspTransfer)
	{
		std::stringstream ss;
		ss << pRspTransfer->BankSerial
			<< "_" << pRspTransfer->PlateSerial;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it =
			m_rtn_from_bank_to_future_by_future_log_map.find(strKey);
		if (it == m_rtn_from_bank_to_future_by_future_log_map.end())
		{
			m_rtn_from_bank_to_future_by_future_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pRspTransfer);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnRtnFromBankToFutureByFuture")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)				
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO,"ctp OnRtnFromBankToFutureByFuture msg");
		}
	}
	else
	{
		Log().WithField("fun","OnRtnFromBankToFutureByFuture")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO,"ctp OnRtnFromBankToFutureByFuture msg");
	}

	if (nullptr == pRspTransfer)
	{
		return;
	}
	std::shared_ptr<CThostFtdcRspTransferField> ptr1 =
		std::make_shared<CThostFtdcRspTransferField>(
			CThostFtdcRspTransferField(*pRspTransfer));
	_ios.post(boost::bind(&traderctp::ProcessFromBankToFutureByFuture, this
		, ptr1));
}

void traderctp::OnRtnFromFutureToBankByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
	if (nullptr != pRspTransfer)
	{
		std::stringstream ss;
		ss << pRspTransfer->BankSerial
			<< "_" << pRspTransfer->PlateSerial;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_rtn_from_future_to_bank_by_future_log_map.find(strKey);
		if (it == m_rtn_from_future_to_bank_by_future_log_map.end())
		{
			m_rtn_from_future_to_bank_by_future_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pRspTransfer);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnRtnFromFutureToBankByFuture")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO,"ctp OnRtnFromFutureToBankByFuture msg");
		}
	}
	else
	{
		Log().WithField("fun","OnRtnFromFutureToBankByFuture")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO, "ctp OnRtnFromFutureToBankByFuture msg");
	}

	if (nullptr == pRspTransfer)
	{
		return;
	}
	std::shared_ptr<CThostFtdcRspTransferField> ptr1 =
		std::make_shared<CThostFtdcRspTransferField>(
			CThostFtdcRspTransferField(*pRspTransfer));
	_ios.post(boost::bind(&traderctp::ProcessFromBankToFutureByFuture, this
		, ptr1));
}

void traderctp::ProcessOnErrRtnBankToFutureByFuture(
	std::shared_ptr<CThostFtdcReqTransferField> pReqTransfer
	,std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (nullptr == pReqTransfer)
	{
		return;
	}

	if (nullptr == pRspInfo)
	{
		return;
	}

	if (!m_req_transfer_list.empty())
	{
		OutputNotifyAllSycn(pRspInfo->ErrorID
			, u8"银行资金转期货错误," + GBKToUTF8(pRspInfo->ErrorMsg)
			, "WARNING");
		m_req_transfer_list.pop_front();
	}
}

void traderctp::OnErrRtnBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer
	, CThostFtdcRspInfoField *pRspInfo)
{
	if (nullptr != pReqTransfer)
	{
		std::stringstream ss;
		ss << pReqTransfer->BankSerial
			<< "_" << pReqTransfer->PlateSerial;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_err_rtn_bank_to_future_by_future_log_map.find(strKey);
		if (it == m_err_rtn_bank_to_future_by_future_log_map.end())
		{
			m_err_rtn_bank_to_future_by_future_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pReqTransfer);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnErrRtnFutureToBankByFuture")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
				.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")				
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO,"ctp OnErrRtnFutureToBankByFuture msg");
		}
	}
	else
	{
		Log().WithField("fun","OnErrRtnFutureToBankByFuture")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")			
			.Log(LOG_INFO,"ctp OnErrRtnFutureToBankByFuture msg");
	}

	if (nullptr == pRspInfo)
	{
		return;
	}
	
	if (0 == pRspInfo->ErrorID)
	{
		return;
	}

	if (nullptr == pReqTransfer)
	{
		return;
	}

	std::shared_ptr<CThostFtdcReqTransferField> ptr1 =
		std::make_shared<CThostFtdcReqTransferField>(
			CThostFtdcReqTransferField(*pReqTransfer));

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 =
		std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));

	_ios.post(boost::bind(&traderctp::ProcessOnErrRtnBankToFutureByFuture, this
		,ptr1,ptr2));
}

void traderctp::ProcessOnErrRtnFutureToBankByFuture(
	std::shared_ptr<CThostFtdcReqTransferField> pReqTransfer
	,std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (nullptr == pReqTransfer)
	{
		return;
	}
	if (nullptr == pRspInfo)
	{
		return;
	}

	if (!m_req_transfer_list.empty())
	{
		OutputNotifyAllSycn(pRspInfo->ErrorID
			,u8"期货资金转银行错误," + GBKToUTF8(pRspInfo->ErrorMsg),"WARNING");
		m_req_transfer_list.pop_front();
	}
}

void traderctp::OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer
	, CThostFtdcRspInfoField *pRspInfo)
{
	if (nullptr != pReqTransfer)
	{
		std::stringstream ss;
		ss << pReqTransfer->BankSerial
			<< "_" << pReqTransfer->PlateSerial;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_err_rtn_future_to_bank_by_future_log_map.find(strKey);
		if (it == m_err_rtn_future_to_bank_by_future_log_map.end())
		{
			m_err_rtn_future_to_bank_by_future_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pReqTransfer);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnErrRtnFutureToBankByFuture")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
				.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO,"ctp OnErrRtnFutureToBankByFuture msg");			
		}
	}
	else
	{
		Log().WithField("fun","OnErrRtnFutureToBankByFuture")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
			.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")			
			.Log(LOG_INFO, "ctp OnErrRtnFutureToBankByFuture msg");
	}

	if (nullptr == pRspInfo)
	{
		return;
	}

	if (0 == pRspInfo->ErrorID)
	{
		return;
	}

	if (nullptr == pReqTransfer)
	{
		return;
	}

	std::shared_ptr<CThostFtdcReqTransferField> ptr1 =
		std::make_shared<CThostFtdcReqTransferField>(
			CThostFtdcReqTransferField(*pReqTransfer));

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 = std::make_shared<
		CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));

	_ios.post(boost::bind(&traderctp::ProcessOnErrRtnFutureToBankByFuture,this
		, ptr1,ptr2));
}

void traderctp::ProcessRtnOrder(std::shared_ptr<CThostFtdcOrderField> pOrder)
{
	if (nullptr == pOrder)
	{
		return;
	}
	
	std::stringstream ss;
	ss << pOrder->FrontID << pOrder->SessionID << pOrder->OrderRef;
	std::string strKey = ss.str();

	RemoteOrderKey remote_key;
	remote_key.exchange_id = pOrder->ExchangeID;
	remote_key.instrument_id = pOrder->InstrumentID;
	remote_key.front_id = pOrder->FrontID;
	remote_key.session_id = pOrder->SessionID;
	remote_key.order_ref = pOrder->OrderRef;
	remote_key.order_sys_id = pOrder->OrderSysID;

	//找到委托单local_key;
	LocalOrderKey local_key;
	auto it = m_ordermap_remote_local.find(remote_key);
	//不是OTG发出的Order
	if (it == m_ordermap_remote_local.end())
	{
		char buf[1024];
		sprintf(buf, "SERVER.%s.%08x.%d"
			, remote_key.order_ref.c_str()
			, remote_key.session_id
			, remote_key.front_id);

		local_key.order_id = buf;
		local_key.user_id= _req_login.user_name;
		m_ordermap_local_remote[local_key] = remote_key;
		m_ordermap_remote_local[remote_key] = local_key;
		m_need_save_file.store(true);

		PrintNotOtgOrderLog(pOrder);
	}
	else
	{
		local_key= it->second;
		RemoteOrderKey& r = const_cast<RemoteOrderKey&>(it->first);
		if (!remote_key.order_sys_id.empty())
		{
			r.order_sys_id = remote_key.order_sys_id;
		}
		m_need_save_file.store(true);
		//不是OTG发出的Order
		if (local_key.order_id.find("SERVER.",0) != std::string::npos)
		{
			PrintNotOtgOrderLog(pOrder);
		}
		//是OTG发出的Order
		else
		{
			PrintOtgOrderLog(pOrder);
		}
	}	
		
	Order& order = GetOrder(local_key.order_id);
	//委托单初始属性(由下单者在下单前确定, 不再改变)
	order.seqno = m_data_seq++;
	order.user_id = local_key.user_id;
	order.order_id = local_key.order_id;
	order.exchange_id = pOrder->ExchangeID;
	order.instrument_id = pOrder->InstrumentID;
	auto ins = GetInstrument(order.symbol());
	if (nullptr==ins)
	{
		Log().WithField("fun","ProcessRtnOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)	
			.WithField("exchange_id", order.exchange_id)
			.WithField("instrument_id", order.instrument_id)
			.Log(LOG_ERROR,"ctp ProcessRtnOrder,instrument not exist");		
		return;
	}
	switch (pOrder->Direction)
	{
	case THOST_FTDC_D_Buy:
		order.direction = kDirectionBuy;
		break;
	case THOST_FTDC_D_Sell:
		order.direction = kDirectionSell;
		break;
	default:
		break;
	}
	switch (pOrder->CombOffsetFlag[0])
	{
	case THOST_FTDC_OF_Open:
		order.offset = kOffsetOpen;
		break;
	case THOST_FTDC_OF_CloseToday:
		order.offset = kOffsetCloseToday;
		break;
	case THOST_FTDC_OF_Close:
	case THOST_FTDC_OF_CloseYesterday:
	case THOST_FTDC_OF_ForceOff:
	case THOST_FTDC_OF_LocalForceClose:
		order.offset = kOffsetClose;
		break;
	default:
		break;
	}
	order.volume_orign = pOrder->VolumeTotalOriginal;
	switch (pOrder->OrderPriceType)
	{
	case THOST_FTDC_OPT_AnyPrice:
		order.price_type = kPriceTypeAny;
		break;
	case THOST_FTDC_OPT_LimitPrice:
		order.price_type = kPriceTypeLimit;
		break;
	case THOST_FTDC_OPT_BestPrice:
		order.price_type = kPriceTypeBest;
		break;
	case THOST_FTDC_OPT_FiveLevelPrice:
		order.price_type = kPriceTypeFiveLevel;
		break;
	default:
		break;
	}
	order.limit_price = pOrder->LimitPrice;
	switch (pOrder->TimeCondition)
	{
	case THOST_FTDC_TC_IOC:
		order.time_condition = kOrderTimeConditionIOC;
		break;
	case THOST_FTDC_TC_GFS:
		order.time_condition = kOrderTimeConditionGFS;
		break;
	case THOST_FTDC_TC_GFD:
		order.time_condition = kOrderTimeConditionGFD;
		break;
	case THOST_FTDC_TC_GTD:
		order.time_condition = kOrderTimeConditionGTD;
		break;
	case THOST_FTDC_TC_GTC:
		order.time_condition = kOrderTimeConditionGTC;
		break;
	case THOST_FTDC_TC_GFA:
		order.time_condition = kOrderTimeConditionGFA;
		break;
	default:
		break;
	}
	switch (pOrder->VolumeCondition)
	{
	case THOST_FTDC_VC_AV:
		order.volume_condition = kOrderVolumeConditionAny;
		break;
	case THOST_FTDC_VC_MV:
		order.volume_condition = kOrderVolumeConditionMin;
		break;
	case THOST_FTDC_VC_CV:
		order.volume_condition = kOrderVolumeConditionAll;
		break;
	default:
		break;
	}
	//下单后获得的信息(由期货公司返回, 不会改变)
	DateTime dt;
	dt.time.microsecond = 0;
	sscanf(pOrder->InsertDate,"%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
	sscanf(pOrder->InsertTime,"%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
	order.insert_date_time = DateTimeToEpochNano(&dt);
	order.exchange_order_id = pOrder->OrderSysID;
	//委托单当前状态
	switch (pOrder->OrderStatus)
	{
	case THOST_FTDC_OST_AllTraded:
	case THOST_FTDC_OST_PartTradedNotQueueing:
	case THOST_FTDC_OST_NoTradeNotQueueing:
	case THOST_FTDC_OST_Canceled:
		order.status = kOrderStatusFinished;
		break;
	case THOST_FTDC_OST_PartTradedQueueing:
	case THOST_FTDC_OST_NoTradeQueueing:
	case THOST_FTDC_OST_Unknown:
		order.status = kOrderStatusAlive;
		break;
	default:
		break;
	}
	order.volume_left = pOrder->VolumeTotal;
	order.last_msg = GBKToUTF8(pOrder->StatusMsg);
	order.changed = true;

	PrintOrderLogIfTouchedByConditionOrder(order);
	
	//要求重新查询持仓
	m_req_position_id++;
	m_req_account_id++;
	m_something_changed = true;
	SendUserData();
	
	//发送下单成功通知
	if (pOrder->OrderStatus != THOST_FTDC_OST_Canceled
		&& pOrder->OrderStatus != THOST_FTDC_OST_Unknown
		&& pOrder->OrderStatus != THOST_FTDC_OST_NoTradeNotQueueing
		&& pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing
		)
	{
		auto it = m_insert_order_set.find(pOrder->OrderRef);
		if (it != m_insert_order_set.end())
		{
			m_insert_order_set.erase(it);
			OutputNotifyAllSycn(328,u8"下单成功");
		}

		//更新Order Key				
		auto itOrder = m_input_order_key_map.find(strKey);
		if (itOrder != m_input_order_key_map.end())
		{
			ServerOrderInfo& serverOrderInfo = itOrder->second;
			serverOrderInfo.OrderLocalID = pOrder->OrderLocalID;
			serverOrderInfo.OrderSysID = pOrder->OrderSysID;
		}

	}

	if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled
		&& pOrder->VolumeTotal > 0)
	{
		auto it = m_cancel_order_set.find(order.order_id);
		if (it != m_cancel_order_set.end())
		{
			m_cancel_order_set.erase(it);
			OutputNotifyAllSycn(329,u8"撤单成功");
			CheckConditionOrderCancelOrderTask(order.order_id);
			//删除Order
			auto itOrder = m_input_order_key_map.find(strKey);
			if (itOrder != m_input_order_key_map.end())
			{
				m_input_order_key_map.erase(itOrder);
			}
		}
		else
		{
			auto it2 = m_insert_order_set.find(pOrder->OrderRef);
			if (it2 != m_insert_order_set.end())
			{
				m_insert_order_set.erase(it2);
				OutputNotifyAllSycn(330,u8"下单失败," + order.last_msg,"WARNING");
			}

			//删除Order
			auto itOrder = m_input_order_key_map.find(strKey);
			if (itOrder != m_input_order_key_map.end())
			{
				m_input_order_key_map.erase(itOrder);
			}
		}
	}
}

void traderctp::OnRtnOrder(CThostFtdcOrderField* pOrder)
{
	if (nullptr != pOrder)
	{
		std::stringstream ss;
		ss << pOrder->FrontID
			<<"_"<< pOrder->SessionID
			<< "_" << pOrder->OrderRef
			<< "_" << pOrder->OrderSubmitStatus
			<< "_" << pOrder->OrderStatus;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_rtn_order_log_map.find(strKey);
		if (it == m_rtn_order_log_map.end())
		{
			m_rtn_order_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));
			SerializerLogCtp nss;
			nss.FromVar(*pOrder);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnRtnOrder")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)				
				.WithPack("ctp_pack",strMsg)
				.Log(LOG_INFO, "ctp OnRtnOrder msg");		
		}
	}
	else
	{
		Log().WithField("fun","OnRtnOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO, "ctp OnRtnOrder msg");
	}

	if (nullptr == pOrder)
	{
		return;
	}
	std::shared_ptr<CThostFtdcOrderField> ptr2 =
		std::make_shared<CThostFtdcOrderField>(CThostFtdcOrderField(*pOrder));
	_ios.post(boost::bind(&traderctp::ProcessRtnOrder, this
		, ptr2));
}

void traderctp::ProcessRtnTrade(std::shared_ptr<CThostFtdcTradeField> pTrade)
{
	std::string exchangeId = pTrade->ExchangeID;
	std::string orderSysId = pTrade->OrderSysID;
	for (std::map<std::string, ServerOrderInfo>::iterator it = m_input_order_key_map.begin();
		it != m_input_order_key_map.end(); it++)
	{
		ServerOrderInfo& serverOrderInfo = it->second;
		if ((serverOrderInfo.ExchangeId == exchangeId)
			&& (serverOrderInfo.OrderSysID == orderSysId))
		{
			serverOrderInfo.VolumeLeft -= pTrade->Volume;

			std::stringstream ss;
			ss << u8"成交通知,合约:" << serverOrderInfo.ExchangeId
				<< u8"." << serverOrderInfo.InstrumentId << u8",手数:" << pTrade->Volume ;
			OutputNotifyAllSycn(331, ss.str().c_str());

			if (serverOrderInfo.VolumeLeft <= 0)
			{
				m_input_order_key_map.erase(it);
			}
			break;
		}
	}

	LocalOrderKey local_key;
	FindLocalOrderId(pTrade->ExchangeID, pTrade->OrderSysID, &local_key);
	//不是OTG发出的Order产生的成交
	if (local_key.order_id.find("SERVER.", 0) != std::string::npos)
	{
		PrintNotOtgTradeLog(pTrade);
	}
	//是OTG发出的Order产生的成交
	else
	{
		PrintOtgTradeLog(pTrade);
	}
	
	std::string trade_key = local_key.order_id + "|" + std::string(pTrade->TradeID);
	Trade& trade = GetTrade(trade_key);
	trade.seqno = m_data_seq++;
	trade.trade_id = trade_key;
	trade.user_id = local_key.user_id;
	trade.order_id = local_key.order_id;
	trade.exchange_id = pTrade->ExchangeID;
	trade.instrument_id = pTrade->InstrumentID;
	trade.exchange_trade_id = pTrade->TradeID;
	auto ins = GetInstrument(trade.symbol());
	if (nullptr==ins)
	{
		Log().WithField("fun","ProcessRtnTrade")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("exchange_id",trade.exchange_id)
			.WithField("instrument_id",trade.instrument_id)
			.Log(LOG_ERROR,"ctp OnRtnTrade,instrument not exist");		
		return;
	}

	switch (pTrade->Direction)
	{
	case THOST_FTDC_D_Buy:
		trade.direction = kDirectionBuy;
		break;
	case THOST_FTDC_D_Sell:
		trade.direction = kDirectionSell;
		break;
	default:
		break;
	}

	switch (pTrade->OffsetFlag)
	{
	case THOST_FTDC_OF_Open:
		trade.offset = kOffsetOpen;
		break;
	case THOST_FTDC_OF_CloseToday:
		trade.offset = kOffsetCloseToday;
		break;
	case THOST_FTDC_OF_Close:
	case THOST_FTDC_OF_CloseYesterday:
	case THOST_FTDC_OF_ForceOff:
	case THOST_FTDC_OF_LocalForceClose:
		trade.offset = kOffsetClose;
		break;
	default:
		break;
	}

	trade.volume = pTrade->Volume;
	trade.price = pTrade->Price;
	DateTime dt;
	dt.time.microsecond = 0;

	bool b_is_dce_or_czce = (exchangeId == "CZCE") || (exchangeId == "DCE");
	sscanf(pTrade->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
	if (b_is_dce_or_czce)
	{
		int nTime = dt.time.hour * 100 + dt.time.minute;
		//夜盘
		if ((nTime > 2030) && (nTime < 2359))
		{
			boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
			int nLocalTime = tm.time_of_day().hours() * 100 + tm.time_of_day().minutes();
			//现在还是夜盘时间
			if ((nLocalTime > 2030) && (nLocalTime <= 2359))
			{
				dt.date.year = tm.date().year();
				dt.date.month = tm.date().month();
				dt.date.day = tm.date().day();
			}
			//现在已经是白盘时间了
			else
			{
				dt.date.year = tm.date().year();
				dt.date.month = tm.date().month();
				dt.date.day = tm.date().day();
				//跳到上一个工作日
				MoveDateByWorkday(&dt.date, -1);	
			}
		}
		//白盘
		else
		{
			sscanf(pTrade->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
		}		
	}
	else
	{
		sscanf(pTrade->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
	}	
	trade.trade_date_time = DateTimeToEpochNano(&dt);
	trade.commission = 0.0;
	trade.changed = true;
	m_something_changed = true;
	if (m_position_inited)
	{
		AdjustPositionByTrade(trade);
	}
	SendUserData();

	CheckConditionOrderSendOrderTask(trade.order_id);
}

void traderctp::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
	if (nullptr != pTrade)
	{
		std::stringstream ss;
		ss << pTrade->OrderSysID
			<< pTrade->TradeID;
		std::string strKey = ss.str();
		std::map<std::string, std::string>::iterator it = m_rtn_trade_log_map.find(strKey);
		if (it == m_rtn_trade_log_map.end())
		{
			m_rtn_trade_log_map.insert(std::map<std::string, std::string>::value_type(strKey, strKey));

			SerializerLogCtp nss;
			nss.FromVar(*pTrade);
			std::string strMsg = "";
			nss.ToString(&strMsg);

			Log().WithField("fun","OnRtnTrade")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithPack("ctp_pack", strMsg)
				.Log(LOG_INFO, "ctp OnRtnTrade msg");
		}
	}
	else
	{
		Log().WithField("fun","OnRtnTrade")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO,"ctp OnRtnTrade msg");
	}

	if (nullptr == pTrade)
	{
		return;
	}
	std::shared_ptr<CThostFtdcTradeField> ptr2 =
		std::make_shared<CThostFtdcTradeField>(CThostFtdcTradeField(*pTrade));
	_ios.post(boost::bind(&traderctp::ProcessRtnTrade, this, ptr2));
}

void traderctp::PrintOtgOrderLog(std::shared_ptr<CThostFtdcOrderField> pOrder)
{
	if (nullptr != pOrder)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pOrder);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","PrintOtgOrderLog")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO, "print ogt send order log");
	}
}

void traderctp::PrintNotOtgOrderLog(std::shared_ptr<CThostFtdcOrderField> pOrder)
{
	if (nullptr != pOrder)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pOrder);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","PrintNotOtgOrderLog")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO, "print not ogt send order log");
	}
}

void traderctp::PrintOtgTradeLog(std::shared_ptr<CThostFtdcTradeField> pTrade)
{
	if (nullptr != pTrade)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pTrade);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","PrintOtgTradeLog")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"print otg trade log");
	}
}

void traderctp::PrintNotOtgTradeLog(std::shared_ptr<CThostFtdcTradeField> pTrade)
{
	if (nullptr != pTrade)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pTrade);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","PrintNotOtgTradeLog")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"print not otg trade log");
	}
}

void traderctp::ProcessOnRtnTradingNotice(std::shared_ptr<CThostFtdcTradingNoticeInfoField> pTradingNoticeInfo)
{
	if (nullptr == pTradingNoticeInfo)
	{
		return;
	}

	auto s = GBKToUTF8(pTradingNoticeInfo->FieldContent);
	if (!s.empty())
	{		
		OutputNotifyAllSycn(332,s);
	}
}

void traderctp::OnRtnTradingNotice(CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo)
{
	if (nullptr != pTradingNoticeInfo)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pTradingNoticeInfo);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRtnTradingNotice")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRtnTradingNotice msg");		
	}
	else
	{
		Log().WithField("fun","OnRtnTradingNotice")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO,"ctp OnRtnTradingNotice msg");
	}

	if (nullptr == pTradingNoticeInfo)
	{
		return;
	}
	std::shared_ptr<CThostFtdcTradingNoticeInfoField> ptr
		= std::make_shared<CThostFtdcTradingNoticeInfoField>(CThostFtdcTradingNoticeInfoField(*pTradingNoticeInfo));
	_ios.post(boost::bind(&traderctp::ProcessOnRtnTradingNotice, this
		, ptr));
}

void traderctp::OnRspError(CThostFtdcRspInfoField* pRspInfo
	, int nRequestID, bool bIsLast)
{
	Log().WithField("fun","OnRspError")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("errid",pRspInfo ? pRspInfo->ErrorID : -999)
		.WithField("errmsg",pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "")
		.WithField("IsLast",bIsLast)
		.WithField("RequestID", nRequestID)
		.Log(LOG_INFO,"ctp OnRspError msg");	
}

void traderctp::OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus)
{
	if (nullptr != pInstrumentStatus)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pInstrumentStatus);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnRtnInstrumentStatus")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"ctp OnRtnInstrumentStatus msg");
	}
	else
	{
		Log().WithField("fun","OnRtnInstrumentStatus")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO,"ctp OnRtnInstrumentStatus msg");
	}

	if (nullptr == pInstrumentStatus)
	{
		return;
	}

	std::shared_ptr<CThostFtdcInstrumentStatusField> ptr1 =
		std::make_shared<CThostFtdcInstrumentStatusField>(CThostFtdcInstrumentStatusField(*pInstrumentStatus));
	
	_ios.post(boost::bind(&traderctp::ProcessRtnInstrumentStatus,this,ptr1));
}

void traderctp::ProcessRtnInstrumentStatus(std::shared_ptr<CThostFtdcInstrumentStatusField>
	pInstrumentStatus)
{
	InstrumentTradeStatusInfo instTradeStatusInfo;
	instTradeStatusInfo.InstrumentId = pInstrumentStatus->InstrumentID;
	instTradeStatusInfo.ExchangeId = pInstrumentStatus->ExchangeID;
	//进入该状态的时间
	std::string strEnterTime = pInstrumentStatus->EnterTime;
	std::vector<std::string> hms;
	boost::algorithm::split(hms, strEnterTime, boost::algorithm::is_any_of(":"));
	if (hms.size() != 3)
	{
		return;
	}
	instTradeStatusInfo.serverEnterTime = atoi(hms[0].c_str()) * 60 * 60 + atoi(hms[1].c_str()) * 60 + atoi(hms[2].c_str());

	//收到状态改变消息的本地时间
	boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
	instTradeStatusInfo.localEnterTime = tm.time_of_day().hours() * 60 * 60 + tm.time_of_day().minutes() * 60 + tm.time_of_day().seconds();
	
	switch (pInstrumentStatus->InstrumentStatus)
	{
	case THOST_FTDC_IS_BeforeTrading:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::beforeTrading;
		break;
	case THOST_FTDC_IS_NoTrading:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::noTrading;
		break;
	case THOST_FTDC_IS_Continous:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::continousTrading;
		break;
	case THOST_FTDC_IS_AuctionOrdering:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::auctionOrdering;
		break;
	case THOST_FTDC_IS_AuctionBalance:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::auctionBalance;
		break;
	case THOST_FTDC_IS_AuctionMatch:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::auctionMatch;
		break;
	case THOST_FTDC_IS_Closed:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::closed;
	default:
		instTradeStatusInfo.instumentStatus = EInstrumentStatus::beforeTrading;
		break;
	}
	
	instTradeStatusInfo.IsDataReady = m_position_inited;

	m_condition_order_manager.OnUpdateInstrumentTradeStatus(instTradeStatusInfo);	
}

#pragma endregion


#pragma region ctp_request

int traderctp::ReqUserLogin()
{
	CThostFtdcReqUserLoginField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, _req_login.broker.ctp_broker_id.c_str());
	strcpy_x(field.UserID, _req_login.user_name.c_str());
	strcpy_x(field.Password, _req_login.password.c_str());
	strcpy_x(field.UserProductInfo, _req_login.broker.product_info.c_str());
	strcpy_x(field.LoginRemark, _req_login.client_ip.c_str());
	int ret = m_pTdApi->ReqUserLogin(&field, ++_requestID);
	if (0 != ret)
	{
		Log().WithField("fun","ReqUserLogin")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("product_info",_req_login.broker.product_info)
			.WithField("client_ip", _req_login.client_ip)			
			.WithField("ret",ret)
			.Log(LOG_WARNING,"ctp ReqUserLogin fail");	
	}	
	return ret;
}

void traderctp::SendLoginRequest()
{
	if (m_try_req_login_times > 0)
	{
		int nSeconds = 10 + m_try_req_login_times * 1;
		if (nSeconds > 60)
		{
			nSeconds = 60;
		}
		boost::this_thread::sleep_for(boost::chrono::seconds(nSeconds));
	}
	m_try_req_login_times++;
	Log().WithField("fun","SendLoginRequest")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("client_app_id",_req_login.client_app_id)
		.WithField("client_system_info_length",(int)_req_login.client_system_info.length())	
		.Log(LOG_INFO,"ctp SendLoginRequest");
	
	long long now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	m_req_login_dt.store(now);
	//提交终端信息
	if ((!_req_login.client_system_info.empty())
		&& (_req_login.bid.find("simnow",0) == std::string::npos))
	{
		int ret = RegSystemInfo();
		if (0 != ret)
		{
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			_logIn_status = ECTPLoginStatus::regSystemInfoFail;
			_logInCondition.notify_all();
			return;
		}
		else
		{
			ret = ReqUserLogin();
			if (0 != ret)
			{
				boost::unique_lock<boost::mutex> lock(_logInmutex);
				_logIn_status = ECTPLoginStatus::reqUserLoginFail;
				_logInCondition.notify_all();
				return;
			}
		}
	}
	else
	{
		int ret = ReqUserLogin();
		if (0 != ret)
		{
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			_logIn_status = ECTPLoginStatus::reqUserLoginFail;
			_logInCondition.notify_all();
			return;
		}
	}
}

void traderctp::ReinitCtp()
{
	Log().WithField("fun","ReinitCtp")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)		
		.Log(LOG_INFO,"ctp ReinitCtp start");	
	if (nullptr != m_pTdApi)
	{
		StopTdApi();
	}
	boost::this_thread::sleep_for(boost::chrono::seconds(60));
	if (m_b_login.load())
	{
		ReqLogin reqLogin;
		reqLogin.aid = "req_login";
		reqLogin.bid = _req_login.bid;
		reqLogin.user_name = _req_login.user_name;
		reqLogin.password = _req_login.password;
		ClearOldData();
		ProcessReqLogIn(0,reqLogin);
	}	
	Log().WithField("fun","ReinitCtp")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.Log(LOG_INFO,"ctp ReinitCtp end");	
}

void traderctp::ReqConfirmSettlement()
{
	CThostFtdcSettlementInfoConfirmField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqSettlementInfoConfirm(&field, 0);
	Log().WithField("fun","ReqConfirmSettlement")
		.WithField("key",_key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqConfirmSettlement");	
}

void traderctp::ReqQrySettlementInfoConfirm()
{
	CThostFtdcQrySettlementInfoConfirmField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	strcpy_x(field.AccountID, _req_login.user_name.c_str());
	strcpy_x(field.CurrencyID, "CNY");
	int r = m_pTdApi->ReqQrySettlementInfoConfirm(&field, 0);
	Log().WithField("fun","ReqQrySettlementInfoConfirm")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQrySettlementInfoConfirm");
}

int traderctp::ReqQryBrokerTradingParams()
{
	CThostFtdcQryBrokerTradingParamsField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	strcpy_x(field.CurrencyID, "CNY");	
	int r = m_pTdApi->ReqQryBrokerTradingParams(&field, 0);
	Log().WithField("fun","ReqQryBrokerTradingParams")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryBrokerTradingParams");
	return r;
}

int traderctp::ReqQryAccount(int reqid)
{
	CThostFtdcQryTradingAccountField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());	
	int r = m_pTdApi->ReqQryTradingAccount(&field, reqid);
	Log().WithField("fun","ReqQryAccount")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("RequestID",reqid)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryTradingAccount");
	return r;
}

int traderctp::ReqQryPosition(int reqid)
{
	CThostFtdcQryInvestorPositionField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryInvestorPosition(&field, reqid);
	Log().WithField("fun","ReqQryPosition")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryInvestorPosition");
	return r;
}

void traderctp::ReqQryTransferSerial()
{
	CThostFtdcQryTransferSerialField field;
	memset(&field,0,sizeof(field));
	strcpy_x(field.BrokerID,m_broker_id.c_str());
	strcpy_x(field.AccountID,_req_login.user_name.c_str());	
	int r = m_pTdApi->ReqQryTransferSerial(&field,0);
	Log().WithField("fun","ReqQryTransferSerial")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryTransferSerial");
}

void traderctp::ReqQryBank()
{
	CThostFtdcQryContractBankField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	m_pTdApi->ReqQryContractBank(&field, 0);
	int r = m_pTdApi->ReqQryContractBank(&field, 0);
	Log().WithField("fun","ReqQryBank")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryContractBank");
}

void traderctp::ReqQryAccountRegister()
{
	CThostFtdcQryAccountregisterField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	m_pTdApi->ReqQryAccountregister(&field, 0);
	int r = m_pTdApi->ReqQryAccountregister(&field, 0);
	Log().WithField("fun","ReqQryAccountRegister")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryAccountregister");
}

void traderctp::ReqQrySettlementInfo()
{
	CThostFtdcQrySettlementInfoField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	strcpy_x(field.AccountID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQrySettlementInfo(&field, 0);
	Log().WithField("fun","ReqQrySettlementInfo")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQrySettlementInfo");
}

void traderctp::ReqQryHistorySettlementInfo()
{
	if (m_qry_his_settlement_info_trading_days.empty())
	{
		return;
	}

	if (m_is_qry_his_settlement_info)
	{
		return;
	}

	std::string tradingDay = std::to_string(
		m_qry_his_settlement_info_trading_days.front());
	CThostFtdcQrySettlementInfoField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	strcpy_x(field.AccountID, _req_login.user_name.c_str());
	strcpy_x(field.TradingDay, tradingDay.c_str());
	int r = m_pTdApi->ReqQrySettlementInfo(&field, 0);
	Log().WithField("fun","ReqQryHistorySettlementInfo")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO,"ctp ReqQryHistorySettlementInfo");
	if (r == 0)
	{
		m_his_settlement_info = "";
		m_is_qry_his_settlement_info = true;
	}
}

void traderctp::ReSendSettlementInfo(int connectId)
{
	if (m_need_query_settlement.load())
	{
		return;
	}

	if (m_confirm_settlement_status.load() != 0)
	{
		return;
	}

	OutputNotifySycn(connectId,325,m_settlement_info,"INFO","SETTLEMENT");
}

#pragma endregion


#pragma region businesslogic

void traderctp::FindLocalOrderId(const std::string& exchange_id
	, const std::string& order_sys_id, LocalOrderKey* local_order_key)
{
	for (auto it = m_ordermap_remote_local.begin()
		; it != m_ordermap_remote_local.end(); ++it)
	{
		const RemoteOrderKey& rkey = it->first;
		if (rkey.order_sys_id == order_sys_id
			&& rkey.exchange_id == exchange_id)
		{
			*local_order_key = it->second;
			return;
		}
	}
}

Order& traderctp::GetOrder(const std::string order_id)
{
	return m_data.m_orders[order_id];
}

Account& traderctp::GetAccount(const std::string account_key)
{
	return m_data.m_accounts[account_key];
}

Position& traderctp::GetPosition(const std::string& exchange_id
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
		m_data.m_positions.insert(std::map<std::string, Position>::value_type(symbol,pos));
	}
	Position& position = m_data.m_positions[symbol];
	return position;
}

Bank& traderctp::GetBank(const std::string& bank_id)
{
	return m_data.m_banks[bank_id];
}

Trade& traderctp::GetTrade(const std::string trade_key)
{
	return m_data.m_trades[trade_key];
}

TransferLog& traderctp::GetTransferLog(const std::string& seq_id)
{
	return m_data.m_transfers[seq_id];
}

void traderctp::LoadFromFile()
{	
	if (m_user_file_path.empty())
	{
		return;
	}
	std::string fn = m_user_file_path + "/" + _key;
	SerializerCtp s;
	if (s.FromFile(fn.c_str()))
	{
		OrderKeyFile kf;
		s.ToVar(kf);
		for (auto it = kf.items.begin(); it != kf.items.end(); ++it)
		{
			m_ordermap_local_remote[it->local_key] = it->remote_key;
			m_ordermap_remote_local[it->remote_key] = it->local_key;
		}
		m_trading_day = kf.trading_day;
	}
}

void traderctp::SaveToFile()
{
	if (m_user_file_path.empty())
	{
		return;
	}

	SerializerCtp s;
	OrderKeyFile kf;
	kf.trading_day = m_trading_day;

	for (auto it = m_ordermap_local_remote.begin();
		it != m_ordermap_local_remote.end(); ++it)
	{
		OrderKeyPair item;
		item.local_key = it->first;
		item.remote_key = it->second;
		kf.items.push_back(item);
	}
	s.FromVar(kf);
	std::string fn = m_user_file_path + "/" + _key;
	s.ToFile(fn.c_str());
}

bool traderctp::NeedReset()
{
	if (m_req_login_dt == 0)
		return false;
	long long now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (now > m_req_login_dt + 60)
		return true;
	return false;
};

void traderctp::OnIdle()
{
	if (!m_b_login)
	{
		return;
	}
	
	if (m_need_save_file.load())
	{
		this->SaveToFile();
		m_need_save_file.store(false);
	}

	//有空的时候, 标记为需查询的项, 如果离上次查询时间够远, 应该发起查询
	long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (m_peeking_message && (m_next_send_dt < now))
	{
		m_next_send_dt = now + 100;
		SendUserData();
	}

	if (m_next_qry_dt >= now)
	{
		return;
	}

	if (m_req_position_id > m_rsp_position_id)
	{
		ReqQryPosition(m_req_position_id);
		m_next_qry_dt = now + 1100;
		return;
	}
	
	if (m_need_query_broker_trading_params)
	{
		ReqQryBrokerTradingParams();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_req_account_id > m_rsp_account_id)
	{
		ReqQryAccount(m_req_account_id);
		m_next_qry_dt = now + 1100;
		return;
	}
	
	if (m_need_query_settlement.load())
	{
		ReqQrySettlementInfo();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_confirm_settlement_status.load() == 1)
	{
		ReqConfirmSettlement();
		m_next_qry_dt = now + 1100;
		return;
	}

	CheckTimeConditionOrder();

	CheckPriceConditionOrder();
	
	if (m_need_query_bank.load())
	{
		ReqQryBank();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_need_query_register.load())
	{
		ReqQryAccountRegister();
		m_next_qry_dt = now + 1100;
		return;
	}	

	if (!m_qry_his_settlement_info_trading_days.empty())
	{
		ReqQryHistorySettlementInfo();
		m_next_qry_dt = now + 1100;
		return;
	}
}

void traderctp::SendUserData()
{
	if (!m_peeking_message)
	{
		return;
	}
		
	if (m_data.m_accounts.size() != 0)
	{
		if (m_position_ready)
		{
			//重算所有持仓项的持仓盈亏和浮动盈亏
			double total_position_profit = 0;
			double total_float_profit = 0;
			for (auto it = m_data.m_positions.begin();
				it != m_data.m_positions.end(); ++it)
			{
				const std::string& symbol = it->first;
				Position& ps = it->second;
				if (nullptr == ps.ins)
				{
					ps.ins = GetInstrument(symbol);
				}
				if (nullptr == ps.ins)
				{					
					Log().WithField("fun","SendUserData")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithField("exchange_id",ps.exchange_id)
						.WithField("instrument_id",ps.instrument_id)
						.Log(LOG_INFO, "ctp miss symbol when processing position");					
					continue;
				}
				ps.volume_long = ps.volume_long_his + ps.volume_long_today;
				ps.volume_short = ps.volume_short_his + ps.volume_short_today;
				ps.volume_long_frozen = ps.volume_long_frozen_today + ps.volume_long_frozen_his;
				ps.volume_short_frozen = ps.volume_short_frozen_today + ps.volume_short_frozen_his;
				ps.margin = ps.margin_long + ps.margin_short;

				//开盘前
				if (!IsValid(ps.ins->last_price) && !IsValid(ps.ins->settlement))
				{
					if (ps.market_status != 0)
					{
						ps.market_status = 0;
						ps.changed = true;						
					}

					double last_price = ps.ins->pre_close;
					if (!IsValid(last_price))
						last_price = ps.ins->pre_settlement;
					if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
					{
						ps.last_price = last_price;

						ps.position_profit_long = 0;
						ps.position_profit_short = 0;
						ps.position_profit = 0;

						ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
						ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
						ps.float_profit = ps.float_profit_long + ps.float_profit_short;
										
						if (ps.volume_long > 0)
						{
							ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
							ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
						}
						else
						{
							ps.open_price_long = 0;
							ps.position_price_long = 0;
						}

						if (ps.volume_short > 0)
						{
							ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
							ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
						}
						else
						{
							ps.open_price_short = 0;
							ps.position_price_short = 0;
						}

						ps.changed = true;
						m_something_changed = true;
					}
				}
				//开盘过程中
				else if (IsValid(ps.ins->last_price) && !IsValid(ps.ins->settlement))
				{
					if (ps.market_status != 1)
					{
						ps.market_status = 1;
						ps.changed = true;
					}

					double last_price = ps.ins->last_price;
					if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
					{
						ps.last_price = last_price;

						ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
						ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
						ps.position_profit = ps.position_profit_long + ps.position_profit_short;

						ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
						ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
						ps.float_profit = ps.float_profit_long + ps.float_profit_short;
												
						if (ps.volume_long > 0)
						{
							ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
							ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
						}
						else
						{
							ps.open_price_long = 0;
							ps.position_price_long = 0;
						}

						if (ps.volume_short > 0)
						{
							ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
							ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
						}
						else
						{
							ps.open_price_short = 0;
							ps.position_price_short = 0;
						}

						ps.changed = true;
						m_something_changed = true;
					}
				}
				//收盘后
				else if (IsValid(ps.ins->last_price) && IsValid(ps.ins->settlement))
				{
					boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
					int nTime = tm.time_of_day().hours() * 100 + tm.time_of_day().minutes();
					//新的交易日
					if ((nTime >= 2020)||(nTime<=820))
					{
						if (ps.market_status != 2)
						{
							ps.market_status = 2;
							ps.changed = true;
						}

						double last_price = ps.ins->last_price;
						if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
						{
							ps.last_price = last_price;

							ps.position_profit_long = 0;
							ps.position_profit_short = 0;
							ps.position_profit = 0;

							ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
							ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
							ps.float_profit = ps.float_profit_long + ps.float_profit_short;

							if (ps.volume_long > 0)
							{
								ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
								ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
							}
							else
							{
								ps.open_price_long = 0;
								ps.position_price_long = 0;
							}

							if (ps.volume_short > 0)
							{
								ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
								ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
							}
							else
							{
								ps.open_price_short = 0;
								ps.position_price_short = 0;
							}

							ps.changed = true;
							m_something_changed = true;
						}
					}
					else
					{
						if (ps.market_status != 1)
						{
							ps.market_status = 1;
							ps.changed = true;
						}
						
						double last_price = ps.ins->last_price;
						if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
						{
							ps.last_price = last_price;

							ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
							ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
							ps.position_profit = ps.position_profit_long + ps.position_profit_short;

							ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
							ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
							ps.float_profit = ps.float_profit_long + ps.float_profit_short;

							if (ps.volume_long > 0)
							{
								ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
								ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
							}
							else
							{
								ps.open_price_long = 0;
								ps.position_price_long = 0;
							}

							if (ps.volume_short > 0)
							{
								ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
								ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
							}
							else
							{
								ps.open_price_short = 0;
								ps.position_price_short = 0;
							}

							ps.changed = true;
							m_something_changed = true;
						}						
					}							
				}
							
				if (IsValid(ps.position_profit))
					total_position_profit += ps.position_profit;

				if (IsValid(ps.float_profit))
					total_float_profit += ps.float_profit;
			}

			//重算资金账户
			if (m_something_changed)
			{
				if (m_rsp_account_id >= m_req_account_id)
				{
					Account& acc = GetAccount("CNY");
					double dv = total_position_profit - acc.position_profit;
					double po_ori = 0;
					double po_curr = 0;
					double av_diff = 0;
					switch (m_Algorithm_Type)
					{
					case THOST_FTDC_AG_All:
						po_ori = acc.position_profit;
						po_curr = total_position_profit;
						break;
					case THOST_FTDC_AG_OnlyLost:
						if (acc.position_profit < 0)
						{
							po_ori = acc.position_profit;
						}
						if (total_position_profit < 0)
						{
							po_curr = total_position_profit;
						}
						break;
					case THOST_FTDC_AG_OnlyGain:
						if (acc.position_profit > 0)
						{
							po_ori = acc.position_profit;
						}
						if (total_position_profit > 0)
						{
							po_curr = total_position_profit;
						}
						break;
					case THOST_FTDC_AG_None:
						po_ori = 0;
						po_curr = 0;
						break;
					default:
						break;
					}
					av_diff = po_curr - po_ori;
					acc.position_profit = total_position_profit;
					acc.float_profit = total_float_profit;
					acc.available += av_diff;
					acc.balance += dv;
					if (IsValid(acc.margin) && IsValid(acc.balance) && !IsZero(acc.balance))
						acc.risk_ratio = acc.margin / acc.balance;
					else
						acc.risk_ratio = NAN;
					acc.changed = true;
				}				
			}

			//发送交易截面数据
			if (m_something_changed)
			{
				//构建数据包	
				m_data.m_trade_more_data = false;
				SerializerTradeBase nss;
				rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");
				rapidjson::Value node_data;
				nss.FromVar(m_data, &node_data);
				rapidjson::Value node_user_id;
				node_user_id.SetString(_req_login.user_name, nss.m_doc->GetAllocator());
				rapidjson::Value node_user;
				node_user.SetObject();
				node_user.AddMember(node_user_id, node_data, nss.m_doc->GetAllocator());
				rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc, node_user);
				std::string json_str;
				nss.ToString(&json_str);

				//发送		
				std::string str = GetConnectionStr();
				if (!str.empty())
				{
					std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
					std::shared_ptr<std::string> conn_ptr(new std::string(str));
					_ios.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
				}						
			}			
		}		

		//发送条件单数据
		SerializerConditionOrderData coss;
		rapidjson::Pointer("/aid").Set(*coss.m_doc, "rtn_condition_orders");
		rapidjson::Pointer("/user_id").Set(*coss.m_doc, m_condition_order_data.user_id);
		rapidjson::Pointer("/trading_day").Set(*coss.m_doc, m_condition_order_data.trading_day);
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
			rapidjson::Pointer("/condition_orders").Set(*coss.m_doc, co_node_data);
			std::string json_str;
			coss.ToString(&json_str);
			//发送		
			std::string str = GetConnectionStr();
			if (!str.empty())
			{
				std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
				std::shared_ptr<std::string> conn_ptr(new std::string(str));
				_ios.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
			}
		}

		if (!m_something_changed)
		{
			return;
		}

		m_something_changed = false;
		m_peeking_message = false;
	}	
}

void traderctp::SendUserDataImd(int connectId)
{
	//重算所有持仓项的持仓盈亏和浮动盈亏
	double total_position_profit = 0;
	double total_float_profit = 0;
	for (auto it = m_data.m_positions.begin();
		it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;
		Position& ps = it->second;
		if (nullptr == ps.ins)
		{
			ps.ins = GetInstrument(symbol);
		}
		if (nullptr == ps.ins)
		{
			Log().WithField("fun","SendUserDataImd")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("exchange_id",ps.exchange_id)
				.WithField("instrument_id",ps.instrument_id)
				.Log(LOG_INFO,"ctp miss symbol when processing position");			
			continue;
		}
		ps.volume_long = ps.volume_long_his + ps.volume_long_today;
		ps.volume_short = ps.volume_short_his + ps.volume_short_today;
		ps.volume_long_frozen = ps.volume_long_frozen_today + ps.volume_long_frozen_his;
		ps.volume_short_frozen = ps.volume_short_frozen_today + ps.volume_short_frozen_his;
		ps.margin = ps.margin_long + ps.margin_short;
	
		//开盘前
		if (!IsValid(ps.ins->last_price) && !IsValid(ps.ins->settlement))
		{
			if (ps.market_status != 0)
			{
				ps.market_status = 0;
				ps.changed = true;
			}

			double last_price = ps.ins->pre_close;
			if (!IsValid(last_price))
				last_price = ps.ins->pre_settlement;
			if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
			{
				ps.last_price = last_price;

				ps.position_profit_long = 0;
				ps.position_profit_short = 0;
				ps.position_profit = 0;

				ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
				ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
				ps.float_profit = ps.float_profit_long + ps.float_profit_short;
				
				if (ps.volume_long > 0)
				{
					ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
					ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
				}
				else
				{
					ps.open_price_long = 0;
					ps.position_price_long = 0;
				}

				if (ps.volume_short > 0)
				{
					ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
					ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
				}
				else
				{
					ps.open_price_short = 0;
					ps.position_price_short = 0;
				}

				ps.changed = true;
				m_something_changed = true;
			}
		}
		//开盘过程中
		else if (IsValid(ps.ins->last_price) && !IsValid(ps.ins->settlement))
		{
			if (ps.market_status != 1)
			{
				ps.market_status = 1;
				ps.changed = true;
			}

			double last_price = ps.ins->last_price;
			if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
			{
				ps.last_price = last_price;

				ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
				ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
				ps.position_profit = ps.position_profit_long + ps.position_profit_short;

				ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
				ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
				ps.float_profit = ps.float_profit_long + ps.float_profit_short;
				
				if (ps.volume_long > 0)
				{
					ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
					ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
				}
				else
				{
					ps.open_price_long = 0;
					ps.position_price_long = 0;
				}

				if (ps.volume_short > 0)
				{
					ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
					ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
				}
				else
				{
					ps.open_price_short = 0;
					ps.position_price_short = 0;
				}

				ps.changed = true;
				m_something_changed = true;
			}
		}
		//收盘后
		else if (IsValid(ps.ins->last_price) && IsValid(ps.ins->settlement))
		{
			boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
			int nTime = tm.time_of_day().hours() * 100 + tm.time_of_day().minutes();
			//新的交易日
			if ((nTime >= 2020) || (nTime <= 820))
			{
				if (ps.market_status != 2)
				{
					ps.market_status = 2;
					ps.changed = true;
				}

				double last_price = ps.ins->last_price;
				if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
				{
					ps.last_price = last_price;

					ps.position_profit_long = 0;
					ps.position_profit_short = 0;
					ps.position_profit = 0;

					ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
					ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
					ps.float_profit = ps.float_profit_long + ps.float_profit_short;

					if (ps.volume_long > 0)
					{
						ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
						ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
					}
					else
					{
						ps.open_price_long = 0;
						ps.position_price_long = 0;
					}

					if (ps.volume_short > 0)
					{
						ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
						ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
					}
					else
					{
						ps.open_price_short = 0;
						ps.position_price_short = 0;
					}

					ps.changed = true;
					m_something_changed = true;
				}
			}
			else
			{
				if (ps.market_status != 1)
				{
					ps.market_status = 1;
					ps.changed = true;
				}

				double last_price = ps.ins->last_price;
				if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
				{
					ps.last_price = last_price;

					ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
					ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
					ps.position_profit = ps.position_profit_long + ps.position_profit_short;

					ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
					ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
					ps.float_profit = ps.float_profit_long + ps.float_profit_short;

					if (ps.volume_long > 0)
					{
						ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
						ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
					}
					else
					{
						ps.open_price_long = 0;
						ps.position_price_long = 0;
					}

					if (ps.volume_short > 0)
					{
						ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
						ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
					}
					else
					{
						ps.open_price_short = 0;
						ps.position_price_short = 0;
					}

					ps.changed = true;
					m_something_changed = true;
				}
			}
		}

		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;

		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;
	}

	//重算资金账户
	if (m_something_changed)
	{
		if (m_rsp_account_id >= m_req_account_id)
		{
			Account& acc = GetAccount("CNY");
			double dv = total_position_profit - acc.position_profit;
			double po_ori = 0;
			double po_curr = 0;
			double av_diff = 0;

			switch (m_Algorithm_Type)
			{
			case THOST_FTDC_AG_All:
				po_ori = acc.position_profit;
				po_curr = total_position_profit;
				break;
			case THOST_FTDC_AG_OnlyLost:
				if (acc.position_profit < 0)
				{
					po_ori = acc.position_profit;
				}
				if (total_position_profit < 0)
				{
					po_curr = total_position_profit;
				}
				break;
			case THOST_FTDC_AG_OnlyGain:
				if (acc.position_profit > 0)
				{
					po_ori = acc.position_profit;
				}
				if (total_position_profit > 0)
				{
					po_curr = total_position_profit;
				}
				break;
			case THOST_FTDC_AG_None:
				po_ori = 0;
				po_curr = 0;
				break;
			default:
				break;
			}

			av_diff = po_curr - po_ori;
			acc.position_profit = total_position_profit;
			acc.float_profit = total_float_profit;
			acc.available += av_diff;
			acc.balance += dv;
			if (IsValid(acc.margin) && IsValid(acc.balance) && !IsZero(acc.balance))
				acc.risk_ratio = acc.margin / acc.balance;
			else
				acc.risk_ratio = NAN;
			acc.changed = true;
		}		
	}

	//构建数据包		
	SerializerTradeBase nss;
	nss.dump_all = true;
	rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");

	rapidjson::Value node_data;
	nss.FromVar(m_data, &node_data);

	rapidjson::Value node_user_id;
	node_user_id.SetString(_req_login.user_name, nss.m_doc->GetAllocator());
	
	rapidjson::Value node_user;
	node_user.SetObject();
	node_user.AddMember(node_user_id, node_data, nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc, node_user);
		
	std::string json_str;
	nss.ToString(&json_str);

	//发送	
	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	_ios.post(boost::bind(&traderctp::SendMsg, this, connectId, msg_ptr));

	//发送条件单数据
	SerializerConditionOrderData coss;
	rapidjson::Pointer("/aid").Set(*coss.m_doc, "rtn_condition_orders");
	rapidjson::Pointer("/user_id").Set(*coss.m_doc, m_condition_order_data.user_id);
	rapidjson::Pointer("/trading_day").Set(*coss.m_doc, m_condition_order_data.trading_day);	
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
	rapidjson::Pointer("/condition_orders").Set(*coss.m_doc,co_node_data);
	coss.ToString(&json_str);
	std::shared_ptr<std::string> msg_ptr2(new std::string(json_str));
	_ios.post(boost::bind(&traderctp::SendMsg, this, connectId,msg_ptr2));
}

void traderctp::AfterLogin()
{
	if (g_config.auto_confirm_settlement)
	{
		if (0 == m_confirm_settlement_status.load())
		{
			m_confirm_settlement_status.store(1);
		}
		ReqConfirmSettlement();
	}
	else if (m_settlement_info.empty())
	{
		ReqQrySettlementInfoConfirm();
	}
	m_req_position_id++;
	m_req_account_id++;
	m_need_query_bank.store(true);
	m_need_query_register.store(true);
	m_need_query_broker_trading_params.store(true);
}

#pragma endregion

#pragma region systemlogic

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
		Log().WithField("fun","Start")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"open message queue exception");
	}

	try
	{

		m_run_receive_msg.store(true);
		_thread_ptr = boost::make_shared<boost::thread>(
			boost::bind(&traderctp::ReceiveMsg,this,_key));
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun","Start")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"trade ctp start ReceiveMsg thread fail");		
	}
}

void traderctp::ReceiveMsg(const std::string& key)
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
				_ios.post(boost::bind(&traderctp::OnIdle, this));
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
					.WithField("key",strKey)
					.WithField("msgcontent",line)
					.Log(LOG_ERROR,"traderctp ReceiveMsg is invalid!");				
				continue;
			}
			else
			{
				std::string strId = line.substr(0, nPos);
				connId = atoi(strId.c_str());
				msg = line.substr(nPos + 1);
				std::shared_ptr<std::string> msg_ptr=std::make_shared<std::string>(msg);
				do
				{
					boost::unique_lock<boost::mutex> lock(m_continue_process_msg_mutex);
					m_continue_process_msg.store(false);
					_ios.post(boost::bind(&traderctp::ProcessInMsg
						, this, connId, msg_ptr));
					bool notify = m_continue_process_msg_condition.timed_wait(lock,boost::posix_time::milliseconds(10));
					if (!notify)
					{
						break;
					}					
					if (m_continue_process_msg.load()
						&& m_run_receive_msg.load())
					{						
						boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
						continue;
					}
					else
					{
						break;
					}
				} while (true);				
			}
		}
		catch (const std::exception& ex)
		{
			Log().WithField("fun","ReceiveMsg")
				.WithField("key",strKey)
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"traderctp ReceiveMsg exception!");		
			break;
		}
	}
}

void traderctp::NotifyContinueProcessMsg(bool flag)
{
	boost::unique_lock<boost::mutex> lock(m_continue_process_msg_mutex);
	m_continue_process_msg.store(flag);
	m_continue_process_msg_condition.notify_all();
}

void traderctp::ProcessInMsg(int connId, std::shared_ptr<std::string> msg_ptr)
{
	if (nullptr == msg_ptr)
	{
		NotifyContinueProcessMsg(false);
		return;
	}
	std::string& msg = *msg_ptr;
	//一个特殊的消息
	if (msg == CLOSE_CONNECTION_MSG)
	{
		CloseConnection(connId);
		NotifyContinueProcessMsg(false);
		return;
	}

	SerializerTradeBase ss;
	if (!ss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "ProcessInMsg")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("msgcontent", msg)
			.WithField("connId", connId)
			.Log(LOG_WARNING, "traderctp parse json fail");
		NotifyContinueProcessMsg(false);
		return;
	}

	ReqLogin req;
	ss.ToVar(req);
	if (req.aid == "req_login")
	{
		ProcessReqLogIn(connId, req);
		NotifyContinueProcessMsg(false);
		return;
	}
	else if (req.aid == "change_password")
	{
		if (nullptr == m_pTdApi)
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_ERROR, "trade ctp receive change_password msg before receive login msg");

			NotifyContinueProcessMsg(false);
			return;
		}

		if ((!m_b_login.load()) && (m_loging_connectId != connId))
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_ERROR, "trade ctp receive change_password msg from a diffrent connection before login suceess");

			NotifyContinueProcessMsg(false);
			return;
		}

		m_loging_connectId = connId;

		SerializerCtp ssCtp;
		if (!ssCtp.FromString(msg.c_str()))
		{
			NotifyContinueProcessMsg(false);
			return;
		}

		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ssCtp.m_doc));
		if (!dt || !dt->IsString())
		{
			NotifyContinueProcessMsg(false);
			return;
		}

		std::string aid = dt->GetString();
		if (aid == "change_password")
		{
			CThostFtdcUserPasswordUpdateField f;
			memset(&f, 0, sizeof(f));
			ssCtp.ToVar(f);
			OnClientReqChangePassword(f);
		}

		NotifyContinueProcessMsg(false);
		return;
	}
	else
	{
		if (!m_b_login)
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_ERROR, "trade ctp receive other msg before login");

			NotifyContinueProcessMsg(false);
			return;
		}

		if (!IsConnectionLogin(connId)
			&& (connId != 0))
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_WARNING, "trade ctp receive other msg which from not login connecion");

			NotifyContinueProcessMsg(false);
			return;
		}

		SerializerCtp ssCtp;
		if (!ssCtp.FromString(msg.c_str()))
		{
			NotifyContinueProcessMsg(false);
			return;
		}

		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ssCtp.m_doc));
		if (!dt || !dt->IsString())
		{
			NotifyContinueProcessMsg(false);
			return;
		}

		std::string aid = dt->GetString();
		if (aid == "peek_message")
		{
			OnClientPeekMessage();
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "req_reconnect_trade")
		{
			if (connId != 0)
			{
				NotifyContinueProcessMsg(false);
				return;
			}

			SerializerConditionOrderData ns;
			if (!ns.FromString(msg.c_str()))
			{
				NotifyContinueProcessMsg(false);
				return;
			}
			req_reconnect_trade_instance req_reconnect_trade;
			ns.ToVar(req_reconnect_trade);
			for (auto id : req_reconnect_trade.connIds)
			{
				m_logined_connIds.push_back(id);
			}
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "insert_order")
		{
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(333, u8"当前时间不支持下单!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}
			CtpActionInsertOrder d;
			ssCtp.ToVar(d);
			int ret=OnClientReqInsertOrder(d);
			if ((-2 == ret) || (-3 == ret))
			{
				NotifyContinueProcessMsg(true);
				return;
			}
			else
			{
				NotifyContinueProcessMsg(false);
				return;
			}			
		}
		else if (aid == "cancel_order")
		{
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(334, u8"当前时间不支持撤单!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}
			CtpActionCancelOrder d;
			ssCtp.ToVar(d);
			OnClientReqCancelOrder(d);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "req_transfer")
		{
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(335, u8"当前时间不支持转账!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}
			CThostFtdcReqTransferField f;
			memset(&f, 0, sizeof(f));
			ssCtp.ToVar(f);
			OnClientReqTransfer(f);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "confirm_settlement")
		{
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(336, u8"当前时间不支持确认结算单!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}

			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.Log(LOG_INFO, "receive confirm settlement info request");

			if (0 == m_confirm_settlement_status.load())
			{
				m_confirm_settlement_status.store(1);
			}
			ReqConfirmSettlement();
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "qry_settlement_info")
		{
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(337, u8"当前时间不支持查询历史结算单!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}

			qry_settlement_info qrySettlementInfo;
			ss.ToVar(qrySettlementInfo);
			OnClientReqQrySettlementInfo(qrySettlementInfo);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "qry_transfer_serial")
		{
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(359, u8"当前时间不支持查询转账记录!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}
			ReqQryTransferSerial();
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "qry_account_info")
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_INFO, "trade ctp receive qry_account_info msg");

			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(360, u8"当前时间不支持查询资金账号!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}
			m_req_account_id++;
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "qry_account_register")
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_INFO, "trade ctp receive qry_account_register msg");
			
			if (nullptr == m_pTdApi)
			{
				OutputNotifyAllSycn(361, u8"当前时间不支持查询银期签约关系!", "WARNING");
				NotifyContinueProcessMsg(false);
				return;
			}

			m_data.m_banks.clear();
			m_banks.clear();
			m_need_query_bank.store(true);
			m_need_query_register.store(true);	
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "insert_condition_order")
		{
			m_condition_order_manager.InsertConditionOrder(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "cancel_condition_order")
		{
			m_condition_order_manager.CancelConditionOrder(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "pause_condition_order")
		{
			m_condition_order_manager.PauseConditionOrder(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "resume_condition_order")
		{
			m_condition_order_manager.ResumeConditionOrder(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "qry_his_condition_order")
		{
			m_condition_order_manager.QryHisConditionOrder(connId, msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "req_ccos_status")
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_INFO, "trade ctp receive ccos msg");
			if (connId != 0)
			{
				NotifyContinueProcessMsg(false);
				return;
			}
			m_condition_order_manager.ChangeCOSStatus(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "req_start_ctp")
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_INFO, "trade ctp receive req_start_ctp msg");
			if (connId != 0)
			{
				NotifyContinueProcessMsg(false);
				return;
			}
			OnReqStartCTP(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
		else if (aid == "req_stop_ctp")
		{
			Log().WithField("fun", "ProcessInMsg")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("connId", connId)
				.Log(LOG_INFO, "trade ctp receive req_stop_ctp msg");
			if (connId != 0)
			{
				NotifyContinueProcessMsg(false);
				return;
			}
			OnReqStopCTP(msg);
			NotifyContinueProcessMsg(false);
			return;
		}
	}
}

void traderctp::Stop()
{
	if (nullptr != _thread_ptr)
	{
		m_run_receive_msg.store(false);
		_thread_ptr->join();
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
	Log().WithField("fun","CloseConnection")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("connId",nId)
		.Log(LOG_INFO,"traderctp CloseConnection");	

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

void traderctp::ClearOldData()
{
	if (m_need_save_file.load())
	{
		SaveToFile();
	}

	StopTdApi();
	m_b_login.store(false);
	_logIn_status = ECTPLoginStatus::init;
	m_try_req_authenticate_times = 0;
	m_try_req_login_times = 0;
	m_ordermap_local_remote.clear();
	m_ordermap_remote_local.clear();

	m_data.m_accounts.clear();
	m_data.m_banks.clear();
	m_data.m_orders.clear();
	m_data.m_positions.clear();
	m_data.m_trades.clear();
	m_data.m_transfers.clear();
	m_data.m_trade_more_data = false;

	m_banks.clear();

	m_settlement_info = "";

	m_data_seq.store(0);
	_requestID.store(0);

	m_trading_day = "";
	m_front_id = 0;
	m_session_id = 0;
	m_order_ref = 0;

	m_req_login_dt = 0;
	m_next_qry_dt = 0;
	m_next_send_dt = 0;

	m_need_query_settlement.store(false);
	m_confirm_settlement_status.store(0);

	m_req_account_id.store(0);
	m_rsp_account_id.store(0);

	m_req_position_id.store(0);
	m_rsp_position_id.store(0);
	m_position_ready.store(false);

	m_need_query_bank.store(false);
	m_need_query_register.store(false);

	m_something_changed = false;
	m_peeking_message = false;

	m_need_save_file.store(false);

	m_need_query_broker_trading_params.store(false);
	m_Algorithm_Type = THOST_FTDC_AG_None;

	m_is_qry_his_settlement_info.store(false);
	m_his_settlement_info = "";
	m_qry_his_settlement_info_trading_days.clear();

	m_position_inited.store(false);
}

void traderctp::OnReqStartCTP(const std::string& msg)
{
	Log().WithField("fun","OnReqStartCTP")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)		
		.Log(LOG_INFO,"req start ctp");	

	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun","OnReqStartCTP")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_WARNING,"traderctp parse json fail");		
		return;
	}
	
	req_start_trade_instance req;
	nss.ToVar(req);
	if (req.aid != "req_start_ctp")
	{
		return;
	}

	//如果CTP已经登录成功
	if (m_b_login.load())
	{
		Log().WithField("fun","OnReqStartCTP")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_INFO,"has login success instance req start ctp");

		ClearOldData();
		
		ReqLogin reqLogin;
		reqLogin.aid = "req_login";
		reqLogin.bid = req.bid;
		reqLogin.user_name = req.user_name;
		reqLogin.password = req.password;

		ProcessReqLogIn(0, reqLogin);
	}
	else
	{
		Log().WithField("fun","OnReqStartCTP")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_INFO,"not login instance req start ctp");
				
		ReqLogin reqLogin;
		reqLogin.aid = "req_login";
		reqLogin.bid = req.bid;
		reqLogin.user_name = req.user_name;
		reqLogin.password = req.password;

		ProcessReqLogIn(0,reqLogin);
	}
}

void traderctp::OnReqStopCTP(const std::string& msg)
{
	Log().WithField("fun","OnReqStopCTP")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.Log(LOG_INFO,"req stop ctp");
	
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun","OnReqStopCTP")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("msgcontent",msg)
			.Log(LOG_WARNING,"traderctp parse json fail");		
		return;
	}

	req_start_trade_instance req;
	nss.ToVar(req);
	if (req.aid != "req_stop_ctp")
	{
		return;
	}

	//如果CTP已经登录成功
	if (m_b_login.load())
	{
		Log().WithField("fun","OnReqStopCTP")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_INFO,"has login success instance req stop ctp");		

		if (m_need_save_file.load())
		{
			SaveToFile();
		}

		StopTdApi();
	}
	else
	{
		Log().WithField("fun","OnReqStopCTP")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_INFO,"not login instance req stop ctp");

		StopTdApi();
	}
}

void traderctp::OnClientReqQrySettlementInfo(const qry_settlement_info
	& qrySettlementInfo)
{
	m_qry_his_settlement_info_trading_days.push_back(qrySettlementInfo.trading_day);
}

void traderctp::ProcessReqLogIn(int connId, ReqLogin& req)
{
	Log().WithField("fun","ProcessReqLogIn")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("client_ip",req.client_ip)
		.WithField("client_port",req.client_port)
		.WithField("client_app_id",req.client_app_id)
		.WithField("client_system_info_length",(int)(req.client_system_info.length()))
		.WithField("front",req.front)
		.WithField("broker_id",req.broker_id)
		.WithField("connId",connId)
		.Log(LOG_INFO,"traderctp ProcessReqLogIn");	

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
			OutputNotifySycn(connId,338,u8"重复发送登录请求!");
			return;
		}

		//简单比较登陆凭证,判断是否能否成功登录
		if ((_req_login.bid == req.bid)
			&& (_req_login.user_name == req.user_name)
			&& (_req_login.password == req.password))
		{
			if (0 != connId)
			{
				//加入登录客户端列表
				m_logined_connIds.push_back(connId);
				OutputNotifySycn(connId,324,u8"登录成功");

				char json_str[1024];
				sprintf(json_str, (u8"{"\
					"\"aid\": \"rtn_data\","\
					"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
					"\"user_id\" : \"%s\","\
					"\"trading_day\" : \"%s\""
					"}}}}]}")
					, _req_login.user_name.c_str()
					, _req_login.user_name.c_str()
					, m_trading_day.c_str());
				std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
				_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));

				//发送用户数据
				SendUserDataImd(connId);
				
				//重发结算结果确认信息
				ReSendSettlementInfo(connId);
			}
		}
		else
		{
			if (0 != connId)
			{
				OutputNotifySycn(connId,339,u8"账户和密码不匹配!");
			}			
		}
	}
	else
	{
		if ((0 == connId)&&(!g_config.auto_confirm_settlement))
		{
			g_config.auto_confirm_settlement = true;
		}

		_req_login = req;
		auto it = g_config.brokers.find(_req_login.bid);
		_req_login.broker = it->second;

		//为了支持次席而添加的功能
		if ((!_req_login.broker_id.empty()) &&
			(!_req_login.front.empty()))
		{
			Log().WithField("fun","ProcessReqLogIn")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)				
				.WithField("front",req.front)
				.WithField("broker_id",req.broker_id)				
				.Log(LOG_INFO,"ctp login from custom front and broker_id");			

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
		ECTPLoginStatus login_status = WaitLogIn();	
		long status_code = static_cast<long>(login_status);
		if ((ECTPLoginStatus::init == login_status)
			||(ECTPLoginStatus::reqAuthenFail== login_status)
			|| (ECTPLoginStatus::rspAuthenFail == login_status)
			||(ECTPLoginStatus::regSystemInfoFail== login_status)
			|| (ECTPLoginStatus::reqUserLoginFail == login_status)
			|| (ECTPLoginStatus::rspLoginFail == login_status)			
			)
		{
			m_b_login.store(false);
			StopTdApi();
			if (connId != 0)
			{
				m_loging_connectId = connId;
				OutputNotifySycn(connId,status_code,u8"用户登录失败!");
			}
			return;
		}		
		else if (ECTPLoginStatus::rspLoginFailNeedModifyPassword == login_status)
		{
			m_b_login.store(false);
			if (connId != 0)
			{
				m_loging_connectId = connId;
				OutputNotifySycn(connId,status_code,u8"用户登录失败!");
			}
			return;
		}
		else if (ECTPLoginStatus::reqLoginTimeOut == login_status)
		{
			m_b_login.store(false);
			StopTdApi();
			if (connId != 0)
			{
				m_loging_connectId = connId;
				OutputNotifySycn(connId,status_code,u8"用户登录超时!");
			}
			return;
		}
		else if (ECTPLoginStatus::rspLoginSuccess == login_status)
		{
			m_peeking_message = true;
			m_b_login.store(true);

			//加入登录客户端列表
			if (connId != 0)
			{
				m_logined_connIds.push_back(connId);

				char json_str[1024];
				sprintf(json_str, (u8"{"\
					"\"aid\": \"rtn_data\","\
					"\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
					"\"user_id\" : \"%s\","\
					"\"trading_day\" : \"%s\""
					"}}}}]}")
					, _req_login.user_name.c_str()
					, _req_login.user_name.c_str()
					, m_trading_day.c_str());
				std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
				_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));
			}

			//加载条件单数据
			m_condition_order_manager.Load(_req_login.bid,
				_req_login.user_name,
				_req_login.password,
				m_trading_day);			
		}		
	}
}

ECTPLoginStatus traderctp::WaitLogIn()
{
	boost::unique_lock<boost::mutex> lock(_logInmutex);
	_logIn_status = ECTPLoginStatus::init;
	m_pTdApi->Init();
	bool notify = _logInCondition.timed_wait(lock, boost::posix_time::seconds(15));
	if (ECTPLoginStatus::init == _logIn_status)
	{
		if (!notify)
		{
			_logIn_status = ECTPLoginStatus::reqLoginTimeOut;

			Log().WithField("fun","WaitLogIn")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)				
				.Log(LOG_WARNING,"CTP login timeout,trading fronts is closed or trading fronts config is error");			
		}
	}
	return _logIn_status;
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
		Log().WithField("fun","InitTdApi")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_INFO,"fens address is used");
		
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
		Log().WithField("fun","StopTdApi")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.Log(LOG_INFO,"ctp OnFinish");

		m_pTdApi->RegisterSpi(NULL);
		m_pTdApi->Release();
		m_pTdApi = NULL;
	}
}

void traderctp::SendMsg(int connId, std::shared_ptr<std::string> msg_ptr)
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
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("msglen",(int)msg.length())
				.Log(LOG_ERROR,"SendMsg exception");
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
			Log().WithField("fun","SendMsg")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("msglen",(int)totalLength)
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"SendMsg exception");			
		}
	}
}

void traderctp::SendMsgAll(std::shared_ptr<std::string> conn_str_ptr, std::shared_ptr<std::string> msg_ptr)
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
			Log().WithField("fun","SendMsgAll")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("msglen",(int)msg.length())
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"SendMsg exception");			
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
			Log().WithField("fun","SendMsgAll")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("msglen",(int)totalLength)
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"SendMsg exception");			
		}
	}
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
	node_message.AddMember("session_id", m_session_id, nss.m_doc->GetAllocator());
	node_message.AddMember("content", rapidjson::Value(notify_msg.c_str(), nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	
	rapidjson::Pointer("/data/0/notify/" + GenerateGuid()).Set(*nss.m_doc, node_message);
	
	std::string json_str;
	nss.ToString(&json_str);

	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));
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
	node_message.AddMember("session_id", m_session_id, nss.m_doc->GetAllocator());
	node_message.AddMember("content", rapidjson::Value(ret_msg.c_str(), nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
	
	rapidjson::Pointer("/data/0/notify/" + GenerateGuid()).Set(*nss.m_doc, node_message);
	
	std::string json_str;
	nss.ToString(&json_str);

	std::string str = GetConnectionStr();
	if (!str.empty())
	{
		std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
		std::shared_ptr<std::string> conn_ptr(new std::string(str));
		_ios.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
	}
	else
	{
		Log().WithField("fun","OutputNotifyAllSycn")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)			
			.Log(LOG_INFO,"GetConnectionStr is empty");		
	}
}

#pragma endregion

#pragma region client_request

void traderctp::OnClientReqChangePassword(CThostFtdcUserPasswordUpdateField f)
{
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqUserPasswordUpdate(&f, 0);
	Log().WithField("fun","OnClientReqChangePassword")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("ret",r)
		.Log(LOG_INFO, "send ReqUserPasswordUpdate request!");
	if (0 != r)
	{		
		OutputNotifyAllSycn(351,u8"修改密码请求发送失败!","WARNING");
	}	
}

void traderctp::OnClientReqTransfer(CThostFtdcReqTransferField f)
{
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	strcpy_x(f.AccountID, _req_login.user_name.c_str());
	strcpy_x(f.BankBranchID, "0000");
	f.SecuPwdFlag = THOST_FTDC_BPWDF_BlankCheck;	// 核对密码
	f.BankPwdFlag = THOST_FTDC_BPWDF_NoCheck;	// 核对密码
	f.VerifyCertNoFlag = THOST_FTDC_YNI_No;

	if (f.TradeAmount >= 0)
	{
		strcpy_x(f.TradeCode, "202001");
		int nRequestID = _requestID++;
		int r = m_pTdApi->ReqFromBankToFutureByFuture(&f,nRequestID);
		
		SerializerLogCtp nss;
		nss.FromVar(f);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnClientReqTransfer")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("ret",r)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO, "send ReqFromBankToFutureByFuture request!");

		if (0 != r)
		{					
			OutputNotifyAllSycn(352, u8"银期转账请求发送失败!", "WARNING");
		}		
		m_req_transfer_list.push_back(nRequestID);		
	}
	else
	{
		strcpy_x(f.TradeCode, "202002");
		f.TradeAmount = -f.TradeAmount;
		int nRequestID = _requestID++;
		int r = m_pTdApi->ReqFromFutureToBankByFuture(&f,nRequestID);

		SerializerLogCtp nss;
		nss.FromVar(f);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log().WithField("fun","OnClientReqTransfer")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("ret",r)
			.WithField("RequestID",nRequestID)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO,"send ReqFromFutureToBankByFuture request!");

		if (0 != r)
		{				
			OutputNotifyAllSycn(352,u8"银期转账请求发送失败!","WARNING");
		}		
		m_req_transfer_list.push_back(nRequestID);		
	}
}

void traderctp::OnClientReqCancelOrder(CtpActionCancelOrder d)
{
	if (d.local_key.user_id.substr(0, _req_login.user_name.size()) != _req_login.user_name)
	{
		OutputNotifyAllSycn(353,u8"撤单user_id错误,不能撤单","WARNING");
		return;
	}

	if (d.local_key.order_id.empty())
	{
		OutputNotifyAllSycn(354,u8"撤单指定的order_id不存在,不能撤单","WARNING");
		return;
	}
	   
	auto it = m_ordermap_local_remote.find(d.local_key);
	if (it == m_ordermap_local_remote.end())
	{
		OutputNotifyAllSycn(354,u8"撤单指定的order_id不存在,不能撤单","WARNING");
		return;
	}
	
	RemoteOrderKey rkey= it->second;	
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

	int r = m_pTdApi->ReqOrderAction(&d.f, 0);
	if (0 != r)
	{
		OutputNotifyAllSycn(355,u8"撤单请求发送失败!", "WARNING");
	}

	SerializerLogCtp nss;
	nss.FromVar(d.f);
	std::string strMsg = "";
	nss.ToString(&strMsg);

	Log().WithField("fun", "OnClientReqCancelOrder")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("user_id", d.local_key.user_id)
		.WithField("order_id", d.local_key.order_id)
		.WithField("FrontID", rkey.front_id)
		.WithField("SessionID", rkey.session_id)
		.WithField("OrderRef", rkey.order_ref)
		.WithField("ret", r)
		.WithPack("ctp_pack",strMsg)
		.Log(LOG_INFO, "send ReqOrderAction request!"); 
}

int traderctp::OnClientReqInsertOrder(CtpActionInsertOrder d)
{
	static int order_count = 0;

	if (d.local_key.user_id.substr(0, _req_login.user_name.size()) != _req_login.user_name)
	{
		OutputNotifyAllSycn(356,u8"报单user_id错误，不能下单", "WARNING");
		return 0;
	}

	strcpy_x(d.f.BrokerID, m_broker_id.c_str());
	strcpy_x(d.f.UserID, _req_login.user_name.c_str());
	strcpy_x(d.f.InvestorID, _req_login.user_name.c_str());
	
	RemoteOrderKey rkey;
	rkey.exchange_id = d.f.ExchangeID;
	rkey.instrument_id = d.f.InstrumentID;
	rkey.session_id = m_session_id;
	rkey.front_id = m_front_id;
	rkey.order_ref = std::to_string(m_order_ref++);

	//客户端没有提供订单编号
	if (d.local_key.order_id.empty())
	{
		char buf[1024];
		sprintf(buf, "OTG.%s.%08x.%d"
			,rkey.order_ref.c_str()
			,rkey.session_id
			,rkey.front_id);
		d.local_key.order_id = buf;	
	}	

	//客户端报单编号已经存在
	auto it = m_ordermap_local_remote.find(d.local_key);
	if (it != m_ordermap_local_remote.end())
	{
		OutputNotifyAllSycn(357
			, u8"报单单号重复，不能下单"
			, "WARNING");
		return 0;
	}
	
	m_ordermap_local_remote[d.local_key] = rkey;
	m_ordermap_remote_local[rkey] = d.local_key;
	m_need_save_file.store(true);
		
	strcpy_x(d.f.OrderRef, rkey.order_ref.c_str());
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
	if (0 == r)
	{		
		SerializerLogCtp nss;
		nss.FromVar(d.f);
		std::string strMsg = "";
		nss.ToString(&strMsg);
		
		order_count++;

		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("user_id", d.local_key.user_id)
			.WithField("order_id", d.local_key.order_id)
			.WithField("FrontID", rkey.front_id)
			.WithField("SessionID", rkey.session_id)
			.WithField("OrderRef", rkey.order_ref)
			.WithField("ret", r)
			.WithField("order_count", order_count)
			.WithPack("ctp_pack", strMsg)
			.Log(LOG_INFO, "send ReqOrderInsert request!");
	}
	else if (-1 != r)
	{
		SerializerLogCtp nss;
		nss.FromVar(d.f);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		OutputNotifyAllSycn(358,u8"下单请求发送失败!", "WARNING");

		Log().WithField("fun", "OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("user_id", d.local_key.user_id)
			.WithField("order_id", d.local_key.order_id)
			.WithField("FrontID", rkey.front_id)
			.WithField("SessionID", rkey.session_id)
			.WithField("OrderRef", rkey.order_ref)
			.WithField("ret", r)
			.WithField("order_count",order_count)
			.WithPack("ctp_pack",strMsg)
			.Log(LOG_INFO, "send ReqOrderInsert request fail!");
	}	
	else
	{
		Log().WithField("fun","OnClientReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("user_id", d.local_key.user_id)
			.WithField("order_id", d.local_key.order_id)
			.WithField("FrontID", rkey.front_id)
			.WithField("SessionID", rkey.session_id)
			.WithField("OrderRef", rkey.order_ref)
			.WithField("ret", r)
			.WithField("order_count", order_count)
			.Log(LOG_WARNING,"send order to ctp fail,because of sending too fast,will try it again later");
	}

	return r;
}

void traderctp::OnClientPeekMessage()
{
	m_peeking_message = true;
	//向客户端发送账户信息
	SendUserData();
}

#pragma endregion

#pragma region ConditionOrderCallBack

void traderctp::OnUserDataChange()
{
	SendUserData();
}

void traderctp::OutputNotifyAll(long notify_code, const std::string& ret_msg
	, const char* level	,const char* type)
{
	OutputNotifyAllSycn(notify_code,ret_msg,level,type);
}

void traderctp::CheckTimeConditionOrder()
{
	std::set<std::string>& os = m_condition_order_manager.GetTimeCoSet();
	if (os.empty())
	{
		return;
	}
	m_condition_order_manager.OnCheckTime();
}

void traderctp::CheckPriceConditionOrder()
{
	TInstOrderIdListMap& om = m_condition_order_manager.GetPriceCoMap();
	if (om.empty())
	{
		return;
	}

	m_condition_order_manager.OnCheckPrice();
}

bool traderctp::ConditionOrder_Open(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	,ctp_condition_order_task& task
	, int nOrderIndex)
{	
	CThostFtdcInputOrderField f;
	memset(&f, 0, sizeof(CThostFtdcInputOrderField));
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	strcpy_x(f.InvestorID, _req_login.user_name.c_str());
	strcpy_x(f.ExchangeID, co.exchange_id.c_str());
	strcpy_x(f.InstrumentID, co.instrument_id.c_str());

	//开仓
	f.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	   
	//开多
	if (EOrderDirection::buy == co.direction)
	{
		f.Direction = THOST_FTDC_D_Buy;
	}
	//开空
	else
	{
		f.Direction = THOST_FTDC_D_Sell;
	}

	//数量类型
	if (EVolumeType::num == co.volume_type)
	{
		f.VolumeTotalOriginal = co.volume;
		f.VolumeCondition = THOST_FTDC_VC_AV;
	}
	else
	{
		//开仓时必须指定具体手数
		Log().WithField("fun","ConditionOrder_Open")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("exchange_id", co.exchange_id)
			.WithField("instrument_id",co.instrument_id)	
			.Log(LOG_WARNING,"has bad volume_type");

		return false;
	}

	//价格类型

	//限价
	if (EPriceType::limit == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		f.LimitPrice = co.limit_price;
	}
	//触发价
	else if (EPriceType::contingent == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
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
		f.LimitPrice = last_price;
	}
	//对价
	else if (EPriceType::consideration == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		//开多
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.ask_price1;
		}
		//开空
		else
		{
			f.LimitPrice = ins.bid_price1;
		}
	}
	//市价
	else if (EPriceType::market == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_IOC;
		//开多
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.upper_limit;
		}
		//开空
		else
		{
			f.LimitPrice = ins.lower_limit;
		}
	}
	//超价
	else if (EPriceType::over == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		//开多
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.ask_price1 + ins.price_tick;
			if (f.LimitPrice > ins.upper_limit)
			{
				f.LimitPrice = ins.upper_limit;
			}
		}
		//开空
		else
		{
			f.LimitPrice = ins.bid_price1 - ins.price_tick;
			if (f.LimitPrice < ins.lower_limit)
			{
				f.LimitPrice = ins.lower_limit;
			}
		}
	}

	f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	f.ContingentCondition = THOST_FTDC_CC_Immediately;
	f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

	task.has_order_to_cancel = false;	
	task.has_second_orders_to_send = false;

	if (f.VolumeTotalOriginal > 0)
	{
		task.has_first_orders_to_send = true;
		CtpActionInsertOrder actionInsertOrder;
		actionInsertOrder.local_key.user_id = _req_login.user_name;
		actionInsertOrder.local_key.order_id = order.order_id + "_open_" + std::to_string(nOrderIndex);
		actionInsertOrder.f = f;
		task.first_orders_to_send.push_back(actionInsertOrder);
	}	
	
	return true;
}

bool traderctp::SetConditionOrderPrice(CThostFtdcInputOrderField& f
	, const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins)
{
	//价格类型
	//限价
	if (EPriceType::limit == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		f.LimitPrice = co.limit_price;
		return true;
	}
	//触发价
	else if (EPriceType::contingent == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
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
		f.LimitPrice = last_price;
		return true;
	}
	//对价
	else if (EPriceType::consideration == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.ask_price1;
		}
		//卖平
		else
		{
			f.LimitPrice = ins.bid_price1;
		}
		return true;
	}
	//市价
	else if (EPriceType::market == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_IOC;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.upper_limit;
		}
		//卖平
		else
		{
			f.LimitPrice = ins.lower_limit;
		}
		return true;
	}
	//超价
	else if (EPriceType::over == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.ask_price1 + ins.price_tick;
			if (f.LimitPrice > ins.upper_limit)
			{
				f.LimitPrice = ins.upper_limit;
			}
		}
		//卖平
		else
		{
			f.LimitPrice = ins.bid_price1 - ins.price_tick;
			if (f.LimitPrice < ins.lower_limit)
			{
				f.LimitPrice = ins.lower_limit;
			}
		}
		return true;
	}
	return false;
}

bool traderctp::ConditionOrder_CloseTodayPrior_NeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id,co.instrument_id,_req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的今仓手数,不需要撤单
		if (co.volume <= pos.pos_short_today - pos.volume_short_frozen_today)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID,m_broker_id.c_str());
			strcpy_x(f.UserID,_req_login.user_name.c_str());
			strcpy_x(f.InvestorID,_req_login.user_name.c_str());
			strcpy_x(f.ExchangeID,co.exchange_id.c_str());
			strcpy_x(f.InstrumentID,co.instrument_id.c_str());

			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			//买平
			f.Direction = THOST_FTDC_D_Buy;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);

					task.has_second_orders_to_send = false;
					task.has_order_to_cancel = false;
				}

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//要平的手数小于等于今仓手数(包括冻结的手数),需要先撤掉平今仓的手数
		else if (co.volume <= pos.pos_short_today)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			f.Direction = THOST_FTDC_D_Buy;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}				

				task.has_second_orders_to_send = false;

				//撤掉所有平今仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f2;
						memset(&f2, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f2.BrokerID, m_broker_id.c_str());
						strcpy_x(f2.UserID, _req_login.user_name.c_str());
						strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f2.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f2.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f2;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}

		}
		//要平的手数小于等于所有空仓(不包手冻结的昨仓),需要先撤掉平今仓的手数
		else if (co.volume <= pos.pos_short_today + pos.pos_short_his - pos.volume_short_frozen_his)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());
			//平今
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f1.Direction = THOST_FTDC_D_Buy;
			f1.VolumeTotalOriginal = pos.pos_short_today;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());
			//平昨
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f2.Direction = THOST_FTDC_D_Buy;
			f2.VolumeTotalOriginal = co.volume - pos.pos_short_today;
			f2.VolumeCondition = THOST_FTDC_VC_AV;
			
			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
				
				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}

				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}
				
				task.has_second_orders_to_send = false;

				//撤掉所有平今仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);
					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return false;
			}
		}
		//要平的手数小于等于所有空仓(包括冻结的手数),要先撤掉所有平仓的挂单,包括平今和平昨
		else if (co.volume <= pos.pos_short_today + pos.pos_short_his)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());
			//平今
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f1.Direction = THOST_FTDC_D_Buy;
			f1.VolumeTotalOriginal = pos.pos_short_today;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());
			//平昨
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f2.Direction = THOST_FTDC_D_Buy;
			f2.VolumeTotalOriginal = co.volume - pos.pos_short_today;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				task.has_first_orders_to_send = true;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}			

				task.has_second_orders_to_send = false;

				//撤掉所有平仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
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
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的今仓手数,不需要撤单
		if (co.volume <= pos.pos_long_today - pos.volume_long_frozen_today)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}				

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;
				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}

		}
		//要平的手数小于等于今仓手数(包括冻结的手数),需要先撤掉平今仓的手数
		else if (co.volume <= pos.pos_long_today)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f,order,co,ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}			

				task.has_second_orders_to_send = false;

				//撤掉所有平今仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f2;
						memset(&f2, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f2.BrokerID, m_broker_id.c_str());
						strcpy_x(f2.UserID, _req_login.user_name.c_str());
						strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f2.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f2.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f2;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//要平的手数小于等于所有多仓(不包手冻结的昨仓),需要先撤掉平今仓的手数
		else if (co.volume <= pos.pos_long_today + pos.pos_long_his - pos.volume_long_frozen_his)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平今
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_long_today;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());
			//平昨
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - pos.pos_long_today;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1,order,co,ins)
				&& SetConditionOrderPrice(f2,order,co,ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}			

				task.has_second_orders_to_send = false;

				//撤掉所有平今仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return false;
			}

		}
		//要平的手数小于等于所有多仓(包括冻结的手数),要先撤掉所有平仓的挂单,包括平今和平昨
		else if (co.volume <= pos.pos_long_today + pos.pos_long_his)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平今
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_long_today;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());

			//平昨
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - pos.pos_long_today;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}			

				task.has_second_orders_to_send = false;

				//撤掉所有平仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);
					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
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
			.WithField("exchange_id", co.exchange_id)
			.WithField("instrument_id", co.instrument_id)
			.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool traderctp::ConditionOrder_CloseTodayPrior_NotNeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的今仓手数,只平今
		if (co.volume <= pos.pos_short_today - pos.volume_short_frozen_today)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			f.Direction = THOST_FTDC_D_Buy;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}			

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//如果可平的手数小于所有可平的今昨仓手数,平今同时平昨
		else if (co.volume <= pos.pos_short_today - pos.volume_short_frozen_today+ pos.pos_short_his- pos.volume_short_frozen_his)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平今
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_short_today - pos.volume_short_frozen_today;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());

			//平昨
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - f1.VolumeTotalOriginal;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}			

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;

				return true;
			}
			//价格设置不合法
			else
			{
				return false;
			}			
		}
		//可平不足,不平
		else
		{
			Log().WithField("fun", "ConditionOrder_CloseTodayPrior_NotNeedCancel")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("exchange_id", co.exchange_id)
			.WithField("instrument_id", co.instrument_id)
			.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的今仓手数,平今
		if (co.volume <= pos.pos_long_today - pos.volume_long_frozen_today)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}				

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;
				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//如果可平的手数小于所有可平的今昨仓手数,平今同时平昨
		else if (co.volume <= pos.pos_long_today - pos.volume_long_frozen_today+pos.pos_long_his- pos.volume_long_frozen_his)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平今
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_long_today - pos.volume_long_frozen_today;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());

			//平昨
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - f1.VolumeTotalOriginal;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}				

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;
				return true;
			}
			//价格设置不合法
			else
			{
				return false;
			}
		}
		//可平不足,不平
		else
		{
			Log().WithField("fun","ConditionOrder_CloseTodayPrior_NotNeedCancel")
				.WithField("key", _key)
				.WithField("bid", _req_login.bid)
				.WithField("user_name", _req_login.user_name)
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}		
	}
}

bool traderctp::ConditionOrder_CloseYesTodayPrior_NeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的昨仓手数,不需要撤原平仓单
		if (co.volume <= pos.pos_short_his - pos.volume_short_frozen_his)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			f.Direction = THOST_FTDC_D_Buy;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}
				
				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//要平的手数小于等于昨仓手数(包括冻结的手数),要先撤掉所有平昨仓的挂单
		else if (co.volume <= pos.pos_short_his)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			f.Direction = THOST_FTDC_D_Buy;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}				

				task.has_second_orders_to_send = false;

				//撤掉所有平昨仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f2;
						memset(&f2, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f2.BrokerID, m_broker_id.c_str());
						strcpy_x(f2.UserID, _req_login.user_name.c_str());
						strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f2.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f2.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f2;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}

		}
		//要平的手数小于等于所有空仓(不包手冻结的昨仓),要先撤掉所有平昨仓的挂单
		else if (co.volume <= pos.pos_short_his+pos.pos_short_today-pos.volume_short_frozen_today)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());
			//平昨
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f1.Direction = THOST_FTDC_D_Buy;
			f1.VolumeTotalOriginal = pos.pos_short_his;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());
			//平今
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f2.Direction = THOST_FTDC_D_Buy;
			f2.VolumeTotalOriginal = co.volume - pos.pos_short_his;
			f2.VolumeCondition = THOST_FTDC_VC_AV;
			
			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}				

				task.has_second_orders_to_send = false;

				//撤掉所有平昨仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);
					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return false;
			}
		}
		//要平的手数小于等于所有空仓(包括冻结的手数),要先撤掉所有平仓的挂单
		else if (co.volume <= pos.pos_short_his+pos.pos_short_today)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());
			//平昨
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f1.Direction = THOST_FTDC_D_Buy;
			f1.VolumeTotalOriginal = pos.pos_short_his;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());
			//平今
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f2.Direction = THOST_FTDC_D_Buy;
			f2.VolumeTotalOriginal = co.volume - pos.pos_short_his;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}
				
				task.has_second_orders_to_send = false;

				//撤掉所有平仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);
					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
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
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的昨仓手数,不需要撤原平仓单
		if (co.volume <= pos.pos_long_his - pos.volume_long_frozen_his)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}				

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;
				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}

		}
		//要平的手数小于等于昨仓手数(包括冻结的手数),先撤掉所有平昨仓的挂单
		else if (co.volume <= pos.pos_long_his)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}
				
				task.has_second_orders_to_send = false;

				//撤掉所有平昨仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f2;
						memset(&f2, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f2.BrokerID, m_broker_id.c_str());
						strcpy_x(f2.UserID, _req_login.user_name.c_str());
						strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f2.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f2.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f2;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//要平的手数小于等于所有多仓(不包手冻结的昨仓),先撤掉所有平昨仓的挂单
		else if (co.volume <= pos.pos_long_his+pos.pos_long_today-pos.volume_long_frozen_today)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平昨
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_long_his;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());
			//平今
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - pos.pos_long_his;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}
				
				task.has_second_orders_to_send = false;

				//撤掉所有平昨仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);

					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
				return false;
			}

		}
		//要平的手数小于等于所有多仓(包括冻结的手数),要先撤掉所有平仓的挂单
		else if (co.volume <= pos.pos_long_his+pos.pos_long_today)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平昨
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_long_his;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());

			//平今
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - pos.pos_long_his;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}				

				task.has_second_orders_to_send = false;

				//撤掉所有平仓的挂单
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
						CtpActionCancelOrder cancelOrder;
						cancelOrder.local_key.order_id = orderId;
						cancelOrder.local_key.user_id = _req_login.user_name.c_str();

						CThostFtdcInputOrderActionField f3;
						memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
						strcpy_x(f3.BrokerID, m_broker_id.c_str());
						strcpy_x(f3.UserID, _req_login.user_name.c_str());
						strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
						strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
						strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

						cancelOrder.f = f3;

						task.has_order_to_cancel = true;
						task.orders_to_cancel.push_back(cancelOrder);
					}
				}//end for

				return true;
			}
			//价格设置不合法
			else
			{
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
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool traderctp::ConditionOrder_CloseYesTodayPrior_NotNeedCancel(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		//要平的手数小于等于可平的昨仓手数,只平昨
		if (co.volume <= pos.pos_short_his - pos.volume_short_frozen_his)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			f.Direction = THOST_FTDC_D_Buy;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}
				
				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;

				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//如果可平的手数小于所有可平的今昨仓手数,平昨又平今
		else if (co.volume <= pos.pos_short_his - pos.volume_short_frozen_his + pos.pos_short_today - pos.volume_short_frozen_today)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平昨
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_short_his - pos.volume_short_frozen_his;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());

			//平今
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - f1.VolumeTotalOriginal;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}
				
				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;

				return true;
			}
			//价格设置不合法
			else
			{
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
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close short is less than will close short");
			return false;
		}
	}
	//卖平
	else
	{
		//要平的手数小于等于可平的昨仓手数,只平昨
		if (co.volume <= pos.pos_long_his - pos.volume_long_frozen_his)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			f.VolumeTotalOriginal = co.volume;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}
				
				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;
				return true;
			}
			//价格设置不合法
			else
			{
				return  false;
			}
		}
		//如果可平的手数小于所有可平的今昨仓手数,平昨又平今
		else if (co.volume <= pos.pos_long_his - pos.volume_long_frozen_his+pos.pos_long_today - pos.volume_long_frozen_today)
		{
			CThostFtdcInputOrderField f1;
			memset(&f1, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f1.BrokerID, m_broker_id.c_str());
			strcpy_x(f1.UserID, _req_login.user_name.c_str());
			strcpy_x(f1.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f1.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f1.InstrumentID, co.instrument_id.c_str());

			//平昨
			f1.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f1.Direction = THOST_FTDC_D_Sell;
			f1.VolumeTotalOriginal = pos.pos_long_his - pos.volume_long_frozen_his;
			f1.VolumeCondition = THOST_FTDC_VC_AV;

			CThostFtdcInputOrderField f2;
			memset(&f2, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f2.BrokerID, m_broker_id.c_str());
			strcpy_x(f2.UserID, _req_login.user_name.c_str());
			strcpy_x(f2.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f2.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f2.InstrumentID, co.instrument_id.c_str());

			//平今
			f2.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f2.Direction = THOST_FTDC_D_Sell;
			f2.VolumeTotalOriginal = co.volume - f1.VolumeTotalOriginal;
			f2.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f1, order, co, ins)
				&& SetConditionOrderPrice(f2, order, co, ins))
			{
				f1.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f1.ContingentCondition = THOST_FTDC_CC_Immediately;
				f1.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				f2.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f2.ContingentCondition = THOST_FTDC_CC_Immediately;
				f2.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f1.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder1;
					actionInsertOrder1.local_key.user_id = _req_login.user_name;
					actionInsertOrder1.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder1.f = f1;
					task.first_orders_to_send.push_back(actionInsertOrder1);
				}
				
				if (f2.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder2;
					actionInsertOrder2.local_key.user_id = _req_login.user_name;
					actionInsertOrder2.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder2.f = f2;
					task.first_orders_to_send.push_back(actionInsertOrder2);
				}				

				task.has_second_orders_to_send = false;
				task.has_order_to_cancel = false;
				return true;
			}
			//价格设置不合法
			else
			{
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
				.WithField("exchange_id", co.exchange_id)
				.WithField("instrument_id", co.instrument_id)
				.Log(LOG_WARNING, "can close long is less than will close long");
			return false;
		}
	}
}

bool traderctp::ConditionOrder_CloseAll(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
	, int nOrderIndex)
{
	//合约
	std::string symbol = co.exchange_id + "." + co.instrument_id;
	//持仓
	Position& pos = GetPosition(co.exchange_id,co.instrument_id,_req_login.user_name);

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		task.has_second_orders_to_send = false;

		//撤掉所有平仓的挂单
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
				CtpActionCancelOrder cancelOrder;
				cancelOrder.local_key.order_id = orderId;
				cancelOrder.local_key.user_id = _req_login.user_name.c_str();

				CThostFtdcInputOrderActionField f3;
				memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
				strcpy_x(f3.BrokerID, m_broker_id.c_str());
				strcpy_x(f3.UserID, _req_login.user_name.c_str());
				strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
				strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
				strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

				cancelOrder.f = f3;

				task.has_order_to_cancel = true;
				task.orders_to_cancel.push_back(cancelOrder);

			}
		}//end for

		//如果今仓手数大于零(包括冻结的今仓手数),平今
		if (pos.pos_short_today  > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());
			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f.Direction = THOST_FTDC_D_Buy;
			f.VolumeTotalOriginal = pos.pos_short_today;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f,order,co,ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}					
			}
			else
			{
				return  false;
			}								
		}

		//如果昨仓手数大于零(包括冻结的昨仓手数)
		if (pos.pos_short_his > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());
			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f.Direction = THOST_FTDC_D_Buy;
			f.VolumeTotalOriginal = pos.pos_short_his;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f,order,co,ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				if (f.VolumeTotalOriginal > 0)
				{
					task.has_first_orders_to_send = true;
					CtpActionInsertOrder actionInsertOrder;
					actionInsertOrder.local_key.user_id = _req_login.user_name;
					actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
					actionInsertOrder.f = f;
					task.first_orders_to_send.push_back(actionInsertOrder);
				}						
			}
			else
			{
				return  false;
			}
		}

		if (task.has_first_orders_to_send)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	//卖平
	else
	{
		task.has_second_orders_to_send = false;

		//撤掉所有平仓的挂单
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
				CtpActionCancelOrder cancelOrder;
				cancelOrder.local_key.order_id = orderId;
				cancelOrder.local_key.user_id = _req_login.user_name.c_str();

				CThostFtdcInputOrderActionField f3;
				memset(&f3, 0, sizeof(CThostFtdcInputOrderActionField));
				strcpy_x(f3.BrokerID, m_broker_id.c_str());
				strcpy_x(f3.UserID, _req_login.user_name.c_str());
				strcpy_x(f3.InvestorID, _req_login.user_name.c_str());
				strcpy_x(f3.ExchangeID, order.exchange_id.c_str());
				strcpy_x(f3.InstrumentID, order.instrument_id.c_str());

				cancelOrder.f = f3;

				task.has_order_to_cancel = true;
				task.orders_to_cancel.push_back(cancelOrder);
			}
		}//end for

		//如果今仓手数大于零(包括冻结的今仓手数),平今
		if (pos.pos_long_today > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());
			//平今
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;
			f.Direction = THOST_FTDC_D_Sell;
			f.VolumeTotalOriginal = pos.pos_long_today;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f, order, co, ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				task.has_first_orders_to_send = true;
				CtpActionInsertOrder actionInsertOrder;
				actionInsertOrder.local_key.user_id = _req_login.user_name;
				actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
				actionInsertOrder.f = f;
				task.first_orders_to_send.push_back(actionInsertOrder);
			}
			else
			{
				return  false;
			}
		}

		//如果昨仓手数大于零(包括冻结的昨仓手数),平昨
		if (pos.pos_long_his > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());
			//平昨
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;
			f.Direction = THOST_FTDC_D_Sell;
			f.VolumeTotalOriginal = pos.pos_long_his;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			if (SetConditionOrderPrice(f,order,co,ins))
			{
				f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
				f.ContingentCondition = THOST_FTDC_CC_Immediately;
				f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

				task.has_first_orders_to_send = true;
				CtpActionInsertOrder actionInsertOrder;
				actionInsertOrder.local_key.user_id = _req_login.user_name;
				actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
				actionInsertOrder.f = f;
				task.first_orders_to_send.push_back(actionInsertOrder);
			}
			else
			{
				return  false;
			}
		}

		if (task.has_first_orders_to_send)
		{			
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool traderctp::ConditionOrder_Close(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
	, int nOrderIndex)
{
	std::string symbol = co.exchange_id + "." + co.instrument_id;

	CThostFtdcInputOrderField f;
	memset(&f, 0, sizeof(CThostFtdcInputOrderField));
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	strcpy_x(f.InvestorID, _req_login.user_name.c_str());
	strcpy_x(f.ExchangeID, co.exchange_id.c_str());
	strcpy_x(f.InstrumentID, co.instrument_id.c_str());

	ENeedCancelOrderType needCancelOrderType = ENeedCancelOrderType::not_need;

	//平仓
	f.CombOffsetFlag[0] = THOST_FTDC_OF_Close;

	//买平
	if (EOrderDirection::buy == co.direction)
	{
		f.Direction = THOST_FTDC_D_Buy;
		Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

		//数量类型
		if (EVolumeType::num == co.volume_type)
		{
			//要平的手数小于等于可平手数,不需要撤单
			if (co.volume <= pos.pos_short_his+ pos.pos_short_today - pos.volume_short_frozen)
			{
				f.VolumeTotalOriginal = co.volume;
				f.VolumeCondition = THOST_FTDC_VC_AV;
			}
			//要平的手数小于等于可平手数(包括冻结的手数),需要撤掉所有挂单
			else if (co.volume <= pos.pos_short_his + pos.pos_short_today)
			{
				if (order.is_cancel_ori_close_order)
				{
					f.VolumeTotalOriginal = co.volume;
					f.VolumeCondition = THOST_FTDC_VC_AV;
					needCancelOrderType = ENeedCancelOrderType::all_buy;
				}
				else
				{
					Log().WithField("fun","ConditionOrder_Close")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithField("exchange_id", co.exchange_id)
						.WithField("instrument_id",co.instrument_id)
						.Log(LOG_WARNING,"can close short is less than will close short");

					return false;
				}
			}
			//仍然发出平仓指今以应对组合持仓的情况
			else if (co.volume>0)
			{
				f.VolumeTotalOriginal = co.volume;
				f.VolumeCondition = THOST_FTDC_VC_AV;
				needCancelOrderType = ENeedCancelOrderType::all_buy;
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name",_req_login.user_name)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id",co.instrument_id)
					.Log(LOG_WARNING,"can close short is less than will close short");				
				return false;
			}
		}
		else if (EVolumeType::close_all == co.volume_type)
		{
			//如果是组合持仓,且用了close_all指令,这里没法处理

			//先要撤掉所有平仓挂单
			needCancelOrderType = ENeedCancelOrderType::all_buy;			
			//如果有持仓(包括冻结的持仓),全部平掉
			if (pos.pos_short_his + pos.pos_short_today > 0)
			{
				f.VolumeTotalOriginal = pos.pos_short_his + pos.pos_short_today;
				f.VolumeCondition = THOST_FTDC_VC_AV;
			}
			else
			{
				Log().WithField("fun", "ConditionOrder_Close")
					.WithField("key", _key)
					.WithField("bid", _req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_WARNING, "have no need close short because of short position is zero");
				return false;
			}			
		}
	}
	//卖平
	else
	{
		f.Direction = THOST_FTDC_D_Sell;
		Position& pos = GetPosition(co.exchange_id,co.instrument_id,_req_login.user_name);

		//数量类型
		if (EVolumeType::num == co.volume_type)
		{
			//要平的手数小于等于可平的手数,不需要撤单
			if (co.volume <= pos.pos_long_his+ pos.pos_long_today - pos.volume_long_frozen)
			{
				f.VolumeTotalOriginal = co.volume;
				f.VolumeCondition = THOST_FTDC_VC_AV;
			}
			//要平的手数小于等于可平手数(包括冻结的手数),需要撤掉所有挂单
			else if (co.volume <= pos.pos_long_his + pos.pos_long_today)
			{
				if (order.is_cancel_ori_close_order)
				{
					f.VolumeTotalOriginal = co.volume;
					f.VolumeCondition = THOST_FTDC_VC_AV;
					needCancelOrderType = ENeedCancelOrderType::all_sell;
				}
				else
				{
					Log().WithField("fun","ConditionOrder_Close")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithField("exchange_id", co.exchange_id)
						.WithField("instrument_id",co.instrument_id)
						.Log(LOG_WARNING,"can close long is less than will close long");				
					return false;
				}
			}
			//仍然发出平仓指今以应对组合持仓的情况
			else if (co.volume > 0)
			{
				f.VolumeTotalOriginal = co.volume;
				f.VolumeCondition = THOST_FTDC_VC_AV;
				needCancelOrderType = ENeedCancelOrderType::all_sell;
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name",_req_login.user_name)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id",co.instrument_id)
					.Log(LOG_WARNING,"can close long is less than will close long");

				return false;
			}
		}
		else if (EVolumeType::close_all == co.volume_type)
		{
			//如果是组合持仓,且用了close_all指令,这里没法处理

			//先要撤掉所有平仓挂单
			needCancelOrderType = ENeedCancelOrderType::all_sell;
			
			//如果有持仓(包括冻结的持仓),全部平掉
			if (pos.pos_long_his + pos.pos_long_today > 0)
			{
				f.VolumeTotalOriginal = pos.pos_long_his + pos.pos_long_today;
				f.VolumeCondition = THOST_FTDC_VC_AV;
			}
			else
			{
				Log().WithField("fun","ConditionOrder_Close")
					.WithField("key",_key)
					.WithField("bid",_req_login.bid)
					.WithField("user_name", _req_login.user_name)
					.WithField("exchange_id", co.exchange_id)
					.WithField("instrument_id", co.instrument_id)
					.Log(LOG_WARNING,"have no need close long because of long position is zero");
				return false;
			}			
		}
	}

	//价格类型

	//限价
	if (EPriceType::limit == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		f.LimitPrice = co.limit_price;
	}
	//触发价
	else if (EPriceType::contingent == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;

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
		f.LimitPrice = last_price;		
	}
	//对价
	else if (EPriceType::consideration == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.ask_price1;
		}
		//卖平
		else
		{
			f.LimitPrice = ins.bid_price1;
		}
	}
	//市价
	else if (EPriceType::market == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_IOC;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.upper_limit;
		}
		//卖平
		else
		{
			f.LimitPrice = ins.lower_limit;
		}
	}
	//超价
	else if (EPriceType::over == co.price_type)
	{
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_GFD;
		//买平
		if (EOrderDirection::buy == co.direction)
		{
			f.LimitPrice = ins.ask_price1 + ins.price_tick;
			if (f.LimitPrice > ins.upper_limit)
			{
				f.LimitPrice = ins.upper_limit;
			}
		}
		//卖平
		else
		{
			f.LimitPrice = ins.bid_price1 - ins.price_tick;
			if (f.LimitPrice < ins.lower_limit)
			{
				f.LimitPrice = ins.lower_limit;
			}
		}
	}

	f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	f.ContingentCondition = THOST_FTDC_CC_Immediately;
	f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

	if (f.VolumeTotalOriginal > 0)
	{
		task.has_first_orders_to_send = true;
		CtpActionInsertOrder actionInsertOrder;
		actionInsertOrder.local_key.user_id = _req_login.user_name;
		actionInsertOrder.local_key.order_id = order.order_id + "_close_" + std::to_string(nOrderIndex);
		actionInsertOrder.f = f;
		task.first_orders_to_send.push_back(actionInsertOrder);
	}	
	
	task.has_second_orders_to_send = false;

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
				&& ((order.offset == kOffsetClose)||(order.offset == kOffsetCloseToday)))
			{
				CtpActionCancelOrder cancelOrder;
				cancelOrder.local_key.order_id = orderId;
				cancelOrder.local_key.user_id = _req_login.user_name.c_str();

				CThostFtdcInputOrderActionField f;
				memset(&f, 0, sizeof(CThostFtdcInputOrderActionField));
				strcpy_x(f.BrokerID, m_broker_id.c_str());
				strcpy_x(f.UserID, _req_login.user_name.c_str());
				strcpy_x(f.InvestorID, _req_login.user_name.c_str());
				strcpy_x(f.ExchangeID, order.exchange_id.c_str());
				strcpy_x(f.InstrumentID, order.instrument_id.c_str());

				cancelOrder.f = f;

				task.has_order_to_cancel = true;
				task.orders_to_cancel.push_back(cancelOrder);

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
				CtpActionCancelOrder cancelOrder;
				cancelOrder.local_key.order_id = orderId;
				cancelOrder.local_key.user_id = _req_login.user_name.c_str();

				CThostFtdcInputOrderActionField f;
				memset(&f, 0, sizeof(CThostFtdcInputOrderActionField));
				strcpy_x(f.BrokerID, m_broker_id.c_str());
				strcpy_x(f.UserID, _req_login.user_name.c_str());
				strcpy_x(f.InvestorID, _req_login.user_name.c_str());
				strcpy_x(f.ExchangeID, order.exchange_id.c_str());
				strcpy_x(f.InstrumentID, order.instrument_id.c_str());

				cancelOrder.f = f;

				task.has_order_to_cancel = true;
				task.orders_to_cancel.push_back(cancelOrder);
			}
		}
	}
	else
	{
		task.has_order_to_cancel = false;
	}
	
	return true;
}

bool traderctp::ConditionOrder_Reverse_Long(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
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
			CtpActionCancelOrder cancelOrder;
			cancelOrder.local_key.order_id = orderId;
			cancelOrder.local_key.user_id = _req_login.user_name.c_str();

			CThostFtdcInputOrderActionField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderActionField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, order.exchange_id.c_str());
			strcpy_x(f.InstrumentID, order.instrument_id.c_str());

			cancelOrder.f = f;

			task.has_order_to_cancel = true;
			task.orders_to_cancel.push_back(cancelOrder);
		}
	}

	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);

	//重新生成平多单
	
	//如果分今昨
	if (b_has_td_yd_distinct)
	{
		//如果有昨仓
		if (pos.pos_long_his > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平仓
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			//数量
			f.VolumeTotalOriginal = pos.pos_long_his;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			//价格类型(反手一定用市价平仓)
			f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			f.TimeCondition = THOST_FTDC_TC_IOC;
			f.LimitPrice = ins.lower_limit;

			f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			f.ContingentCondition = THOST_FTDC_CC_Immediately;
			f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

			task.has_first_orders_to_send = true;
			CtpActionInsertOrder actionInsertOrder;
			actionInsertOrder.local_key.user_id = _req_login.user_name;
			actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
			actionInsertOrder.f = f;
			task.first_orders_to_send.push_back(actionInsertOrder);
		}

		if (pos.pos_long_today > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平仓
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			//数量
			f.VolumeTotalOriginal = pos.pos_long_today;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			//价格类型(反手一定用市价平仓)
			f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			f.TimeCondition = THOST_FTDC_TC_IOC;
			f.LimitPrice = ins.lower_limit;

			f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			f.ContingentCondition = THOST_FTDC_CC_Immediately;
			f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

			task.has_first_orders_to_send = true;
			CtpActionInsertOrder actionInsertOrder;
			actionInsertOrder.local_key.user_id = _req_login.user_name;
			actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
			actionInsertOrder.f = f;
			task.first_orders_to_send.push_back(actionInsertOrder);
		}
	}
	//如果不分今昨
	else
	{
		//如果有多仓
		int volume_long = pos.pos_long_today + pos.pos_long_his;
		if (volume_long > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平仓
			f.CombOffsetFlag[0] = THOST_FTDC_OF_Close;

			//卖平
			f.Direction = THOST_FTDC_D_Sell;

			//数量
			f.VolumeTotalOriginal = volume_long;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			//价格类型(反手一定用市价平仓)
			f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			f.TimeCondition = THOST_FTDC_TC_IOC;
			f.LimitPrice = ins.lower_limit;

			f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			f.ContingentCondition = THOST_FTDC_CC_Immediately;
			f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

			task.has_first_orders_to_send = true;
			CtpActionInsertOrder actionInsertOrder;
			actionInsertOrder.local_key.user_id = _req_login.user_name;
			actionInsertOrder.local_key.order_id = order.order_id + "_close_" + std::to_string(nOrderIndex);
			actionInsertOrder.f = f;
			task.first_orders_to_send.push_back(actionInsertOrder);
		}	
	}

	//生成开空单	
	//如果有多仓
	int volume_long = pos.pos_long_today + pos.pos_long_his;
	if (volume_long > 0)
	{
		CThostFtdcInputOrderField f;
		memset(&f, 0, sizeof(CThostFtdcInputOrderField));
		strcpy_x(f.BrokerID, m_broker_id.c_str());
		strcpy_x(f.UserID, _req_login.user_name.c_str());
		strcpy_x(f.InvestorID, _req_login.user_name.c_str());
		strcpy_x(f.ExchangeID, co.exchange_id.c_str());
		strcpy_x(f.InstrumentID, co.instrument_id.c_str());

		//开仓
		f.CombOffsetFlag[0] = THOST_FTDC_OF_Open;

		//开空
		f.Direction = THOST_FTDC_D_Sell;

		//数量
		f.VolumeTotalOriginal = volume_long;
		f.VolumeCondition = THOST_FTDC_VC_AV;

		//价格(反手一定用市价开仓)
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_IOC;
		f.LimitPrice = ins.lower_limit;

		f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		f.ContingentCondition = THOST_FTDC_CC_Immediately;
		f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

		CtpActionInsertOrder actionInsertOrder;
		actionInsertOrder.local_key.user_id = _req_login.user_name;
		actionInsertOrder.local_key.order_id = order.order_id + "_open_" + std::to_string(nOrderIndex);
		actionInsertOrder.f = f;

		task.has_second_orders_to_send = true;
		task.second_orders_to_send.push_back(actionInsertOrder);
	}
	
	return true;
}

bool traderctp::ConditionOrder_Reverse_Short(const ConditionOrder& order
	, const ContingentOrder& co
	, const Instrument& ins
	, ctp_condition_order_task& task
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
			CtpActionCancelOrder cancelOrder;
			cancelOrder.local_key.order_id = orderId;
			cancelOrder.local_key.user_id = _req_login.user_name.c_str();

			CThostFtdcInputOrderActionField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderActionField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, order.exchange_id.c_str());
			strcpy_x(f.InstrumentID, order.instrument_id.c_str());

			cancelOrder.f = f;

			task.has_order_to_cancel = true;
			task.orders_to_cancel.push_back(cancelOrder);
		}

	}

	//重新生成平空单
	Position& pos = GetPosition(co.exchange_id, co.instrument_id, _req_login.user_name);
	//如果分今昨
	if (b_has_td_yd_distinct)
	{
		//如果有昨仓
		if (pos.pos_short_his > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平仓
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseYesterday;

			//买平
			f.Direction = THOST_FTDC_D_Buy;

			//数量
			f.VolumeTotalOriginal = pos.pos_short_his;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			//价格类型(反手一定用市价平仓)
			f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			f.TimeCondition = THOST_FTDC_TC_IOC;
			f.LimitPrice = ins.upper_limit;

			f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			f.ContingentCondition = THOST_FTDC_CC_Immediately;
			f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

			task.has_first_orders_to_send = true;
			CtpActionInsertOrder actionInsertOrder;
			actionInsertOrder.local_key.user_id = _req_login.user_name;
			actionInsertOrder.local_key.order_id = order.order_id + "_closeyestoday_" + std::to_string(nOrderIndex);
			actionInsertOrder.f = f;
			task.first_orders_to_send.push_back(actionInsertOrder);
		}

		//如果有今仓
		if (pos.pos_short_today > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平仓
			f.CombOffsetFlag[0] = THOST_FTDC_OF_CloseToday;

			//买平
			f.Direction = THOST_FTDC_D_Buy;

			//数量
			f.VolumeTotalOriginal = pos.pos_short_today;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			//价格类型(反手一定用市价平仓)
			f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			f.TimeCondition = THOST_FTDC_TC_IOC;
			f.LimitPrice = ins.upper_limit;

			f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			f.ContingentCondition = THOST_FTDC_CC_Immediately;
			f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

			task.has_first_orders_to_send = true;
			CtpActionInsertOrder actionInsertOrder;
			actionInsertOrder.local_key.user_id = _req_login.user_name;
			actionInsertOrder.local_key.order_id = order.order_id + "_closetoday_" + std::to_string(nOrderIndex);
			actionInsertOrder.f = f;
			task.first_orders_to_send.push_back(actionInsertOrder);
		}
	}
	//如果不分今昨
	else
	{
		//如果有空仓
		int volume_short = pos.pos_short_today + pos.pos_short_his;
		if (volume_short > 0)
		{
			CThostFtdcInputOrderField f;
			memset(&f, 0, sizeof(CThostFtdcInputOrderField));
			strcpy_x(f.BrokerID, m_broker_id.c_str());
			strcpy_x(f.UserID, _req_login.user_name.c_str());
			strcpy_x(f.InvestorID, _req_login.user_name.c_str());
			strcpy_x(f.ExchangeID, co.exchange_id.c_str());
			strcpy_x(f.InstrumentID, co.instrument_id.c_str());

			//平仓
			f.CombOffsetFlag[0] = THOST_FTDC_OF_Close;

			//买平
			f.Direction = THOST_FTDC_D_Buy;

			//数量
			f.VolumeTotalOriginal = volume_short;
			f.VolumeCondition = THOST_FTDC_VC_AV;

			//价格类型(反手一定用市价平仓)
			f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
			f.TimeCondition = THOST_FTDC_TC_IOC;
			f.LimitPrice = ins.upper_limit;

			f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
			f.ContingentCondition = THOST_FTDC_CC_Immediately;
			f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

			task.has_first_orders_to_send = true;
			CtpActionInsertOrder actionInsertOrder;
			actionInsertOrder.local_key.user_id = _req_login.user_name;
			actionInsertOrder.local_key.order_id = order.order_id + "_close_" + std::to_string(nOrderIndex);
			actionInsertOrder.f = f;
			task.first_orders_to_send.push_back(actionInsertOrder);
		}
	}

	//生成开多单	

	//如果有空仓
	int volume_short = pos.pos_short_today + pos.pos_short_his;
	if (volume_short > 0)
	{
		CThostFtdcInputOrderField f;
		memset(&f, 0, sizeof(CThostFtdcInputOrderField));
		strcpy_x(f.BrokerID, m_broker_id.c_str());
		strcpy_x(f.UserID, _req_login.user_name.c_str());
		strcpy_x(f.InvestorID, _req_login.user_name.c_str());
		strcpy_x(f.ExchangeID, co.exchange_id.c_str());
		strcpy_x(f.InstrumentID, co.instrument_id.c_str());

		//开仓
		f.CombOffsetFlag[0] = THOST_FTDC_OF_Open;

		//开空
		f.Direction = THOST_FTDC_D_Buy;

		//数量
		f.VolumeTotalOriginal = volume_short;
		f.VolumeCondition = THOST_FTDC_VC_AV;

		//价格(反手一定用市价开仓)
		f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		f.TimeCondition = THOST_FTDC_TC_IOC;
		f.LimitPrice = ins.upper_limit;

		f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		f.ContingentCondition = THOST_FTDC_CC_Immediately;
		f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;

		CtpActionInsertOrder actionInsertOrder;
		actionInsertOrder.local_key.user_id = _req_login.user_name;
		actionInsertOrder.local_key.order_id = order.order_id + "_open_" + std::to_string(nOrderIndex);
		actionInsertOrder.f = f;

		task.has_second_orders_to_send = true;
		task.second_orders_to_send.push_back(actionInsertOrder);
	}

	return true;
}

void traderctp::OnTouchConditionOrder(const ConditionOrder& order)
{
	SerializerConditionOrderData ss;
	ss.FromVar(order);
	std::string strMsg;
	ss.ToString(&strMsg);

	Log().WithField("fun","OnTouchConditionOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("order_id",order.order_id)
		.WithPack("co_pack",strMsg)
		.Log(LOG_INFO,"condition order is touched");

	int nOrderIndex = 0;
	for (const ContingentOrder& co : order.order_list)
	{
		nOrderIndex++;
		ctp_condition_order_task task;
		task.condition_order = order;

		std::string symbol = co.exchange_id + "." + co.instrument_id;
		Instrument* ins = GetInstrument(symbol);
		if (nullptr==ins)
		{
			Log().WithField("fun","OnTouchConditionOrder")
				.WithField("key",_key)
				.WithField("bid",_req_login.bid)
				.WithField("user_name",_req_login.user_name)
				.WithField("exchange_id",co.exchange_id)
				.WithField("instrument_id",co.instrument_id)
				.Log(LOG_WARNING,"instrument not exist");			
			continue;
		}
		
		bool flag = false;
		//如果是开仓
		if (EOrderOffset::open == co.offset)
		{
			flag=ConditionOrder_Open(order,co,*ins,task,nOrderIndex);				
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
							//平今优先，如果可平不足需要撤原平仓单
							flag = ConditionOrder_CloseTodayPrior_NeedCancel(order, co, *ins, task, nOrderIndex);
						}
						else
						{
							//平今优先,如果可平不足不需要撤原平仓单
							flag = ConditionOrder_CloseTodayPrior_NotNeedCancel(order, co, *ins, task, nOrderIndex);
						}
					}
					//如果是平昨优先
					else
					{
						if (order.is_cancel_ori_close_order)
						{
							//平昨优先,如果可平不足需要撤原平仓单
							flag = ConditionOrder_CloseYesTodayPrior_NeedCancel(order, co, *ins, task, nOrderIndex);
						}
						else
						{
							//平昨优先,如果可平不足不需要撤原平仓单
							flag = ConditionOrder_CloseYesTodayPrior_NotNeedCancel(order, co, *ins, task, nOrderIndex);
						}
					}
				}
				//如果是全平
				else if (EVolumeType::close_all == co.volume_type)
				{
					//先撤掉所有平仓挂单,再平掉所有今昨仓
					flag = ConditionOrder_CloseAll(order, co, *ins, task, nOrderIndex);
				}						
			}
			else
			{
				//不分今昨的平仓				
				flag = ConditionOrder_Close(order,co,*ins,task,nOrderIndex);
			}			
		}		
		else if (EOrderOffset::reverse == co.offset)
		{
			//对空头进行反手操作
			if (co.direction == EOrderDirection::buy)
			{
				flag = ConditionOrder_Reverse_Short(order, co, *ins, task, nOrderIndex);
			}
			//对多头进行反手操作
			else if (co.direction == EOrderDirection::sell)
			{
				flag = ConditionOrder_Reverse_Long(order, co, *ins, task, nOrderIndex);
			}
		}

		if (!flag)
		{
			continue;
		}

		//开始发单
		if (task.has_order_to_cancel)
		{			
			for (CtpActionCancelOrder& oc : task.orders_to_cancel)
			{
				OnConditionOrderReqCancelOrder(oc);
			}			
			m_condition_order_task.push_back(task);
			continue;
		}
		else if (task.has_first_orders_to_send)
		{			
			for (CtpActionInsertOrder& o : task.first_orders_to_send)
			{
				OnConditionOrderReqInsertOrder(o);
			}			
			m_condition_order_task.push_back(task);
			continue;
		}
		else if (task.has_second_orders_to_send)
		{			
			for (CtpActionInsertOrder& o : task.second_orders_to_send)
			{
				OnConditionOrderReqInsertOrder(o);
			}			
			m_condition_order_task.push_back(task);
			continue;
		}
	}	
}

void traderctp::PrintOrderLogIfTouchedByConditionOrder(const Order& order)
{
	bool flag = false;
	for (ctp_condition_order_task& task : m_condition_order_task)
	{
		if (task.has_order_to_cancel)
		{
			for (auto it = task.orders_to_cancel.begin(); it != task.orders_to_cancel.end(); it++)
			{
				if (it->local_key.order_id == order.order_id)
				{
					SerializerConditionOrderData ss1;
					ss1.FromVar(task.condition_order);
					std::string strMsg1;
					ss1.ToString(&strMsg1);

					SerializerTradeBase ss2;
					ss2.FromVar(order);
					std::string strMsg2;
					ss2.ToString(&strMsg2);

					Log().WithField("fun","PrintOrderLogIfTouchedByConditionOrder")
						.WithField("key",_key)
						.WithField("bid",_req_login.bid)
						.WithField("user_name",_req_login.user_name)
						.WithPack("order_pack",strMsg2)
						.WithPack("co_pack",strMsg1)						
						.Log(LOG_INFO, "order is touched by condition order");

					flag = true;
					break;
				}
			}
		}

		if (flag)
		{
			break;
		}

		if (task.has_first_orders_to_send)
		{
			for (auto it = task.first_orders_to_send.begin(); it != task.first_orders_to_send.end(); it++)
			{
				if (it->local_key.order_id == order.order_id)
				{
					SerializerConditionOrderData ss1;
					ss1.FromVar(task.condition_order);
					std::string strMsg1;
					ss1.ToString(&strMsg1);

					SerializerTradeBase ss2;
					ss2.FromVar(order);
					std::string strMsg2;
					ss2.ToString(&strMsg2);

					Log().WithField("fun", "PrintOrderLogIfTouchedByConditionOrder")
						.WithField("key", _key)
						.WithField("bid", _req_login.bid)
						.WithField("user_name", _req_login.user_name)
						.WithPack("order_pack", strMsg2)
						.WithPack("co_pack", strMsg1)
						.Log(LOG_INFO, "order is touched by condition order");

					flag = true;
					break;
				}
			}
		}

		if (flag)
		{
			break;
		}

		if (task.has_second_orders_to_send)
		{
			for (auto it = task.second_orders_to_send.begin(); it != task.second_orders_to_send.end(); it++)
			{
				if (it->local_key.order_id == order.order_id)
				{
					SerializerConditionOrderData ss1;
					ss1.FromVar(task.condition_order);
					std::string strMsg1;
					ss1.ToString(&strMsg1);

					SerializerTradeBase ss2;
					ss2.FromVar(order);
					std::string strMsg2;
					ss2.ToString(&strMsg2);

					Log().WithField("fun", "PrintOrderLogIfTouchedByConditionOrder")
						.WithField("key", _key)
						.WithField("bid", _req_login.bid)
						.WithField("user_name", _req_login.user_name)
						.WithPack("order_pack", strMsg2)
						.WithPack("co_pack", strMsg1)
						.Log(LOG_INFO, "order is touched by condition order");

					flag = true;
					break;
				}
			}
		}
	}
}

void traderctp::CheckConditionOrderCancelOrderTask(const std::string& orderId)
{
	for (ctp_condition_order_task& task : m_condition_order_task)
	{
		if (!task.has_order_to_cancel)
		{
			continue;
		}

		bool flag = false;
		for (auto it = task.orders_to_cancel.begin(); it != task.orders_to_cancel.end(); it++)
		{
			if (it->local_key.order_id == orderId)
			{
				task.orders_to_cancel.erase(it);
				flag = true;
				break;
			}			
		}

		if (flag)
		{
			//撤单已经完成
			if (task.orders_to_cancel.empty())
			{
				task.has_order_to_cancel = false;

				if (task.has_first_orders_to_send)
				{
					for (auto o : task.first_orders_to_send)
					{
						OnConditionOrderReqInsertOrder(o);
					}
				}
				else if (task.has_second_orders_to_send)
				{
					for (auto o : task.second_orders_to_send)
					{
						OnConditionOrderReqInsertOrder(o);
					}					
				}
			}

			break;
		}
	}	
}

void traderctp::CheckConditionOrderSendOrderTask(const std::string& orderId)
{
	for (ctp_condition_order_task& task : m_condition_order_task)
	{
		if (task.has_order_to_cancel)
		{
			continue;
		}

		if (task.has_first_orders_to_send)
		{
			bool flag = false;
			for (auto it = task.first_orders_to_send.begin(); it != task.first_orders_to_send.end(); it++)
			{
				if (it->local_key.order_id == orderId)
				{
					task.first_orders_to_send.erase(it);
					flag = true;
					break;
				}
			}

			if (flag)
			{
				//第一批Order已经成交
				if (task.first_orders_to_send.empty())
				{
					task.has_first_orders_to_send = false;

					//发送第二批Order
					if (task.has_second_orders_to_send)
					{
						for (auto o : task.second_orders_to_send)
						{
							OnConditionOrderReqInsertOrder(o);
						}
					}
					
				}


				break;
			}
			else
			{
				continue;
			}			
		}

		if (task.has_second_orders_to_send)
		{
			bool flag = false;
			for (auto it = task.second_orders_to_send.begin(); it != task.second_orders_to_send.end(); it++)
			{
				if (it->local_key.order_id == orderId)
				{
					task.second_orders_to_send.erase(it);
					flag = true;
					break;
				}
			}

			if (flag)
			{
				//第二批Order已经成交
				if (task.second_orders_to_send.empty())
				{
					task.has_second_orders_to_send = false;
				}
				break;
			}
			else
			{
				continue;
			}
		}

	}

	for (auto it = m_condition_order_task.begin(); it != m_condition_order_task.end();)
	{
		ctp_condition_order_task& task = *it;
		if (!task.has_order_to_cancel
			&& !task.has_first_orders_to_send
			&& !task.has_second_orders_to_send)
		{
			it = m_condition_order_task.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void traderctp::OnConditionOrderReqCancelOrder(CtpActionCancelOrder& d)
{
	if (d.local_key.order_id.empty())
	{
		Log().WithField("fun", "OnConditionOrderReqCancelOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("orderid", d.local_key.order_id)
			.Log(LOG_WARNING, "orderid is not exist");
		return;
	}

	auto it = m_ordermap_local_remote.find(d.local_key);
	if (it == m_ordermap_local_remote.end())
	{
		Log().WithField("fun", "OnConditionOrderReqCancelOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("orderid", d.local_key.order_id)
			.Log(LOG_WARNING, "orderid is not exist");
		return;
	}

	RemoteOrderKey rkey = it->second;
	
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
	
	if (nullptr == m_pTdApi)
	{
		OutputNotifyAllSycn(334,u8"当前时间不支持撤单!","WARNING");
		return;
	}
	int r = m_pTdApi->ReqOrderAction(&d.f, 0);
	if (0 != r)
	{
		OutputNotifyAllSycn(355,u8"撤单请求发送失败!","WARNING");
		Log().WithField("fun","OnConditionOrderReqCancelOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("orderid",d.local_key.order_id)
			.Log(LOG_WARNING,"cancel order request is fail");		
	}

	SerializerLogCtp nss;
	nss.FromVar(d.f);
	std::string strMsg = "";
	nss.ToString(&strMsg);

	Log().WithField("fun","OnConditionOrderReqCancelOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("user_id",d.local_key.user_id)
		.WithField("order_id",d.local_key.order_id)
		.WithField("FrontID",rkey.front_id)
		.WithField("SessionID",rkey.session_id)
		.WithField("OrderRef",rkey.order_ref)
		.WithField("ret",r)
		.WithPack("ctp_pack",strMsg)
		.Log(LOG_INFO,"send ReqOrderAction request!");
}

void traderctp::OnConditionOrderReqInsertOrder(CtpActionInsertOrder& d)
{	
	RemoteOrderKey rkey;
	rkey.exchange_id = d.f.ExchangeID;
	rkey.instrument_id = d.f.InstrumentID;
	rkey.session_id = m_session_id;
	rkey.front_id = m_front_id;
	rkey.order_ref = std::to_string(m_order_ref++);

	//客户端没有提供订单编号
	if (d.local_key.order_id.empty())
	{
		char buf[1024];
		sprintf(buf, "OTG.%s.%08x.%d"
			, rkey.order_ref.c_str()
			, rkey.session_id
			, rkey.front_id);
		d.local_key.order_id = buf;
	}

	//客户端报单编号已经存在
	auto it = m_ordermap_local_remote.find(d.local_key);
	if (it != m_ordermap_local_remote.end())
	{
		Log().WithField("fun", "OnConditionOrderReqInsertOrder")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("order_id", d.local_key.order_id)
			.Log(LOG_WARNING, "orderid is duplicate,can not send order");
		return;
	}
	   
	m_ordermap_local_remote[d.local_key] = rkey;
	m_ordermap_remote_local[rkey] = d.local_key;
	m_need_save_file.store(true);
		
	strcpy_x(d.f.OrderRef, rkey.order_ref.c_str());
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
	
	if (nullptr == m_pTdApi)
	{
		OutputNotifyAllSycn(333,u8"当前时间不支持下单!","WARNING");
		return;
	}
	int r = m_pTdApi->ReqOrderInsert(&d.f, 0);	
	if (0 != r)
	{		
		OutputNotifyAllSycn(358,u8"下单请求发送失败!","WARNING");
		Log().WithField("fun","OnConditionOrderReqInsertOrder")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("order_id",d.local_key.order_id)
			.Log(LOG_WARNING,"send order request is fail");
	}

	SerializerLogCtp nss;
	nss.FromVar(d.f);
	std::string strMsg = "";
	nss.ToString(&strMsg);

	Log().WithField("fun","OnClientReqInsertConditionOrder")
		.WithField("key",_key)
		.WithField("bid",_req_login.bid)
		.WithField("user_name",_req_login.user_name)
		.WithField("user_id",d.local_key.user_id)
		.WithField("order_id",d.local_key.order_id)
		.WithField("FrontID",rkey.front_id)
		.WithField("SessionID",rkey.session_id)
		.WithField("OrderRef",rkey.order_ref)
		.WithField("ret",r)
		.WithPack("ctp_pack",strMsg)
		.Log(LOG_INFO,"send ReqOrderInsert request!");	
}

void traderctp::SetExchangeTime(CThostFtdcRspUserLoginField& userLogInField)
{
	boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
	
	DateTime dtLocalTime;
	dtLocalTime.date.year= tm.date().year();
	dtLocalTime.date.month= tm.date().month();
	dtLocalTime.date.day= tm.date().day();
	dtLocalTime.time.hour = tm.time_of_day().hours();
	dtLocalTime.time.minute = tm.time_of_day().minutes();
	dtLocalTime.time.second = tm.time_of_day().seconds();
	dtLocalTime.time.microsecond = 0;
	
	DateTime dtSHFETime;
	dtSHFETime.date.year = tm.date().year();
	dtSHFETime.date.month = tm.date().month();
	dtSHFETime.date.day = tm.date().day();
	GetTimeFromString(userLogInField.SHFETime,dtSHFETime.time);
	if ((dtSHFETime.time.hour == 0)
		&& (dtSHFETime.time.minute == 0)
		&& (dtSHFETime.time.second == 0))
	{
		dtSHFETime.time.hour = dtLocalTime.time.hour;
		dtSHFETime.time.minute = dtLocalTime.time.minute;
		dtSHFETime.time.second = dtLocalTime.time.second;
		dtSHFETime.time.microsecond = 0;
	}
	
	DateTime dtDCETime;
	dtDCETime.date.year = tm.date().year();
	dtDCETime.date.month = tm.date().month();
	dtDCETime.date.day = tm.date().day();
	GetTimeFromString(userLogInField.DCETime, dtDCETime.time);
	if ((dtDCETime.time.hour == 0)
		&& (dtDCETime.time.minute == 0)
		&& (dtDCETime.time.second == 0))
	{
		dtDCETime.time.hour = dtLocalTime.time.hour;
		dtDCETime.time.minute = dtLocalTime.time.minute;
		dtDCETime.time.second = dtLocalTime.time.second;
		dtDCETime.time.microsecond = 0;
	}

	DateTime dtINETime;
	dtINETime.date.year = tm.date().year();
	dtINETime.date.month = tm.date().month();
	dtINETime.date.day = tm.date().day();
	GetTimeFromString(userLogInField.INETime, dtINETime.time);
	if ((dtINETime.time.hour == 0)
		&& (dtINETime.time.minute == 0)
		&& (dtINETime.time.second == 0))
	{
		dtINETime.time.hour = dtLocalTime.time.hour;
		dtINETime.time.minute = dtLocalTime.time.minute;
		dtINETime.time.second = dtLocalTime.time.second;
		dtINETime.time.microsecond = 0;
	}

	DateTime dtFFEXTime;
	dtFFEXTime.date.year = tm.date().year();
	dtFFEXTime.date.month = tm.date().month();
	dtFFEXTime.date.day = tm.date().day();
	GetTimeFromString(userLogInField.FFEXTime, dtFFEXTime.time);
	if ((dtFFEXTime.time.hour == 0)
		&& (dtFFEXTime.time.minute == 0)
		&& (dtFFEXTime.time.second == 0))
	{
		dtFFEXTime.time.hour = dtLocalTime.time.hour;
		dtFFEXTime.time.minute = dtLocalTime.time.minute;
		dtFFEXTime.time.second = dtLocalTime.time.second;
		dtFFEXTime.time.microsecond = 0;
	}

	DateTime dtCZCETime;
	dtCZCETime.date.year = tm.date().year();
	dtCZCETime.date.month = tm.date().month();
	dtCZCETime.date.day = tm.date().day();
	GetTimeFromString(userLogInField.CZCETime, dtCZCETime.time);
	if ((dtCZCETime.time.hour == 0)
		&& (dtCZCETime.time.minute == 0)
		&& (dtCZCETime.time.second == 0))
	{
		dtCZCETime.time.hour = dtLocalTime.time.hour;
		dtCZCETime.time.minute = dtLocalTime.time.minute;
		dtCZCETime.time.second = dtLocalTime.time.second;
		dtCZCETime.time.microsecond = 0;
	}

	m_condition_order_manager.SetExchangeTime(DateTimeToEpochSeconds(dtLocalTime),
		DateTimeToEpochSeconds(dtSHFETime),
		DateTimeToEpochSeconds(dtDCETime),
		DateTimeToEpochSeconds(dtINETime),
		DateTimeToEpochSeconds(dtFFEXTime),
		DateTimeToEpochSeconds(dtCZCETime));
}

void traderctp::SendDataDirect(int connId, const std::string& msg)
{
	std::shared_ptr<std::string> msg_ptr(new std::string(msg));
	_ios.post(boost::bind(&traderctp::SendMsg,this,connId,msg_ptr));
}

#pragma endregion

#pragma region ctpse

int traderctp::RegSystemInfo()
{
	CThostFtdcUserSystemInfoField f;
	memset(&f, 0, sizeof(f));
	strcpy_x(f.BrokerID, _req_login.broker.ctp_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	std::string client_system_info = base64_decode(_req_login.client_system_info);
	memcpy(f.ClientSystemInfo, client_system_info.c_str(), client_system_info.length());
	f.ClientSystemInfoLen = client_system_info.length();
	///用户公网IP
	strcpy_x(f.ClientPublicIP, _req_login.client_ip.c_str());
	///终端IP端口
	f.ClientIPPort = _req_login.client_port;
	///登录成功时间
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	snprintf(f.ClientLoginTime, 9, "%02d:%02d:%02d", now.time_of_day().hours()
		, now.time_of_day().minutes(), now.time_of_day().seconds());
	///App代码
	strcpy_x(f.ClientAppID, _req_login.client_app_id.c_str());

	int ret = m_pTdApi->RegisterUserSystemInfo(&f);

	Log().WithField("fun", "RegSystemInfo")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("client_login_time", f.ClientLoginTime)
		.WithField("client_ip", _req_login.client_ip)
		.WithField("client_port", _req_login.client_port)
		.WithField("client_app_id", _req_login.client_app_id)
		.WithField("client_system_info_length", (int)client_system_info.length())
		.WithField("ret", ret)
		.Log(LOG_INFO, "ctp RegisterUserSystemInfo");
	return ret;
}

int traderctp::ReqAuthenticate()
{
	if (m_try_req_authenticate_times > 0)
	{
		int nSeconds = 10 + m_try_req_authenticate_times * 1;
		if (nSeconds > 60)
		{
			nSeconds = 60;
		}
		boost::this_thread::sleep_for(boost::chrono::seconds(nSeconds));
	}

	m_try_req_authenticate_times++;
	if (_req_login.broker.auth_code.empty())
	{
		Log().WithField("fun", "ReqAuthenticate")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.Log(LOG_INFO, "auth_code is empty");
		SendLoginRequest();
		return 0;
	}

	CThostFtdcReqAuthenticateField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.UserID, _req_login.user_name.c_str());
	strcpy_x(field.UserProductInfo, USER_PRODUCT_INFO_NAME.c_str());
	strcpy_x(field.AppID, _req_login.broker.product_info.c_str());
	strcpy_x(field.AuthCode, _req_login.broker.auth_code.c_str());
	int ret = m_pTdApi->ReqAuthenticate(&field, ++_requestID);
	Log().WithField("fun", "ReqAuthenticate")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("product_info", USER_PRODUCT_INFO_NAME)
		.WithField("app_id", _req_login.broker.product_info)
		.WithField("auth_code", _req_login.broker.auth_code)
		.WithField("ret", ret)
		.Log(LOG_INFO, "ctp ReqAuthenticate");
	return ret;
}

#pragma endregion
