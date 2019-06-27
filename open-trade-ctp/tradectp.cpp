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

#include <fstream>
#include <functional>
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

static std::string GuessExchangeId(std::string instrument_id)
{
	if (instrument_id.size() > 11) 
	{
		//组合
		if ((instrument_id[0] == 'S' && instrument_id[1] == 'P' && instrument_id[2] == 'D')
			|| (instrument_id[0] == 'I' && instrument_id[1] == 'P' && instrument_id[2] == 'S')
			)
			return "CZCE";
		else
			return "DCE";
	}
	if (instrument_id.size() > 8
		&& instrument_id[0] == 'm'
		&& instrument_id[5] == '-'
		&& (instrument_id[6] == 'C' || instrument_id[6] == 'P')
		&& instrument_id[7] == '-'
		) 
	{
		//大连豆粕期权
		//"^DCE\.m(\d\d)(\d\d)-([CP])-(\d+)$"
		return "DCE";
	}
	if (instrument_id.size() > 8
		&& instrument_id[0] == 'c'
		&& instrument_id[5] == '-'
		&& (instrument_id[6] == 'C' || instrument_id[6] == 'P')
		&& instrument_id[7] == '-'
		)
	{
		//大连玉米期权
		//"^DCE\.c(\d\d)(\d\d)-([CP])-(\d+)$"
		return "DCE";
	}
	if (instrument_id.size() > 7
		&& instrument_id[0] == 'c'
		&& instrument_id[1] == 'u'
		&& (instrument_id[6] == 'C' || instrument_id[6] == 'P')
		) {
		//上海铜期权
		//"^SHFE\.cu(\d\d)(\d\d)([CP])(\d+)$"
		return "SHFE";
	}
	if (instrument_id.size() > 7
		&& instrument_id[0] == 'r'
		&& instrument_id[1] == 'u'
		&& (instrument_id[6] == 'C' || instrument_id[6] == 'P')
		) {
		//上海橡胶期权
		//"^SHFE\.ru(\d\d)(\d\d)([CP])(\d+)$"
		return "SHFE";
	}
	if (instrument_id.size() > 6
		&& instrument_id[0] == 'S'
		&& instrument_id[1] == 'R'
		&& (instrument_id[5] == 'C' || instrument_id[5] == 'P')
		) {
		//郑州白糖期权
		//"CZCE\.SR(\d)(\d\d)([CP])(\d+)"
		return "CZCE";
	}
	if (instrument_id.size() > 6
		&& instrument_id[0] == 'C'
		&& instrument_id[1] == 'F'
		&& (instrument_id[5] == 'C' || instrument_id[5] == 'P')
		) {
		//郑州棉花期权
		//"CZCE\.CF(\d)(\d\d)([CP])(\d+)"
		return "CZCE";
	}
	if (instrument_id.size() == 5
		&& instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
		&& instrument_id[1] >= 'A' && instrument_id[1] <= 'Z'
		) {
		//郑州期货
		return "CZCE";
	}
	if (instrument_id.size() == 5
		&& instrument_id[0] >= 'a' && instrument_id[0] <= 'z'
		&& instrument_id[1] >= '0' && instrument_id[1] <= '9'
		) {
		//大连期货
		return "DCE";
	}
	if (instrument_id.size() == 5
		&& instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
		) {
		//中金期货
		return "CFFEX";
	}
	if (instrument_id.size() == 6
		&& instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
		&& instrument_id[1] >= 'A' && instrument_id[1] <= 'Z'
		) {
		//中金期货
		return "CFFEX";
	}
	if (instrument_id.size() == 6
		&& instrument_id[0] >= 'a' && instrument_id[0] <= 'z'
		&& instrument_id[1] >= 'a' && instrument_id[1] <= 'z'
		) {
		if ((instrument_id[0] == 'c' && instrument_id[1] == 's')
			|| (instrument_id[0] == 'f' && instrument_id[1] == 'b')
			|| (instrument_id[0] == 'b' && instrument_id[1] == 'b')
			|| (instrument_id[0] == 'j' && instrument_id[1] == 'd')
			|| (instrument_id[0] == 'p' && instrument_id[1] == 'p')
			|| (instrument_id[0] == 'e' && instrument_id[1] == 'g')
			|| (instrument_id[0] == 'j' && instrument_id[1] == 'm'))
			return "DCE";
		if (instrument_id[0] == 's' && instrument_id[1] == 'c')
			return "INE";
		return "SHFE";
	}
	return "UNKNOWN";
}

traderctp::traderctp(boost::asio::io_context& ios
	, const std::string& key)
	:m_b_login(false)
	, _key(key)
	, m_settlement_info("")
	, _ios(ios)
	, _out_mq_ptr()
	, _out_mq_name(_key + "_msg_out")
	, _in_mq_ptr()
	, _in_mq_name(_key + "_msg_in")
	, _thread_ptr()
	, m_notify_seq(0)
	, m_data_seq(0)
	, _req_login()
	, m_broker_id("")
	, m_pTdApi(NULL)
	, m_trading_day("")
	, m_front_id(0)
	, m_session_id(0)
	, m_order_ref(0)
	, m_input_order_key_map()
	, m_action_order_map()
	, m_req_transfer_list()
	, _logIn_status(0)
	, _logInmutex()
	, _logInCondition()
	, m_loging_connectId(-1)
	, m_logined_connIds()
	, m_user_file_path("")
	, m_ordermap_local_remote()
	, m_ordermap_remote_local()
	, m_data()
	, m_Algorithm_Type(THOST_FTDC_AG_None)
	, m_banks()
	, m_try_req_authenticate_times(0)
	, m_try_req_login_times(0)
	, m_run_receive_msg(false)
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
	ReqAuthenticate();
	OutputNotifyAllSycn(0, u8"已经重新连接到交易前置");
}

