/////////////////////////////////////////////////////////////////////////
///@file ctp_define.h
///@brief	CTP接口相关数据结构及序列化函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "ctp/ThostFtdcTraderApi.h"
#include "datetime.h"
#include "rapid_serialize.h"
#include "condition_order_type.h"

struct LocalOrderKey
{
	std::string user_id;
	std::string order_id;
	bool operator < (const LocalOrderKey& key) const
	{
		if (user_id != key.user_id)
			return user_id < key.user_id;
		return order_id < key.order_id;
	}
};

struct RemoteOrderKey
{
	std::string exchange_id;
	std::string instrument_id;
	int session_id;
	int front_id;
	std::string order_ref;
	std::string order_sys_id;
	bool operator < (const RemoteOrderKey& key) const
	{
		if (session_id != key.session_id)
			return session_id < key.session_id;
		if (front_id != key.front_id)
			return front_id < key.front_id;
		return order_ref < key.order_ref;
	}
};

struct ServerOrderInfo
{
	ServerOrderInfo()
		:ExchangeId("")
		, InstrumentId("")
		, VolumeOrigin(0)
		, VolumeLeft(0)
		, OrderLocalID("")
		, OrderSysID("")
	{
	}

	std::string ExchangeId;

	std::string InstrumentId;

	int VolumeOrigin;

	int VolumeLeft;

	std::string	OrderLocalID;

	std::string	OrderSysID;
};

struct OrderKeyPair
{
	LocalOrderKey local_key;

	RemoteOrderKey remote_key;
};

struct OrderKeyFile
{
	std::string trading_day;

	std::vector<OrderKeyPair> items;
};

struct CtpActionInsertOrder
{
	CtpActionInsertOrder()
	{
		memset(&f, 0, sizeof(f));
	}
	LocalOrderKey local_key;
	CThostFtdcInputOrderField f;
};

struct CtpActionCancelOrder
{
	CtpActionCancelOrder()
	{
		memset(&f, 0, sizeof(f));
	}
	LocalOrderKey local_key;
	CThostFtdcInputOrderActionField f;
};

struct CtpChangeTradingAccountPassword
{
	CtpChangeTradingAccountPassword()
		:account_id("")
		,old_password("")
		,new_password("")
		,currency_id("")
	{
	}

	std::string account_id;

	std::string old_password;

	std::string new_password;

	std::string currency_id;
};

class SerializerCtp
	: public RapidSerialize::Serializer<SerializerCtp>
{
public:
	using RapidSerialize::Serializer<SerializerCtp>::Serializer;

	void DefineStruct(OrderKeyFile& d);

	void DefineStruct(OrderKeyPair& d);

	void DefineStruct(LocalOrderKey& d);

	void DefineStruct(RemoteOrderKey& d);

	void DefineStruct(CtpActionInsertOrder& d);

	void DefineStruct(CtpActionCancelOrder& d);

	void DefineStruct(CThostFtdcUserPasswordUpdateField& d);

	void DefineStruct(CThostFtdcReqTransferField& d);

	void DefineStruct(CThostFtdcTransferSerialField& d);

	void DefineStruct(CtpChangeTradingAccountPassword& d);
};

class SerializerLogCtp
	: public RapidSerialize::Serializer<SerializerLogCtp>
{
public:
	using RapidSerialize::Serializer<SerializerLogCtp>::Serializer;

	void DefineStruct(CThostFtdcRspAuthenticateField& d);

	void DefineStruct(CThostFtdcRspUserLoginField& d);

	void DefineStruct(CThostFtdcSettlementInfoConfirmField& d);

	void DefineStruct(CThostFtdcSettlementInfoField& d);

	void DefineStruct(CThostFtdcUserPasswordUpdateField& d);

	void DefineStruct(CThostFtdcInputOrderField& d);

	void DefineStruct(CThostFtdcInputOrderActionField& d);

	void DefineStruct(CThostFtdcOrderActionField& d);

	void DefineStruct(CThostFtdcInvestorPositionField& d);

	void DefineStruct(CThostFtdcBrokerTradingParamsField& d);

	void DefineStruct(CThostFtdcTradingAccountField& d);

	void DefineStruct(CThostFtdcContractBankField& d);

	void DefineStruct(CThostFtdcAccountregisterField& d);

	void DefineStruct(CThostFtdcTransferSerialField& d);

	void DefineStruct(CThostFtdcRspTransferField& d);

	void DefineStruct(CThostFtdcReqTransferField& d);

	void DefineStruct(CThostFtdcOrderField& d);

	void DefineStruct(CThostFtdcTradeField& d);

	void DefineStruct(CThostFtdcTradingNoticeInfoField& d);

	void DefineStruct(CThostFtdcInstrumentStatusField& d);

	void DefineStruct(CThostFtdcTradingAccountPasswordUpdateField& d);	
};

struct ctp_condition_order_task
{
	ctp_condition_order_task();

	bool has_order_to_cancel;

	std::vector<CtpActionCancelOrder> orders_to_cancel;

	bool has_first_orders_to_send;

	std::vector <CtpActionInsertOrder> first_orders_to_send;

	bool has_second_orders_to_send;

	std::vector <CtpActionInsertOrder> second_orders_to_send;

	ConditionOrder condition_order;
};
