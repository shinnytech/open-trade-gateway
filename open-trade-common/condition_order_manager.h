#pragma once

#include "condition_order_type.h"

struct IConditionOrderCallBack
{	
};

class ConditionOrderManager
{
public:
	ConditionOrderManager(IConditionOrderCallBack& callBack);

	~ConditionOrderManager();	

	void Load(const std::string& userKey);

	void Save(const std::string& userKey);

	void InsertConditionOrder(const std::string& msg);

	void CancelConditionOrder(const std::string& msg);

	void PauseConditionOrder(const std::string& msg);

	void QryHisConditionOrder(const std::string& msg);
private:
	IConditionOrderCallBack& m_callBack;
};

