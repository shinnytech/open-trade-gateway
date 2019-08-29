#include "condition_order_serializer.h"

SerializerConditionOrderData::SerializerConditionOrderData()
	:dump_all(false)
{
}

bool SerializerConditionOrderData::FilterMapItem(const std::string& key
	, ConditionOrder& value)
{
	if (dump_all)
		return true;
	bool b = value.changed;
	value.changed = false;
	return b;
}

void SerializerConditionOrderData::DefineStruct(ContingentCondition& d)
{
	AddItemEnum(d.contingent_type
		, ("contingent_type")
		, { { EContingentType::market_open, ("market_open") },
		{ EContingentType::time, ("time") },
		{ EContingentType::price, ("price") },
		{ EContingentType::price_range, ("price_range") },
		{ EContingentType::break_even, ("break_even") },
		});

	AddItem(d.exchange_id, ("exchange_id"));

	AddItem(d.instrument_id, ("instrument_id"));

	AddItem(d.is_touched, ("is_touched"));
	
	AddItem(d.contingent_price, ("contingent_price"));

	AddItemEnum(d.price_relation
		, ("price_relation")
		, { { EPriceRelationType::G, ("G") },
		{ EPriceRelationType::GE, ("GE") },
		{ EPriceRelationType::L, ("L") },
		{ EPriceRelationType::LE, ("LE") },
		});
	   
	AddItem(d.contingent_time, ("contingent_time"));

	AddItem(d.contingent_price_left, ("contingent_price_range_left"));

	AddItem(d.contingent_price_right, ("contingent_price_range_right"));

	AddItem(d.break_even_price, ("break_even_price"));

	AddItem(d.m_has_break_event, ("m_has_break_event"));
	
	AddItemEnum(d.break_even_direction
		, ("break_even_direction")
		, { { EOrderDirection::buy, ("BUY") },
		{ EOrderDirection::sell, ("SELL") },
		});	
}

void SerializerConditionOrderData::DefineStruct(ContingentOrder& d)
{
	AddItem(d.exchange_id, ("exchange_id"));

	AddItem(d.instrument_id, ("instrument_id"));

	AddItemEnum(d.direction
		, ("direction")
		, { { EOrderDirection::buy, ("BUY") },
		{ EOrderDirection::sell, ("SELL") },
		});

	AddItemEnum(d.offset
		, ("offset")
		, { { EOrderOffset::open, ("OPEN") },
		{ EOrderOffset::close, ("CLOSE") },		
		{ EOrderOffset::reverse, ("REVERSE") },
		});

	AddItem(d.close_today_prior,("close_today_prior"));

	AddItemEnum(d.volume_type
		, ("volume_type")
		, { { EVolumeType::num, ("NUM") },
		{ EVolumeType::close_all, ("CLOSE_ALL") },
		});

	AddItem(d.volume, ("volume"));

	AddItemEnum(d.price_type
		, ("price_type")
		, { { EPriceType::contingent, ("CONTINGENT") },
		{ EPriceType::consideration, ("CONSIDERATION") },
		{ EPriceType::market, ("MARKET") },
		{ EPriceType::over, ("OVER") },
		{ EPriceType::limit, ("LIMIT") },
		});

	AddItem(d.limit_price, ("limit_price"));
}


void SerializerConditionOrderData::DefineStruct(ConditionOrder& d)
{
	AddItem(d.order_id, ("order_id"));

	AddItem(d.trading_day, ("trading_day"));

	AddItem(d.insert_date_time, ("insert_date_time"));

	AddItem(d.condition_list, ("condition_list"));

	AddItemEnum(d.conditions_logic_oper
		, ("conditions_logic_oper")
		, { { ELogicOperator::logic_and, ("AND") },
		{ ELogicOperator::logic_or, ("OR") }, });

	AddItemEnum(d.conditions_logic_oper
		, ("conditions_logic_operator")
		, { { ELogicOperator::logic_and, ("AND") },
		{ ELogicOperator::logic_or, ("OR") }, });
	
	AddItem(d.order_list, ("order_list"));

	AddItemEnum(d.time_condition_type
		, ("time_condition_type")
		, { { ETimeConditionType::GFD, ("GFD") },
		{ ETimeConditionType::GTC, ("GTC") },
		{ ETimeConditionType::GTD, ("GTD") },
		});

	AddItem(d.GTD_date, ("GTD_date"));

	AddItem(d.is_cancel_ori_close_order, ("is_cancel_ori_close_order"));

	AddItem(d.is_cancel_ori_close_order, ("is_cancel_origin_close_order"));

	AddItemEnum(d.status
		, ("status")
		, { { EConditionOrderStatus::cancel, ("cancel") },
		{ EConditionOrderStatus::live, ("live") },
		{ EConditionOrderStatus::suspend, ("suspend") },
		{ EConditionOrderStatus::discard, ("discard") },
		{ EConditionOrderStatus::touched, ("touched") },
		});

	AddItem(d.touched_time,("touched_time"));
}

