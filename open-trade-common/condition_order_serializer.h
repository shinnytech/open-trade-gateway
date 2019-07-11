#pragma once

#include "condition_order_type.h"
#include "rapid_serialize.h"

class SerializerConditionOrderData
	: public RapidSerialize::Serializer<SerializerConditionOrderData>
{
public:
	SerializerConditionOrderData();

	bool FilterMapItem(const std::string& key, ConditionOrder& value);

	using RapidSerialize::Serializer<SerializerConditionOrderData>::Serializer;

	void DefineStruct(ContingentCondition& d);

	void DefineStruct(ContingentOrder& d);

	void DefineStruct(ConditionOrder& d);

	void DefineStruct(ConditionOrderData& d);

	void DefineStruct(ConditionOrderHisData& d);

	void DefineStruct(req_insert_condition_order& d);

	void DefineStruct(req_cancel_condition_order& d);	

	void DefineStruct(req_pause_condition_order& d);

	void DefineStruct(req_resume_condition_order& d);

	void DefineStruct(qry_histroy_condition_order& d);

	void DefineStruct(time_span& d);

	void DefineStruct(weekday_time_span& d);

	void DefineStruct(condition_order_config& d);

	void DefineStruct(req_ccos_status& d);

	void DefineStruct(req_start_trade_instance& d);

	void DefineStruct(req_reconnect_trade_instance& d);
public:
	bool dump_all;
};


