#pragma once

#include "condition_order_type.h"

#include <set>

struct IConditionOrderCallBack
{	
	virtual void SendDataDirect(int connId, const std::string& msg) = 0;

	virtual void OnUserDataChange() = 0;
	
	virtual void OutputNotifyAll(long notify_code, const std::string& ret_msg
		, const char* level	, const char* type) = 0;

	virtual void OnTouchConditionOrder(const ConditionOrder& order)=0;
};

class ConditionOrderManager
{
public:
	ConditionOrderManager(const std::string& userKey
		,ConditionOrderData& condition_order_data
		,ConditionOrderHisData& condition_order_his_data
		,IConditionOrderCallBack& callBack);

	~ConditionOrderManager();	

	void Load(const std::string& bid
		, const std::string& user_id
		, const std::string& user_password
		, const std::string& trading_day);

	void NotifyPasswordUpdate(const std::string& strOldPassword,
		const std::string& strNewPassword);
	
	void InsertConditionOrder(const std::string& msg);

	void CancelConditionOrder(const std::string& msg);

	void PauseConditionOrder(const std::string& msg);

	void ResumeConditionOrder(const std::string& msg);
	
	void ChangeCOSStatus(const std::string& msg);
		
	void OnMarketOpen(const std::string& strSymbol,EInstrumentStatus instStatus);

	void OnCheckTime();

	void OnCheckPrice();

	void OnUpdateInstrumentTradeStatus(const InstrumentTradeStatusInfo& instTradeStatusInfo);

	TInstOrderIdListMap& GetOpenmarketCoMap();

	std::set<std::string>& GetTimeCoSet();

	TInstOrderIdListMap& GetPriceCoMap();

	void SetExchangeTime(int localTime,int SHFETime, int DCETime, int INETime, int FFEXTime,int CZCETime);

	void QryHisConditionOrder(int connId, const std::string& msg);
private:
	std::string m_userKey;

	std::string m_user_file_path;

	ConditionOrderData& m_condition_order_data;

	//当前交易日生成的条件单数量
	int m_current_day_condition_order_count;
	
	//当前有效条件单数量
	int m_current_valid_condition_order_count;

	ConditionOrderHisData& m_condition_order_his_data;

	bool m_run_server;

	TInstOrderIdListMap m_openmarket_condition_order_map;

	std::set<std::string> m_time_condition_order_set;

	TInstOrderIdListMap m_price_condition_order_map;

	IConditionOrderCallBack& m_callBack;

	int m_localTime;

	int m_SHFETime;

	int m_DCETime;

	int m_INETime;

	int m_FFEXTime;

	int m_CZCETime;

	TInstrumentTradeStatusInfoMap _instrumentTradeStatusInfoMap;	

	int GetExchangeTime(const std::string& exchange_id);
	
	bool ValidConditionOrder(ConditionOrder& order);	

	void SaveHistory();

	void SaveCurrent();
	
	void BuildConditionOrderIndex();

	bool IsContingentConditionTouched(
		std::vector<ContingentCondition>& condition_list
		,ELogicOperator logicOperator);	

	int GetTouchedTime(ConditionOrder& conditionOrder);

	bool InstrumentLastTradeStatusIsContinousTrading(const std::string& instId);
};

