#pragma once

#include "condition_order_type.h"

#include <set>


struct IConditionOrderCallBack
{	
	virtual void SendConditionOrderData(const std::string& msg) = 0;

	virtual void SendConditionOrderData(int connectId,const std::string& msg) = 0;

	virtual void OutputNotifyAll(long notify_code, const std::string& ret_msg
		, const char* level	, const char* type) = 0;

	virtual void OnTouchConditionOrder(const std::vector<ContingentOrder>& order_list
		, bool is_cancel_ori_close_order)=0;
};

class ConditionOrderManager
{
public:
	ConditionOrderManager(const std::string& userKey
		,IConditionOrderCallBack& callBack);

	~ConditionOrderManager();	

	void Load(const std::string& bid
		, const std::string& user_id
		, const std::string& user_password
		, const std::string& trading_day);
	
	void InsertConditionOrder(const std::string& msg);

	void CancelConditionOrder(const std::string& msg);

	void PauseConditionOrder(const std::string& msg);

	void ResumeConditionOrder(const std::string& msg);

	void QryHisConditionOrder(const std::string& msg);

	void ChangeCOSStatus(const std::string& msg);

	void SendAllConditionOrderDataImd(int connectId);

	void OnMarketOpen(const std::string& strSymbol);

	void OnCheckTime(long long currentTime);

	void OnCheckPrice();

	TInstOrderIdListMap& GetOpenmarketCoMap();

	std::set<std::string>& GetTimeCoSet();

	TInstOrderIdListMap& GetPriceCoMap();
private:
	std::string m_userKey;

	std::string m_user_file_path;

	ConditionOrderData m_condition_order_data;

	//当前交易日生成的条件单数量
	int m_current_day_condition_order_count;
	
	//当前有效条件单数量
	int m_current_valid_condition_order_count;

	ConditionOrderHisData m_condition_order_his_data;

	bool m_run_server;

	TInstOrderIdListMap m_openmarket_condition_order_map;

	std::set<std::string> m_time_condition_order_set;

	TInstOrderIdListMap m_price_condition_order_map;

	IConditionOrderCallBack& m_callBack;	
	
	void SendAllConditionOrderData();

	void SendConditionOrderData();

	bool ValidConditionOrder(const ConditionOrder& order);	

	void SaveHistory();

	void SaveCurrent();

	void LoadConditionOrderConfig();

	void BuildConditionOrderIndex();

	bool IsContingentConditionTouched(std::vector<ContingentCondition>& condition_list
		,ELogicOperator logicOperator);
};