void traderctp::OnFrontConnected()
{
	Log.WithField("fun", "OnFrontConnected").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_INFO);
	//还在等待登录阶段
	if (!m_b_login.load())
	{
		//这时是安全的
		ReqAuthenticate();		
		OutputNotifySycn(m_loging_connectId, 0, u8"已经连接到交易前置");
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
	OutputNotifyAllSycn(1, u8"已经断开与交易前置的连接");
}

void traderctp::OnFrontDisconnected(int nReason)
{
	Log.WithField("fun", "OnFrontDisconnected").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("nReason", nReason).Write(LOG_WARNING);
	//还在等待登录阶段
	if (!m_b_login.load())
	{		
		OutputNotifySycn(m_loging_connectId, 1, u8"已经断开与交易前置的连接");
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
			_ios.post(boost::bind(&traderctp::ReinitCtp, this));
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
		Log.WithField("fun", "OnRspAuthenticate").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast?"true":"false").WithField("pack", strMsg.c_str()).Write(LOG_WARNING);
	}
	else
	{
		Log.WithField("fun", "OnRspAuthenticate").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_WARNING);
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
			_logIn_status = 0;
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
			u8"交易服务器重登录失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
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
			m_insert_order_set.clear();
			m_cancel_order_set.clear();

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

			m_notify_seq = 0;
			m_data_seq = 0;
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

			m_is_qry_his_settlement_info.store(false);
			m_his_settlement_info = "";
			m_qry_his_settlement_info_trading_days.clear();

			AfterLogin();
		}
		else
		{
			//正常的断开重连成功
			m_front_id = pRspUserLogin->FrontID;
			m_session_id = pRspUserLogin->SessionID;
			m_order_ref = atoi(pRspUserLogin->MaxOrderRef);
			OutputNotifyAllSycn(0, u8"交易服务器重登录成功");

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

		Log.WithField("fun", "OnRspUserLogin").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspUserLogin").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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
				u8"交易服务器登录失败," + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			if ((pRspInfo->ErrorID == 140)
				|| (pRspInfo->ErrorID == 131)
				|| (pRspInfo->ErrorID == 141))
			{
				_logIn_status = 1;
			}
			else
			{
				_logIn_status = 0;
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
			OutputNotifySycn(m_loging_connectId, 0, u8"登录成功");
			AfterLogin();
			boost::unique_lock<boost::mutex> lock(_logInmutex);
			_logIn_status = 2;
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

		Log.WithField("fun", "OnRspQrySettlementInfoConfirm").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQrySettlementInfoConfirm").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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
				OutputNotifyAllSycn(0, m_settlement_info, "INFO", "SETTLEMENT");
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
			OutputNotifyAllSycn(0, "", "INFO", "SETTLEMENT");
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

		Log.WithField("fun", "OnRspQrySettlementInfo").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQrySettlementInfo").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
	}

	if (nullptr == pSettlementInfo)
	{
		if ((nullptr == pRspInfo) && (bIsLast))
		{
			_ios.post(boost::bind(&traderctp::ProcessEmptySettlementInfo, this));
		}
		return;
	}
	else
	{
		std::shared_ptr<CThostFtdcSettlementInfoField> ptr
			= std::make_shared<CThostFtdcSettlementInfoField>
			(CThostFtdcSettlementInfoField(*pSettlementInfo));
		_ios.post(boost::bind(&traderctp::ProcessQrySettlementInfo, this, ptr, bIsLast));
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

		Log.WithField("fun", "OnRspSettlementInfoConfirm").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspSettlementInfoConfirm").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);

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
		OutputNotifySycn(m_loging_connectId, pRspInfo->ErrorID, u8"修改密码成功");
		if (_req_login.password == strOldPassword)
		{
			_req_login.password = strNewPassword;
		}
	}
	else
	{
		OutputNotifySycn(m_loging_connectId, pRspInfo->ErrorID
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

		Log.WithField("fun", "OnRspUserPasswordUpdate").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspUserPasswordUpdate").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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
			OrderIdRemoteToLocal(remote_key, &local_key);

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

		Log.WithField("fun", "OnRspOrderInsert").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspOrderInsert").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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

		Log.WithField("fun", "OnRspOrderAction").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspOrderAction").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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
			OrderIdRemoteToLocal(remote_key, &local_key);

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
		SerializerLogCtp nss;
		nss.FromVar(*pInputOrder);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnErrRtnOrderInsert").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnErrRtnOrderInsert").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").Write(LOG_INFO);
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
		SerializerLogCtp nss;
		nss.FromVar(*pOrderAction);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnErrRtnOrderAction").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnErrRtnOrderAction").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").Write(LOG_INFO);
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
		if (!ins)
		{
			Log.WithField("fun", "ProcessQryInvestorPosition").WithField("key", _key.c_str()).WithField("msg", "ctp OnRspQryInvestorPosition,instrument not exist").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("symbol", symbol.c_str()).Write(LOG_WARNING);
		}
		else
		{
			bool b_has_td_yd_distinct = (exchange_id == "SHFE") || (exchange_id == "INE");
			Position& position = GetPosition(symbol);
			position.user_id = pRspInvestorPosition->InvestorID;
			position.exchange_id = exchange_id;
			position.instrument_id = pRspInvestorPosition->InstrumentID;
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
}

void traderctp::AdjustPositionByTrade(const Trade& trade)
{
	Position& pos = GetPosition(trade.symbol());
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
			Log.WithField("fun", "InitPositionVolume").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("exchange_id", trade.exchange_id.c_str()).WithField("instrument_id", trade.instrument_id.c_str()).WithField("pos_short_today", pos.pos_short_today).WithField("pos_short_his", pos.pos_short_his).WithField("pos_long_today", pos.pos_long_today)(LOG_ERROR, nullptr
				,"pos_long_his=%d" 
				, pos.pos_long_his
				);
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

		Log.WithField("fun", "OnRspQryInvestorPosition").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQryInvestorPosition").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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

	if (!pBrokerTradingParams)
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

		Log.WithField("fun", "OnRspQryBrokerTradingParams").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQryBrokerTradingParams").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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

		Log.WithField("fun", "OnRspQryTradingAccount").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQryTradingAccount").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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

		Log.WithField("fun", "OnRspQryContractBank").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQryContractBank").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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

		Log.WithField("fun", "OnRspQryAccountregister").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQryAccountregister").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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

		Log.WithField("fun", "OnRspQryTransferSerial").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").WithField("pack", strMsg.c_str()).Write(LOG_INFO);
	}
	else
	{
		Log.WithField("fun", "OnRspQryTransferSerial").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);
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
			OutputNotifyAllSycn(0, u8"转账成功");
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
		SerializerLogCtp nss;
		nss.FromVar(*pRspTransfer);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnRtnFromBankToFutureByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, strMsg.c_str()
			,"user_name=%s"
			, _req_login.user_name.c_str()
			);
	}
	else
	{
		Log.WithField("fun", "OnRtnFromBankToFutureByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO,nullptr
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
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
		SerializerLogCtp nss;
		nss.FromVar(*pRspTransfer);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnRtnFromFutureToBankByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, strMsg.c_str()
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
	}
	else
	{
		Log.WithField("fun", "OnRtnFromFutureToBankByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, nullptr
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
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
		OutputNotifyAllAsych(pRspInfo->ErrorID
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
		SerializerLogCtp nss;
		nss.FromVar(*pReqTransfer);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnErrRtnBankToFutureByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999)(LOG_INFO, strMsg.c_str()
			,"ErrMsg=%s"
			, pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : ""
			);
	}
	else
	{
		Log.WithField("fun", "OnErrRtnBankToFutureByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999)(LOG_INFO, nullptr
			,"ErrMsg=%s"
			, pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : ""
			);
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
			, u8"期货资金转银行错误," + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
		m_req_transfer_list.pop_front();
	}

}

void traderctp::OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{
	if (nullptr != pReqTransfer)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pReqTransfer);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnErrRtnFutureToBankByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999)(LOG_INFO, strMsg.c_str()
			,"ErrMsg=%s"
			, pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : ""
		);
	}
	else
	{
		Log.WithField("fun", "OnErrRtnFutureToBankByFuture").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999)(LOG_INFO, nullptr
			,"ErrMsg=%s"
			, pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : ""
		);
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

	trader_dll::RemoteOrderKey remote_key;
	remote_key.exchange_id = pOrder->ExchangeID;
	remote_key.instrument_id = pOrder->InstrumentID;
	remote_key.front_id = pOrder->FrontID;
	remote_key.session_id = pOrder->SessionID;
	remote_key.order_ref = pOrder->OrderRef;
	remote_key.order_sys_id = pOrder->OrderSysID;
	trader_dll::LocalOrderKey local_key;
	//找到委托单local_key;
	OrderIdRemoteToLocal(remote_key, &local_key);
	Order& order = GetOrder(local_key.order_id);
	//委托单初始属性(由下单者在下单前确定, 不再改变)
	order.seqno = m_data_seq++;
	order.user_id = local_key.user_id;
	order.order_id = local_key.order_id;
	order.exchange_id = pOrder->ExchangeID;
	order.instrument_id = pOrder->InstrumentID;
	auto ins = GetInstrument(order.symbol());
	if (!ins)
	{
		Log.WithField("fun", "ProcessRtnOrder").WithField("key", _key.c_str()).WithField("msg", "ctp OnRtnOrder,instrument not exist").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("symbol", order.symbol().c_str()).Write(LOG_ERROR);
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
	sscanf(pOrder->InsertDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
	sscanf(pOrder->InsertTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
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
			OutputNotifyAllSycn(1, u8"下单成功");
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
			OutputNotifyAllSycn(1, u8"撤单成功");

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
				OutputNotifyAllSycn(1, u8"下单失败," + order.last_msg, "WARNING");
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
		SerializerLogCtp nss;
		nss.FromVar(*pOrder);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnRtnOrder").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, strMsg.c_str()
			,"user_name=%s"
			, _req_login.user_name.c_str()			
		);
	}
	else
	{
		Log.WithField("fun", "OnRtnOrder").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, nullptr
			,"user_name=%s"
			, _req_login.user_name.c_str()			
		);
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
				<< u8"." << serverOrderInfo.InstrumentId << u8",手数:" << pTrade->Volume << "!";
			OutputNotifyAllSycn(0, ss.str().c_str());

			if (serverOrderInfo.VolumeLeft <= 0)
			{
				m_input_order_key_map.erase(it);
			}
			break;
		}
	}

	LocalOrderKey local_key;
	FindLocalOrderId(pTrade->ExchangeID, pTrade->OrderSysID, &local_key);
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
	if (!ins)
	{
		Log.WithField("fun", "ProcessRtnTrade").WithField("key", _key.c_str()).WithField("msg", "ctp OnRtnTrade,instrument not exist").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("symbol", trade.symbol().c_str()).Write(LOG_ERROR);
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
	sscanf(pTrade->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
	sscanf(pTrade->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
	trade.trade_date_time = DateTimeToEpochNano(&dt);
	trade.commission = 0.0;
	trade.changed = true;
	m_something_changed = true;
	if (m_position_inited){
		AdjustPositionByTrade(trade);
	}
	SendUserData();
}

void traderctp::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
	if (nullptr != pTrade)
	{
		SerializerLogCtp nss;
		nss.FromVar(*pTrade);
		std::string strMsg = "";
		nss.ToString(&strMsg);

		Log.WithField("fun", "OnRtnTrade").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, strMsg.c_str()
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
	}
	else
	{
		Log.WithField("fun", "OnRtnTrade").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, nullptr
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
	}

	if (nullptr == pTrade)
	{
		return;
	}
	std::shared_ptr<CThostFtdcTradeField> ptr2 =
		std::make_shared<CThostFtdcTradeField>(CThostFtdcTradeField(*pTrade));
	_ios.post(boost::bind(&traderctp::ProcessRtnTrade, this, ptr2));
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
		OutputNotifyAllAsych(0,s);
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

		Log.WithField("fun", "OnRtnTradingNotice").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, strMsg.c_str()
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
	}
	else
	{
		Log.WithField("fun", "OnRtnTradingNotice").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str())(LOG_INFO, nullptr
			,"user_name=%s"
			, _req_login.user_name.c_str()
		);
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

void traderctp::ProcessRspError(std::shared_ptr<CThostFtdcRspInfoField> pRspInfo)
{
	if (nullptr != pRspInfo)
	{		
		OutputNotifyAllAsych(pRspInfo->ErrorID,GBKToUTF8(pRspInfo->ErrorMsg).c_str());
	}
}

void traderctp::OnRspError(CThostFtdcRspInfoField* pRspInfo
	, int nRequestID, bool bIsLast)
{
	Log.WithField("fun", "OnRspError").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ErrorID", pRspInfo ? pRspInfo->ErrorID : -999).WithField("ErrMsg", pRspInfo ? GBKToUTF8(pRspInfo->ErrorMsg).c_str() : "").WithField("nRequestID", nRequestID).WithField("bIsLast", bIsLast ? "true" : "false").Write(LOG_INFO);

	if (nullptr == pRspInfo)
	{
		return;
	}

	std::shared_ptr<CThostFtdcRspInfoField> ptr2 =
		std::make_shared<CThostFtdcRspInfoField>(CThostFtdcRspInfoField(*pRspInfo));
	_ios.post(boost::bind(&traderctp::ProcessRspError, this
		, ptr2));
}


#pragma endregion


#pragma region ctp_request

void traderctp::ReqAuthenticate()
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
		Log.WithField("fun", "ReqAuthenticate").WithField("key", _key.c_str()).WithField("msg", "_req_login.broker.auth_code.empty()").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_INFO);
		SendLoginRequest();
		return;
	}
	CThostFtdcReqAuthenticateField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.UserID, _req_login.user_name.c_str());
	strcpy_x(field.UserProductInfo, _req_login.broker.product_info.c_str());
	strcpy_x(field.AuthCode, _req_login.broker.auth_code.c_str());
	int ret = m_pTdApi->ReqAuthenticate(&field, ++_requestID);
	if (0 != ret)
	{
		Log.WithField("fun", "ReqAuthenticate").WithField("key", _key.c_str()).WithField("msg", "ctp ReqAuthenticate fail").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("UserProductInfo", _req_login.broker.product_info.c_str()).WithField("AuthCode", _req_login.broker.auth_code.c_str()).WithField("ret", ret).Write(LOG_INFO);
		boost::unique_lock<boost::mutex> lock(_logInmutex);
		_logIn_status = 0;
		_logInCondition.notify_all();
	}
}

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
		Log.WithField("fun", "ReqUserLogin").WithField("key", _key.c_str()).WithField("msg", "ctp ReqUserLogin fail").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("UserProductInfo", _req_login.broker.product_info.c_str()).WithField("LoginRemark", _req_login.client_ip.c_str()).WithField("ret", ret).Write(LOG_INFO);
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
	long long now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	m_req_login_dt.store(now);
	int ret = ReqUserLogin();
	if (0 != ret)
	{		
		boost::unique_lock<boost::mutex> lock(_logInmutex);
		_logIn_status = 0;
		_logInCondition.notify_all();
	}
}

