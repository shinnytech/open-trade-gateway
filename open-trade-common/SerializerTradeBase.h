/////////////////////////////////////////////////////////////////////////
///@file SerializerTradeBase.h
///@brief	业务对象序列化
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

class SerializerTradeBase
	: public RapidSerialize::Serializer<SerializerTradeBase>
{
public:
	using RapidSerialize::Serializer<SerializerTradeBase>::Serializer;

	SerializerTradeBase()
	{
		dump_all = false;
	}

	bool dump_all;

	bool FilterMapItem(const std::string& key, Order& value)
	{
		if (dump_all)
			return true;
		bool b = value.changed;
		value.changed = false;
		return b;
	}

	bool FilterMapItem(const std::string& key, Trade& value)
	{
		if (dump_all)
			return true;
		bool b = value.changed;
		value.changed = false;
		return b;
	}

	bool FilterMapItem(const std::string& key, Position& value)
	{
		if (dump_all)
			return true;
		bool b = value.changed;
		value.changed = false;
		return b;
	}

	bool FilterMapItem(const std::string& key, Account& value)
	{
		if (dump_all)
			return true;
		bool b = value.changed;
		value.changed = false;
		return b;
	}

	bool FilterMapItem(const std::string& key, Bank& value)
	{
		if (dump_all)
			return true;
		bool b = value.changed;
		value.changed = false;
		return b;
	}

	bool FilterMapItem(const std::string& key, TransferLog& value)
	{
		if (dump_all)
			return true;
		bool b = value.changed;
		value.changed = false;
		return b;
	}

	void DefineStruct(ReqLogin& d);

	void DefineStruct(User& d);

	void DefineStruct(Bank& d);

	void DefineStruct(TransferLog& d);

	void DefineStruct(Account& d);

	void DefineStruct(Position& d);

	void DefineStruct(Order& d);

	void DefineStruct(Trade& d);

	void DefineStruct(Notify& d);

	void DefineStruct(qry_settlement_info& d);
};