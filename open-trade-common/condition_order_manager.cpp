#include "condition_order_manager.h"
#include "condition_order_serializer.h"

ConditionOrderManager::ConditionOrderManager(IConditionOrderCallBack& callBack)
	:m_callBack(callBack)
{
}

ConditionOrderManager::~ConditionOrderManager()
{
}

void ConditionOrderManager::Load(const std::string& userKey)
{
	//TODO::加载条件单
}

void ConditionOrderManager::Save(const std::string& userKey)
{
	//TODO::保存条件单
}

void ConditionOrderManager::InsertConditionOrder(const std::string& msg)
{
	//TODO::插入条件单
}

void ConditionOrderManager::CancelConditionOrder(const std::string& msg)
{
	//TODO::撤销条件单
}

void ConditionOrderManager::PauseConditionOrder(const std::string& msg)
{
	//TODO::暂停条件单
}

void ConditionOrderManager::QryHisConditionOrder(const std::string& msg)
{
	//TODO::查询历史条件单
}