void traderctp::ReinitCtp()
{
	Log.WithField("fun", "ReinitCtp").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_INFO);
	if (nullptr != m_pTdApi)
	{
		StopTdApi();
	}
	boost::this_thread::sleep_for(boost::chrono::seconds(60));
	InitTdApi();
	if (nullptr != m_pTdApi)
	{
		m_pTdApi->Init();
	}
	Log.WithField("fun", "ReinitCtp").WithField("key", _key.c_str()).WithField("msg", "ctp ReinitCtp end").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_INFO);
}

void traderctp::ReqConfirmSettlement()
{
	CThostFtdcSettlementInfoConfirmField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqSettlementInfoConfirm(&field, 0);
	Log.WithField("fun", "ReqConfirmSettlement").WithField("key", _key.c_str()).WithField("msg", "ctp ReqConfirmSettlement").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
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
	Log.WithField("fun", "ReqQrySettlementInfoConfirm").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQrySettlementInfoConfirm").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
}

int traderctp::ReqQryBrokerTradingParams()
{
	CThostFtdcQryBrokerTradingParamsField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryBrokerTradingParams(&field, 0);
	if (0 != r)
	{
		Log.WithField("fun", "ReqQryBrokerTradingParams").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQryBrokerTradingParams").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}
	return r;
}

int traderctp::ReqQryAccount(int reqid)
{
	CThostFtdcQryTradingAccountField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryTradingAccount(&field, reqid);
	if (0 != r)
	{
		Log.WithField("fun", "ReqQryAccount").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQryTradingAccount").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}
	return r;
}

int traderctp::ReqQryPosition(int reqid)
{
	CThostFtdcQryInvestorPositionField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryInvestorPosition(&field, reqid);
	if (0 != r)
	{
		Log.WithField("fun", "ReqQryPosition").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQryInvestorPosition").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}	
	return r;
}

void traderctp::ReqQryBank()
{
	CThostFtdcQryContractBankField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	m_pTdApi->ReqQryContractBank(&field, 0);
	int r = m_pTdApi->ReqQryContractBank(&field, 0);
	if (0 != r)
	{
		Log.WithField("fun", "ReqQryBank").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQryContractBank").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}	
}

void traderctp::ReqQryAccountRegister()
{
	CThostFtdcQryAccountregisterField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	m_pTdApi->ReqQryAccountregister(&field, 0);
	int r = m_pTdApi->ReqQryAccountregister(&field, 0);
	if (0 != r)
	{
		Log.WithField("fun", "ReqQryAccountRegister").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQryAccountregister").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}	
}

void traderctp::ReqQrySettlementInfo()
{
	CThostFtdcQrySettlementInfoField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	strcpy_x(field.AccountID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQrySettlementInfo(&field, 0);
	if (0 != r)
	{
		Log.WithField("fun", "ReqQrySettlementInfo").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQrySettlementInfo").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}	
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
	if (0 != r)
	{
		Log.WithField("fun", "ReqQryHistorySettlementInfo").WithField("key", _key.c_str()).WithField("msg", "ctp ReqQryHistorySettlementInfo").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
	}	
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

	OutputNotifySycn(connectId, 0, m_settlement_info, "INFO", "SETTLEMENT");
}

#pragma endregion


#pragma region businesslogic

bool traderctp::OrderIdLocalToRemote(const LocalOrderKey& local_order_key
	, RemoteOrderKey* remote_order_key)
{
	if (local_order_key.order_id.empty())
	{
		remote_order_key->session_id = m_session_id;
		remote_order_key->front_id = m_front_id;
		remote_order_key->order_ref = std::to_string(m_order_ref++);
		return false;
	}
	auto it = m_ordermap_local_remote.find(local_order_key);
	if (it == m_ordermap_local_remote.end())
	{
		remote_order_key->session_id = m_session_id;
		remote_order_key->front_id = m_front_id;
		remote_order_key->order_ref = std::to_string(m_order_ref++);
		m_ordermap_local_remote[local_order_key] = *remote_order_key;
		m_ordermap_remote_local[*remote_order_key] = local_order_key;
		return false;
	}
	else
	{
		*remote_order_key = it->second;
		return true;
	}
}

void traderctp::OrderIdRemoteToLocal(const RemoteOrderKey& remote_order_key
	, LocalOrderKey* local_order_key)
{
	auto it = m_ordermap_remote_local.find(remote_order_key);
	if (it == m_ordermap_remote_local.end())
	{
		char buf[1024];
		sprintf(buf, "SERVER.%s.%08x.%d"
			, remote_order_key.order_ref.c_str()
			, remote_order_key.session_id
			, remote_order_key.front_id);
		local_order_key->order_id = buf;
		local_order_key->user_id = _req_login.user_name;
		m_ordermap_local_remote[*local_order_key] = remote_order_key;
		m_ordermap_remote_local[remote_order_key] = *local_order_key;
	}
	else
	{
		*local_order_key = it->second;
		RemoteOrderKey& r = const_cast<RemoteOrderKey&>(it->first);
		if (!remote_order_key.order_sys_id.empty())
		{
			r.order_sys_id = remote_order_key.order_sys_id;
		}
	}
}

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

Position& traderctp::GetPosition(const std::string symbol)
{
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

	if (m_confirm_settlement_status.load() == 1)
	{
		ReqConfirmSettlement();
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

	if (m_data.m_accounts.size() == 0)
		return;

	if (!m_position_ready)
		return;

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
			Log.WithField("fun", "SendUserData").WithField("key", symbol.c_str()).WithField("bid", _key.c_str()).WithField("user_name", _req_login.bid.c_str())(LOG_ERROR,nullptr
				,"msg=ctp miss symbol %s when processing position;"
				, _req_login.user_name.c_str()
			);
			continue;
		}
		ps.volume_long = ps.volume_long_his + ps.volume_long_today;
		ps.volume_short = ps.volume_short_his + ps.volume_short_today;
		ps.volume_long_frozen = ps.volume_long_frozen_today + ps.volume_long_frozen_his;
		ps.volume_short_frozen = ps.volume_short_frozen_today + ps.volume_short_frozen_his;
		ps.margin = ps.margin_long + ps.margin_short;
		double last_price = ps.ins->last_price;
		if (!IsValid(last_price))
			last_price = ps.ins->pre_settlement;
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
			if (ps.volume_short > 0)
			{
				ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
				ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
			}
			ps.changed = true;
			m_something_changed = true;
		}
		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;
		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;
	}
	//重算资金账户
	if (m_something_changed)
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
		if (IsValid(acc.available) && IsValid(acc.balance) && !IsZero(acc.balance))
			acc.risk_ratio = 1.0 - acc.available / acc.balance;
		else
			acc.risk_ratio = NAN;
		acc.changed = true;
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
	m_something_changed = false;
	m_peeking_message = false;
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
			Log.WithField("fun", "SendUserDataImd").WithField("key", symbol.c_str()).WithField("bid", _key.c_str()).WithField("user_name", _req_login.bid.c_str())(LOG_ERROR,nullptr
				,"msg=ctp miss symbol %s when processing position;"
				, _req_login.user_name.c_str()
			);
			continue;
		}
		ps.volume_long = ps.volume_long_his + ps.volume_long_today;
		ps.volume_short = ps.volume_short_his + ps.volume_short_today;
		ps.volume_long_frozen = ps.volume_long_frozen_today + ps.volume_long_frozen_his;
		ps.volume_short_frozen = ps.volume_short_frozen_today + ps.volume_short_frozen_his;
		ps.margin = ps.margin_long + ps.margin_short;
		double last_price = ps.ins->last_price;
		if (!IsValid(last_price))
			last_price = ps.ins->pre_settlement;
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
			if (ps.volume_short > 0)
			{
				ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
				ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
			}
			ps.changed = true;
			m_something_changed = true;
		}
		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;
		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;
	}

	//重算资金账户
	if (m_something_changed)
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
		if (IsValid(acc.available) && IsValid(acc.balance) && !IsZero(acc.balance))
			acc.risk_ratio = 1.0 - acc.available / acc.balance;
		else
			acc.risk_ratio = NAN;
		acc.changed = true;
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
		Log.WithField("fun", "Start").WithField("key", _key.c_str()).WithField("msg", "open message_queue").WithField("errmsg", ex.what()).Write(LOG_ERROR);
	}

	try
	{

		m_run_receive_msg.store(true);
		_thread_ptr = boost::make_shared<boost::thread>(
			boost::bind(&traderctp::ReceiveMsg,this,_key));
	}
	catch (const std::exception& ex)
	{
		Log.WithField("fun", "Start").WithField("key", _key.c_str()).WithField("msg", "trade ctp start ReceiveMsg thread fail").WithField("errmsg", ex.what()).Write(LOG_ERROR);
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
				Log.WithField("fun", "ReceiveMsg").WithField("key", strKey.c_str()).WithField("msg", "traderctp ReceiveMsg is invalid!").WithField("msgcontent", line.c_str()).Write(LOG_WARNING);
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
			Log.WithField("fun", "ReceiveMsg").WithField("key", strKey.c_str()).WithField("msg", "traderctp ReceiveMsg exception").WithField("errmsg", ex.what()).Write(LOG_ERROR);
			break;
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
	Log.WithField("fun", "CloseConnection").WithField("key", _key.c_str()).WithField("msg", "traderctp CloseConnection").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", nId).Write(LOG_INFO);

	for (std::vector<int>::iterator it = m_logined_connIds.begin();
		it != m_logined_connIds.end(); it++)
	{
		if (*it == nId)
		{
			m_logined_connIds.erase(it);
			break;
		}
	}

	//如果已经没有客户端连接,将来要考虑条件单的情况
	if (m_logined_connIds.empty())
	{
		if (m_need_save_file.load())
		{
			SaveToFile();
		}

		StopTdApi();
		m_b_login.store(false);
		_logIn_status = 0;
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

		m_notify_seq = 0;
		m_data_seq = 0;
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
	}
}

void traderctp::ProcessInMsg(int connId, std::shared_ptr<std::string> msg_ptr)
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
		Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "traderctp parse json fail").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("msgcontent", msg.c_str()).WithField("connid", connId).Write(LOG_WARNING);
		return;
	}

	ReqLogin req;
	ss.ToVar(req);
	if (req.aid == "req_login")
	{
		ProcessReqLogIn(connId, req);
	}
	else if (req.aid == "change_password")
	{
		if (nullptr == m_pTdApi)
		{
			Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "trade ctp receive change_password msg before receive login msg").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", connId).Write(LOG_ERROR);
			return;
		}

		if ((!m_b_login.load()) && (m_loging_connectId != connId))
		{
			Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "trade ctp receive change_password msg from a diffrent connection before login suceess").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", connId).Write(LOG_ERROR);
			return;
		}

		m_loging_connectId = connId;

		SerializerCtp ssCtp;
		if (!ssCtp.FromString(msg.c_str()))
		{
			return;
		}

		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ssCtp.m_doc));
		if (!dt || !dt->IsString())
		{
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
	}
	else
	{
		if (!m_b_login)
		{
			Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "trade ctp receive other msg before login").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", connId).WithField("pack", msg.c_str()).Write(LOG_WARNING);
			return;
		}

		if (!IsConnectionLogin(connId))
		{
			Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "trade ctp receive other msg which from not login connecion").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", connId).WithField("pack", msg.c_str()).Write(LOG_WARNING);
			return;
		}

		SerializerCtp ssCtp;
		if (!ssCtp.FromString(msg.c_str()))
		{
			return;
		}

		rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ssCtp.m_doc));
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
			ssCtp.ToVar(d);
			OnClientReqInsertOrder(d);
		}
		else if (aid == "cancel_order")
		{
			CtpActionCancelOrder d;
			ssCtp.ToVar(d);
			OnClientReqCancelOrder(d);
		}
		else if (aid == "req_transfer")
		{
			CThostFtdcReqTransferField f;
			memset(&f, 0, sizeof(f));
			ssCtp.ToVar(f);
			OnClientReqTransfer(f);
		}
		else if (aid == "confirm_settlement")
		{
			Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "trade ctp receive confirm_settlement").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", connId).WithField("pack", msg.c_str()).Write(LOG_INFO);
			if (0 == m_confirm_settlement_status.load())
			{
				m_confirm_settlement_status.store(1);
			}
			ReqConfirmSettlement();
		}
		else if (aid == "qry_settlement_info")
		{
			Log.WithField("fun", "ProcessInMsg").WithField("key", _key.c_str()).WithField("msg", "trade ctp receive qry_settlement_info msg").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("connid", connId).WithField("pack", msg.c_str()).Write(LOG_INFO);
			qry_settlement_info qrySettlementInfo;
			ss.ToVar(qrySettlementInfo);
			OnClientReqQrySettlementInfo(qrySettlementInfo);
		}
	}
}

