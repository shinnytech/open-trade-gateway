/////////////////////////////////////////////////////////////////////////
///@file tradeks.h
///@brief	ks交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "ks_define.h"
#include "types.h"
#include "condition_order_manager.h"

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

#include "ks/KSTraderApiEx.h"
#include "ks/KSCosApi.h"
#include "ks/KSOptionApi.h"
#include "ks/KSVocApi.h"

enum class ECTPLoginStatus
{
	init = 340,
	reqAuthenFail = 341,
	rspAuthenFail = 342,
	regSystemInfoFail = 343,
	reqUserLoginFail = 344,
	rspLoginFail = 345,
	rspLoginFailNeedModifyPassword = 346,
	reqLoginTimeOut = 347,
	rspLoginSuccess = 350
};

class traderctp : public KingstarAPI::CThostFtdcTraderSpi, public IConditionOrderCallBack, public KingstarAPI::CKSVocSpi
{
public:
	traderctp(boost::asio::io_context& ios
		, const std::string& key);

	void Start();

	void Stop();

	///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
	virtual void OnFrontConnected();

	///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
	///@param nReason 错误原因
	///        0x1001 网络读失败
	///        0x1002 网络写失败
	///        0x2001 接收心跳超时
	///        0x2002 发送心跳失败
	///        0x2003 收到错误报文
	virtual void OnFrontDisconnected(int nReason);

