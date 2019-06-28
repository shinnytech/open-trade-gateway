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

	void DefineStruct(req_insert_condition_order& d);
public:
	bool dump_all;
};