void traderctp::OnClientReqQrySettlementInfo(const qry_settlement_info
	& qrySettlementInfo)
{
	m_qry_his_settlement_info_trading_days.push_back(qrySettlementInfo.trading_day);
}

void traderctp::ProcessReqLogIn(int connId, ReqLogin& req)
{
	Log.WithField("fun", "ProcessReqLogIn").WithField("key", _key.c_str()).WithField("msg", "traderctp ProcessReqLogIn").WithField("bid", req.bid.c_str()).WithField("user_name", req.user_name.c_str()).WithField("client_ip", req.client_ip.c_str()).WithField("client_port", req.client_port).WithField("client_app_id", req.client_app_id.c_str()).WithField("front", req.front.c_str()).WithField("broker_id", req.broker_id.c_str()).Write(LOG_INFO);

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
			OutputNotifySycn(connId, 0, u8"用户登录失败!");
		}
	}
	else
	{
		_req_login = req;
		auto it = g_config.brokers.find(_req_login.bid);
		_req_login.broker = it->second;

		//为了支持次席而添加的功能
		if ((!_req_login.broker_id.empty()) &&
			(!_req_login.front.empty()))
		{
			Log.WithField("fun", "ProcessReqLogIn").WithField("key", _key.c_str()).WithField("msg", "ctp login from custom front and broker_id").WithField("bid", req.bid.c_str()).WithField("user_name", req.user_name.c_str()).WithField("broker_id", req.broker_id.c_str()).WithField("front", req.front.c_str()).Write(LOG_INFO);

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
		int login_status = WaitLogIn();
		if (0 == login_status)
		{
			m_b_login.store(false);
			StopTdApi();
		}
		else if (1 == login_status)
		{
			m_b_login.store(false);
		}
		else if (2 == login_status)
		{
			m_b_login.store(true);
		}

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
				, _req_login.user_name.c_str()
				, _req_login.user_name.c_str()
				, m_trading_day.c_str()
			);
			std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
			_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));
		}
		else
		{
			m_loging_connectId = connId;
			OutputNotifySycn(connId, 0, u8"用户登录失败!");
		}
	}
}