void SerializerConditionOrderData::DefineStruct(ConditionOrderData& d)
{
	AddItem(d.broker_id, ("broker_id"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.user_password, ("user_password"));
	AddItem(d.trading_day, ("trading_day"));
	AddItem(d.condition_orders, ("condition_orders"));
}

void SerializerConditionOrderData::DefineStruct(ConditionOrderHisData& d)
{
	AddItem(d.broker_id, ("broker_id"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.user_password, ("user_password"));
	AddItem(d.trading_day, ("trading_day"));
	AddItem(d.his_condition_orders, ("his_condition_orders"));
}

void SerializerConditionOrderData::DefineStruct(req_insert_condition_order& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.order_id, ("order_id"));
	AddItem(d.condition_list, ("condition_list"));
	AddItemEnum(d.conditions_logic_oper
		, ("conditions_logic_operator")
		, { { ELogicOperator::logic_and, ("AND") },
		{ ELogicOperator::logic_or, ("OR") }, });
	AddItem(d.order_list, ("order_list"));
	AddItemEnum(d.time_condition_type
		, ("time_condition_type")
		, { { ETimeConditionType::GFD, ("GFD") },
		{ ETimeConditionType::GTC, ("GTC") },
		{ ETimeConditionType::GTD, ("GTD") },
		});
	AddItem(d.GTD_date, ("GTD_date"));
	AddItem(d.is_cancel_ori_close_order, ("is_cancel_origin_close_order"));	
}

void SerializerConditionOrderData::DefineStruct(req_cancel_condition_order& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.order_id, ("order_id"));
}

void SerializerConditionOrderData::DefineStruct(req_pause_condition_order& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.order_id, ("order_id"));
}

void SerializerConditionOrderData::DefineStruct(req_resume_condition_order& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.order_id, ("order_id"));
}

void SerializerConditionOrderData::DefineStruct(qry_histroy_condition_order& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.action_day, ("action_day"));
}

void SerializerConditionOrderData::DefineStruct(time_span& d)
{
	AddItem(d.begin, ("begin"));
	AddItem(d.end, ("end"));
}

void SerializerConditionOrderData::DefineStruct(weekday_time_span& d)
{
	AddItem(d.weekday, ("weekday"));
	AddItem(d.time_span_list, ("timespan"));
}

void SerializerConditionOrderData::DefineStruct(condition_order_config& d)
{
	AddItem(d.run_server, ("run_server"));
	AddItem(d.max_new_cos_per_day, ("max_new_cos_per_day"));
	AddItem(d.max_valid_cos_all, ("max_valid_cos_all"));
	AddItem(d.auto_start_ctp_time, ("auto_start_ctp_time"));
	AddItem(d.auto_close_ctp_time, ("auto_close_ctp_time"));
	AddItem(d.auto_restart_process_time, ("auto_restart_process_time"));
}

void SerializerConditionOrderData::DefineStruct(req_ccos_status& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.run_server, ("run_server"));
}

void SerializerConditionOrderData::DefineStruct(req_start_trade_instance& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.bid, ("bid"));
	AddItem(d.user_name, ("user_name"));
	AddItem(d.password, ("password"));
}

void SerializerConditionOrderData::DefineStruct(req_reconnect_trade_instance& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.connIds, ("connIds"));
}