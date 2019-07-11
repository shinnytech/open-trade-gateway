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

req_cancel_condition_order::req_cancel_condition_order()
	:aid("")
	,user_id("")
	,order_id("")
{

}

req_pause_condition_order::req_pause_condition_order()
	:aid("")
	, user_id("")
	, order_id("")
{

}

req_resume_condition_order::req_resume_condition_order()
	:aid("")
	, user_id("")
	, order_id("")
{

}

qry_histroy_condition_order::qry_histroy_condition_order()
	:aid("")
	, user_id("")
	, action_day(0)
{

}

time_span::time_span()
	:begin(0)
	,end(0)
{
}

weekday_time_span::weekday_time_span()
	: weekday(0)
	,time_span_list()
{
}

condition_order_config::condition_order_config()
	: run_server(true)
	, auto_start_ctp_time()
	, has_auto_start_ctp(false)
	, auto_close_ctp_time()
	, has_auto_close_ctp(false)
	, auto_restart_process_time()
{
}

req_ccos_status::req_ccos_status()
	:aid("req_ccos_status")
	,run_server(false)
{
}

req_start_trade_instance::req_start_trade_instance()
	: aid("req_start_ctp")
	, bid("")
	, user_name("")
	, password("")
{
}

req_reconnect_trade_instance::req_reconnect_trade_instance()
	:aid("req_reconnect_trade")
	,connIds()
{

}