	///客户端认证响应
	virtual void OnRspAuthenticate(KingstarAPI::CThostFtdcRspAuthenticateField *pRspAuthenticateField, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///登录请求响应
	virtual void OnRspUserLogin(KingstarAPI::CThostFtdcRspUserLoginField* pRspUserLogin, KingstarAPI::CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

	///用户口令更新请求响应
	virtual void OnRspUserPasswordUpdate(KingstarAPI::CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询结算信息确认响应
	virtual void OnRspQrySettlementInfoConfirm(KingstarAPI::CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询投资者结算结果响应
	virtual void OnRspQrySettlementInfo(KingstarAPI::CThostFtdcSettlementInfoField *pSettlementInfo, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///投资者结算结果确认响应
	virtual void OnRspSettlementInfoConfirm(KingstarAPI::CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///报单录入请求响应
	virtual void OnRspOrderInsert(KingstarAPI::CThostFtdcInputOrderField* pInputOrder, KingstarAPI::CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

	///报单操作请求响应
	virtual void OnRspOrderAction(KingstarAPI::CThostFtdcInputOrderActionField* pInputOrderAction, KingstarAPI::CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

	///报单录入错误回报
	virtual void OnErrRtnOrderInsert(KingstarAPI::CThostFtdcInputOrderField *pInputOrder, KingstarAPI::CThostFtdcRspInfoField *pRspInfo);

	///报单操作错误回报
	virtual void OnErrRtnOrderAction(KingstarAPI::CThostFtdcOrderActionField *pOrderAction, KingstarAPI::CThostFtdcRspInfoField *pRspInfo);

	///请求查询投资者持仓响应
	virtual void OnRspQryInvestorPosition(KingstarAPI::CThostFtdcInvestorPositionField* pInvestorPosition, KingstarAPI::CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

	///请求查询资金账户响应
	virtual void OnRspQryTradingAccount(KingstarAPI::CThostFtdcTradingAccountField* pTradingAccount, KingstarAPI::CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

	///请求查询签约银行响应
	virtual void OnRspQryContractBank(KingstarAPI::CThostFtdcContractBankField *pContractBank, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);;

	///请求查询银期签约关系响应
	virtual void OnRspQryAccountregister(KingstarAPI::CThostFtdcAccountregisterField *pAccountregister, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///请求查询转帐流水响应
	virtual void OnRspQryTransferSerial(KingstarAPI::CThostFtdcTransferSerialField *pTransferSerial, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///期货发起银行资金转期货通知
	virtual void OnRtnFromBankToFutureByFuture(KingstarAPI::CThostFtdcRspTransferField *pRspTransfer);

	///期货发起期货资金转银行通知
	virtual void OnRtnFromFutureToBankByFuture(KingstarAPI::CThostFtdcRspTransferField *pRspTransfer);

	///期货发起银行资金转期货错误回报
	virtual void OnErrRtnBankToFutureByFuture(KingstarAPI::CThostFtdcReqTransferField *pReqTransfer, KingstarAPI::CThostFtdcRspInfoField *pRspInfo);

	///期货发起期货资金转银行错误回报
	virtual void OnErrRtnFutureToBankByFuture(KingstarAPI::CThostFtdcReqTransferField *pReqTransfer, KingstarAPI::CThostFtdcRspInfoField *pRspInfo);

	///错误应答
	virtual void OnRspError(KingstarAPI::CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

	///报单通知
	virtual void OnRtnOrder(KingstarAPI::CThostFtdcOrderField* pOrder);

	///成交通知
	virtual void OnRtnTrade(KingstarAPI::CThostFtdcTradeField* pTrade);

	///交易通知
	virtual void OnRtnTradingNotice(KingstarAPI::CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo);

	///请求查询经纪公司交易参数响应
	virtual void OnRspQryBrokerTradingParams(KingstarAPI::CThostFtdcBrokerTradingParamsField *pBrokerTradingParams
		, KingstarAPI::CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	///合约交易状态通知
	virtual void OnRtnInstrumentStatus(KingstarAPI::CThostFtdcInstrumentStatusField *pInstrumentStatus);
	//////////
	// 请求号
	int m_nRequestID;
	//////////
private:	
	//////////////////////
	std::atomic_bool m_b_login;

	std::string _key;

	std::string m_settlement_info;

	boost::asio::io_context& _ios;

	std::shared_ptr<boost::interprocess::message_queue> _out_mq_ptr;

	std::string _out_mq_name;

	std::shared_ptr<boost::interprocess::message_queue> _in_mq_ptr;

	std::string _in_mq_name;

	boost::shared_ptr<boost::thread> _thread_ptr;

	std::atomic_int m_data_seq;

	ReqLogin _req_login;

	std::string m_broker_id;

	KingstarAPI::CThostFtdcTraderApi* m_pTdApi;

	std::atomic_llong m_req_login_dt;

	std::string m_trading_day;

	int m_session_id;

	int m_front_id;

	int m_order_ref;

	std::atomic_int _requestID;;

	ECTPLoginStatus _logIn_status;

	boost::mutex _logInmutex;

	int m_loging_connectId;

	std::vector<int> m_logined_connIds;

	boost::condition_variable _logInCondition;

	std::string m_user_file_path;

	//委托单单号映射表管理
	std::map<LocalOrderKey, RemoteOrderKey> m_ordermap_local_remote;

	std::map<RemoteOrderKey, LocalOrderKey> m_ordermap_remote_local;

	std::atomic_bool m_need_query_settlement;

	std::atomic_int m_confirm_settlement_status;

	std::atomic_int m_req_position_id;

	std::atomic_int m_req_account_id;

	std::atomic_bool m_need_query_bank;

	std::atomic_bool m_need_query_register;

	std::atomic_bool m_is_qry_his_settlement_info;

	std::string m_his_settlement_info;

	std::list<int> m_qry_his_settlement_info_trading_days;

	//交易账户全信息
	User m_data;

	std::atomic_bool m_something_changed;

	std::atomic_bool m_peeking_message;

	std::atomic_bool m_position_ready;

	//当日初始持仓信息是否已到位,每个交易日只需一次
	std::atomic_bool m_position_inited;

	std::atomic_int m_rsp_position_id;

	std::atomic_int m_rsp_account_id;

	std::map<std::string, ServerOrderInfo> m_input_order_key_map;

	std::map<std::string, std::string> m_action_order_map;

	std::list<int> m_req_transfer_list;

	//用户操作反馈

	//还需要给用户下单反馈的单号集合
	std::set<std::string> m_insert_order_set;

	//还需要给用户撤单反馈的单号集合
	std::set<std::string> m_cancel_order_set;

	long long m_next_qry_dt;

	long long m_next_send_dt;

	std::atomic_bool m_need_save_file;

	std::atomic_bool m_need_query_broker_trading_params;

	KingstarAPI::TThostFtdcAlgorithmType m_Algorithm_Type;

	std::map<std::string, Bank> m_banks;

	int m_try_req_authenticate_times;

	int m_try_req_login_times;

	std::atomic_bool m_run_receive_msg;

	std::atomic_bool m_continue_process_msg;

	boost::mutex m_continue_process_msg_mutex;

	boost::condition_variable m_continue_process_msg_condition;

	std::map<std::string, std::string> m_rtn_order_log_map;

	std::map<std::string, std::string> m_rtn_trade_log_map;

	std::map<std::string, std::string> m_err_rtn_future_to_bank_by_future_log_map;

	std::map<std::string, std::string> m_err_rtn_bank_to_future_by_future_log_map;

	std::map<std::string, std::string> m_rtn_from_bank_to_future_by_future_log_map;

	std::map<std::string, std::string> m_rtn_from_future_to_bank_by_future_log_map;

	std::map<std::string, std::string> m_err_rtn_order_insert_log_map;

	std::map<std::string, std::string> m_err_rtn_order_action_log_map;

	ConditionOrderData m_condition_order_data;

	ConditionOrderHisData m_condition_order_his_data;

	ConditionOrderManager m_condition_order_manager;

	std::vector<ctp_condition_order_task> m_condition_order_task;

	void InitTdApi();

	void StopTdApi();

	void ReceiveMsg(const std::string& key);

	void ProcessInMsg(int connId, std::shared_ptr<std::string> msg_ptr);

	void NotifyContinueProcessMsg(bool flag);

	void ProcessReqLogIn(int connId, ReqLogin& req);

	void OutputNotifySycn(int connId, long notify_code
		, const std::string& ret_msg, const char* level = "INFO"
		, const char* type = "MESSAGE");

	void OutputNotifyAllSycn(long notify_code
		, const std::string& ret_msg, const char* level = "INFO"
		, const char* type = "MESSAGE");

	void SendMsgAll(std::shared_ptr<std::string> conn_str_ptr, std::shared_ptr<std::string> msg_ptr);

	void SendMsg(int connId, std::shared_ptr<std::string> msg_ptr);

	void CloseConnection(int nId);

	bool IsConnectionLogin(int nId);

	std::string GetConnectionStr();

	ECTPLoginStatus WaitLogIn();

	int ReqAuthenticate();

	int ReqUserLogin();

	void SendLoginRequest();

	void LoadFromFile();

	void SaveToFile();

	void AfterLogin();

	void ReqConfirmSettlement();

	void ProcessEmptySettlementInfo();

	void ProcessQrySettlementInfo(std::shared_ptr<KingstarAPI::CThostFtdcSettlementInfoField> pSettlementInfo
		, bool bIsLast);

	void ProcessSettlementInfoConfirm(std::shared_ptr<KingstarAPI::CThostFtdcSettlementInfoConfirmField>
		pSettlementInfoConfirm, bool bIsLast);

	void ReqQrySettlementInfoConfirm();

	void ProcessQrySettlementInfoConfirm(std::shared_ptr<KingstarAPI::CThostFtdcSettlementInfoConfirmField>
		pSettlementInfoConfirm);

	void ProcessRspOrderInsert(std::shared_ptr<KingstarAPI::CThostFtdcInputOrderField> pInputOrder
		, std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void FindLocalOrderId(const std::string& exchange_id
		, const std::string& order_sys_id, LocalOrderKey* local_order_key);

	Order& GetOrder(const std::string order_key);

	Account& GetAccount(const std::string account_key);

	Position& GetPosition(const std::string& exchange_id, const std::string& instrument_id, const std::string& user_id);

	Bank& GetBank(const std::string& bank_id);

	Trade& GetTrade(const std::string trade_key);

	TransferLog& GetTransferLog(const std::string& seq_id);

	void SendUserData();

	void SendUserDataImd(int connectId);

	void ReSendSettlementInfo(int connectId);

	void ProcessUserPasswordUpdateField(std::shared_ptr<KingstarAPI::CThostFtdcUserPasswordUpdateField>
		pUserPasswordUpdate, std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessOrderAction(std::shared_ptr<KingstarAPI::CThostFtdcInputOrderActionField> pInputOrderAction,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessErrRtnOrderInsert(std::shared_ptr<KingstarAPI::CThostFtdcInputOrderField> pInputOrder,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessErrRtnOrderAction(std::shared_ptr<KingstarAPI::CThostFtdcOrderActionField> pOrderAction,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessQryInvestorPosition(std::shared_ptr<KingstarAPI::CThostFtdcInvestorPositionField> pInvestorPosition,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast);

	void ProcessQryTradingAccount(std::shared_ptr<KingstarAPI::CThostFtdcTradingAccountField> pRspInvestorAccount,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast);

	void ProcessQryBrokerTradingParams(std::shared_ptr<KingstarAPI::CThostFtdcBrokerTradingParamsField>
		pBrokerTradingParams, std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo
		, int nRequestID, bool bIsLast);

	void ProcessQryContractBank(std::shared_ptr<KingstarAPI::CThostFtdcContractBankField> pContractBank,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast);

	void ProcessQryAccountregister(std::shared_ptr<KingstarAPI::CThostFtdcAccountregisterField> pAccountregister,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast);

	void ProcessQryTransferSerial(std::shared_ptr<KingstarAPI::CThostFtdcTransferSerialField> pTransferSerial,
		std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo, int nRequestID, bool bIsLast);

	void ProcessRtnOrder(std::shared_ptr<KingstarAPI::CThostFtdcOrderField> pOrder);

	void ProcessRtnTrade(std::shared_ptr<KingstarAPI::CThostFtdcTradeField> pTrade);

	void OnClientPeekMessage();

	int OnClientReqInsertOrder(CtpActionInsertOrder d);

	void OnClientReqCancelOrder(CtpActionCancelOrder d);

	void OnClientReqTransfer(KingstarAPI::CThostFtdcReqTransferField f);

	void OnClientReqChangePassword(KingstarAPI::CThostFtdcUserPasswordUpdateField f);

	void OnClientReqQrySettlementInfo(const qry_settlement_info
		& qrySettlementInfo);

	void OnReqStartCTP(const std::string& msg);

	void OnReqStopCTP(const std::string& msg);

	void ClearOldData();

	void OnIdle();

	int ReqQryBrokerTradingParams();

	int ReqQryAccount(int reqid);

	int ReqQryPosition(int reqid);

	void ReqQryTransferSerial();

	void ReqQryBank();

	void ReqQryAccountRegister();

	void ReqQrySettlementInfo();

	void ReqQryHistorySettlementInfo();

	bool NeedReset();

	void ProcessOnFrontConnected();

	void ProcessOnFrontDisconnected(int nReason);

	void ProcessOnRspAuthenticate(std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessOnRspUserLogin(std::shared_ptr<KingstarAPI::CThostFtdcRspUserLoginField> pRspUserLogin
		, std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessOnRtnTradingNotice(std::shared_ptr<KingstarAPI::CThostFtdcTradingNoticeInfoField> pTradingNoticeInfo);

	void ReinitCtp();

	void ProcessFromBankToFutureByFuture(
		std::shared_ptr<KingstarAPI::CThostFtdcRspTransferField> pRspTransfer);

	void ProcessOnErrRtnFutureToBankByFuture(
		std::shared_ptr<KingstarAPI::CThostFtdcReqTransferField> pReqTransfer
		, std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessOnErrRtnBankToFutureByFuture(
		std::shared_ptr<KingstarAPI::CThostFtdcReqTransferField> pReqTransfer
		, std::shared_ptr<KingstarAPI::CThostFtdcRspInfoField> pRspInfo);

	void ProcessRtnInstrumentStatus(std::shared_ptr<KingstarAPI::CThostFtdcInstrumentStatusField>
		pInstrumentStatus);

	void NotifyClientHisSettlementInfo(const std::string& hisSettlementInfo);

	void InitPositionVolume();

	void AdjustPositionByTrade(const Trade& trade);

	void CheckTimeConditionOrder();

	void CheckPriceConditionOrder();

	virtual void OnUserDataChange();

	virtual void OutputNotifyAll(long notify_code, const std::string& ret_msg
		, const char* level, const char* type);

	virtual void OnTouchConditionOrder(const ConditionOrder& order);

	bool ConditionOrder_Open(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool SetConditionOrderPrice(KingstarAPI::CThostFtdcInputOrderField& f
		, const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins);

	bool ConditionOrder_CloseTodayPrior_NeedCancel(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_CloseTodayPrior_NotNeedCancel(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_CloseYesTodayPrior_NeedCancel(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_CloseYesTodayPrior_NotNeedCancel(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_CloseAll(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_Close(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_Reverse_Long(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	bool ConditionOrder_Reverse_Short(const ConditionOrder& order
		, const ContingentOrder& co
		, const Instrument& ins
		, ctp_condition_order_task& task
		, int nOrderIndex);

	void OnConditionOrderReqInsertOrder(CtpActionInsertOrder& d);

	void OnConditionOrderReqCancelOrder(CtpActionCancelOrder& d);

	void CheckConditionOrderCancelOrderTask(const std::string& orderId);

	void CheckConditionOrderSendOrderTask(const std::string& orderId);

	int RegSystemInfo();

	void SetExchangeTime(KingstarAPI::CThostFtdcRspUserLoginField& userLogInField);

	virtual void SendDataDirect(int connId, const std::string& msg);

	void PrintOrderLogIfTouchedByConditionOrder(const Order& order);

	void PrintOtgOrderLog(std::shared_ptr<KingstarAPI::CThostFtdcOrderField> pOrder);

	void PrintNotOtgOrderLog(std::shared_ptr<KingstarAPI::CThostFtdcOrderField> pOrder);

	void PrintOtgTradeLog(std::shared_ptr<KingstarAPI::CThostFtdcTradeField> pTrade);

	void PrintNotOtgTradeLog(std::shared_ptr<KingstarAPI::CThostFtdcTradeField> pTrade);
};
