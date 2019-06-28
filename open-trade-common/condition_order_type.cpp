#include "condition_order_type.h"

void ContingentCondition::UpdateMarketStatus(EMarketStatus marketStatus)
{
	if ((marketStatus == EMarketStatus::continous)
		&& (market_status != EMarketStatus::continous))
	{
		is_touched = true;
	}
	market_status = marketStatus;
}

void ContingentCondition::UpdateMarketTime(int marketTime)
{
	if (marketTime >= contingent_time)
	{
		is_touched = true;
	}
}

void ContingentCondition::UpdatePrice(double price)
{
	//TODO::
	switch (price_relation_type)
	{
	case EPriceRelationType::G:
		break;
	case EPriceRelationType::GE:
		break;
	case EPriceRelationType::L:
		break;
	case EPriceRelationType::LE:
		break;
	default:
		break;
	}
}

req_insert_condition_order::req_insert_condition_order()
	:aid("")
	,user_id("")
	,order_id("")
	,condition_list()
	,conditions_logic_oper(ELogicOperator::logic_and)
	,order_list()
	,time_condition_type(ETimeConditionType::GFD)
	,GTD_date(0)
	,is_cancel_ori_close_order(false)
{
}