int traderctp::WaitLogIn()
{
	boost::unique_lock<boost::mutex> lock(_logInmutex);
	_logIn_status = 0;
	m_pTdApi->Init();
	bool notify = _logInCondition.timed_wait(lock, boost::posix_time::seconds(15));
	if (0 == _logIn_status)
	{
		if (!notify)
		{
			Log.WithField("fun", "WaitLogIn").WithField("key", _key.c_str()).WithField("msg", "CTP login timeout,trading fronts is closed or trading fronts config is error").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_WARNING);
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
		Log.WithField("fun", "InitTdApi").WithField("key", _key.c_str()).WithField("msg", "fens address is used").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_INFO);

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
		Log.WithField("fun", "StopTdApi").WithField("key", _key.c_str()).WithField("msg", "ctp OnFinish").WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_INFO);
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
			Log.WithField("fun", "SendMsg").WithField("length", ex.what()).WithField("key", msg.length()).WithField("bid", _key.c_str()).WithField("user_name", _req_login.bid.c_str())(LOG_ERROR,nullptr
				,"msg=SendMsg exception,%s;"
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
			Log.WithField("fun", "SendMsg").WithField("length", ex.what()).WithField("key", totalLength).WithField("bid", _key.c_str()).WithField("user_name", _req_login.bid.c_str())(LOG_ERROR,nullptr
				,"msg=SendMsg exception,%s;"
				, _req_login.user_name.c_str());
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
			Log.WithField("fun", "SendMsgAll").WithField("errmsg", ex.what()).WithField("length", msg.length()).WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_ERROR);
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
			Log.WithField("fun", "SendMsgAll").WithField("errmsg", ex.what()).WithField("length", totalLength).WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).Write(LOG_ERROR);
		}
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
	_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));
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
	_ios.post(boost::bind(&traderctp::SendMsg, this, connId, msg_ptr));
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
		_ios.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
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

#pragma endregion

#pragma region client_request

void traderctp::OnClientReqChangePassword(CThostFtdcUserPasswordUpdateField f)
{
	strcpy_x(f.BrokerID, m_broker_id.c_str());
	strcpy_x(f.UserID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqUserPasswordUpdate(&f, 0);
	Log.WithField("fun", "OnClientReqChangePassword").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("ret", r).Write(LOG_INFO);
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
		Log.WithField("fun", "OnClientReqTransfer").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("TradeAmount", f.TradeAmount).WithField("ret", r).WithField("nRequestID", nRequestID).Write(LOG_INFO);
		m_req_transfer_list.push_back(nRequestID);		
	}
	else
	{
		strcpy_x(f.TradeCode, "202002");
		f.TradeAmount = -f.TradeAmount;
		int nRequestID = _requestID++;
		int r = m_pTdApi->ReqFromFutureToBankByFuture(&f,nRequestID);
		Log.WithField("fun", "OnClientReqTransfer").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("TradeAmount", f.TradeAmount).WithField("ret", r).WithField("nRequestID", nRequestID).Write(LOG_INFO);
		m_req_transfer_list.push_back(nRequestID);		
	}
}

void traderctp::OnClientReqCancelOrder(CtpActionCancelOrder d)
{
	if (d.local_key.user_id.substr(0, _req_login.user_name.size()) != _req_login.user_name)
	{
		OutputNotifyAllSycn(1, u8"撤单user_id错误，不能撤单", "WARNING");
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

	int r = m_pTdApi->ReqOrderAction(&d.f, 0);
	Log.WithField("fun", "OnClientReqCancelOrder").WithField("key", _key.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("InstrumentID", d.f.InstrumentID).WithField("OrderRef", d.f.OrderRef).WithField("ret", r).Write(LOG_INFO);
}

void traderctp::OnClientReqInsertOrder(CtpActionInsertOrder d)
{
	if (d.local_key.user_id.substr(0, _req_login.user_name.size()) != _req_login.user_name)
	{
		OutputNotifyAllSycn(1, u8"报单user_id错误，不能下单", "WARNING");
		return;
	}

	strcpy_x(d.f.BrokerID, m_broker_id.c_str());
	strcpy_x(d.f.UserID, _req_login.user_name.c_str());
	strcpy_x(d.f.InvestorID, _req_login.user_name.c_str());
	RemoteOrderKey rkey;
	rkey.exchange_id = d.f.ExchangeID;
	rkey.instrument_id = d.f.InstrumentID;
	if (OrderIdLocalToRemote(d.local_key, &rkey))
	{
		OutputNotifyAllSycn(1, u8"报单单号重复，不能下单", "WARNING");
		return;
	}

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
	Log.WithField("fun", "OnClientReqInsertOrder").WithField("key", _key.c_str()).WithField("orderid", d.local_key.order_id.c_str()).WithField("bid", _req_login.bid.c_str()).WithField("user_name", _req_login.user_name.c_str()).WithField("InstrumentID", d.f.InstrumentID).WithField("OrderRef", d.f.OrderRef).WithField("ret", r).WithField("OrderPriceType", d.f.OrderPriceType).WithField("Direction", d.f.Direction).WithField("CombOffsetFlag", d.f.CombHedgeFlag[0]).WithField("LimitPrice", d.f.LimitPrice).WithField("VolumeTotalOriginal", d.f.VolumeTotalOriginal).WithField("VolumeCondition", d.f.VolumeCondition).WithField("TimeCondition", d.f.TimeCondition).Write(LOG_INFO);
	m_need_save_file.store(true);
}

void traderctp::OnClientPeekMessage()
{
	m_peeking_message = true;
	//向客户端发送账户信息
	SendUserData();
}



#pragma endregion

