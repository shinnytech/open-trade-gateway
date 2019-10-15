/////////////////////////////////////////////////////////////////////////
///@file SerializerTradeBase.cpp
///@brief	业务对象序列化
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "SerializerTradeBase.h"

void SerializerTradeBase::DefineStruct(ReqLogin& d)
{
	AddItem(d.aid, "aid");
	AddItem(d.bid, "bid");
	AddItem(d.user_name, "user_name");
	AddItem(d.password, "password");
	AddItem(d.client_app_id, "client_app_id");
	AddItem(d.client_system_info, "client_system_info");	
	AddItem(d.broker_id, "broker_id");
	AddItem(d.front, "front");
	AddItem(d.client_ip, "client_ip");
	AddItem(d.client_port, "client_port");	
}

void SerializerTradeBase::DefineStruct(Bank& d)
{
	AddItem(d.bank_id, ("id"));
	AddItem(d.bank_name, ("name"));
}

void SerializerTradeBase::DefineStruct(qry_settlement_info& d)
{
	AddItem(d.aid, ("aid"));
	AddItem(d.trading_day, ("trading_day"));
	AddItem(d.user_name, ("user_name"));
	AddItem(d.settlement_info, ("settlement_info"));
}

void SerializerTradeBase::DefineStruct(TransferLog& d)
{
	AddItem(d.datetime, ("datetime"));
	AddItem(d.currency, ("currency"));
	AddItem(d.amount, ("amount"));
	AddItem(d.error_id, ("error_id"));
	AddItem(d.error_msg, ("error_msg"));
}

void SerializerTradeBase::DefineStruct(User& d)
{
	AddItem(d.user_id, ("user_id"));
	AddItem(d.trading_day, ("trading_day"));
	AddItem(d.m_trade_more_data, ("trade_more_data"));
	AddItem(d.m_accounts, ("accounts"));
	AddItem(d.m_positions, ("positions"));
	AddItem(d.m_orders, ("orders"));
	AddItem(d.m_trades, ("trades"));
	AddItem(d.m_banks, ("banks"));
	AddItem(d.m_transfers, ("transfers"));
}

void SerializerTradeBase::DefineStruct(Notify& d)
{
	AddItemEnum(d.type, ("type"), {
		{ kNotifyTypeMessage, ("MESSAGE") },
		{ kNotifyTypeText, ("TEXT") },
		});
	AddItem(d.code, ("code"));
	AddItem(d.content, ("content"));
}

void SerializerTradeBase::DefineStruct(Account& d)
{
	AddItem(d.user_id, ("user_id"));
	AddItem(d.currency, ("currency"));

	AddItem(d.pre_balance, ("pre_balance"));

	AddItem(d.deposit, ("deposit"));
	AddItem(d.withdraw, ("withdraw"));
	AddItem(d.close_profit, ("close_profit"));
	AddItem(d.commission, ("commission"));
	AddItem(d.premium, ("premium"));
	AddItem(d.static_balance, ("static_balance"));

	AddItem(d.position_profit, ("position_profit"));
	AddItem(d.float_profit, ("float_profit"));

	AddItem(d.balance, ("balance"));

	AddItem(d.margin, ("margin"));
	AddItem(d.frozen_margin, ("frozen_margin"));
	AddItem(d.frozen_commission, ("frozen_commission"));
	AddItem(d.frozen_premium, ("frozen_premium"));
	AddItem(d.available, ("available"));
	AddItem(d.risk_ratio, ("risk_ratio"));
}

void SerializerTradeBase::DefineStruct(Position& d)
{
	AddItem(d.user_id, ("user_id"));
	AddItem(d.exchange_id, ("exchange_id"));
	AddItem(d.instrument_id, ("instrument_id"));

	AddItem(d.volume_long_today, ("volume_long_today"));
	AddItem(d.volume_long_his, ("volume_long_his"));
	AddItem(d.volume_long, ("volume_long"));

	AddItem(d.volume_long_frozen_today, ("volume_long_frozen_today"));
	AddItem(d.volume_long_frozen_his, ("volume_long_frozen_his"));
	AddItem(d.volume_long_frozen, ("volume_long_frozen"));

	AddItem(d.volume_short_today, ("volume_short_today"));
	AddItem(d.volume_short_his, ("volume_short_his"));
	AddItem(d.volume_short, ("volume_short"));

	AddItem(d.volume_short_frozen_today, ("volume_short_frozen_today"));
	AddItem(d.volume_short_frozen_his, ("volume_short_frozen_his"));
	AddItem(d.volume_short_frozen, ("volume_short_frozen"));

	AddItem(d.volume_long_yd, ("volume_long_yd"));
	AddItem(d.volume_short_yd, ("volume_short_yd"));

	AddItem(d.pos_long_his, ("pos_long_his"));
	AddItem(d.pos_long_today, ("pos_long_today"));
	AddItem(d.pos_short_his, ("pos_short_his"));
	AddItem(d.pos_short_today, ("pos_short_today"));

	AddItem(d.open_price_long, ("open_price_long"));
	AddItem(d.open_price_short, ("open_price_short"));

	AddItem(d.open_cost_long, ("open_cost_long"));
	AddItem(d.open_cost_short, ("open_cost_short"));

	AddItem(d.position_price_long, ("position_price_long"));
	AddItem(d.position_price_short, ("position_price_short"));

	AddItem(d.position_cost_long, ("position_cost_long"));
	AddItem(d.position_cost_short, ("position_cost_short"));

	AddItem(d.last_price, ("last_price"));

	AddItem(d.float_profit_long, ("float_profit_long"));
	AddItem(d.float_profit_short, ("float_profit_short"));

	AddItem(d.float_profit, ("float_profit"));

	AddItem(d.position_profit_long, ("position_profit_long"));
	AddItem(d.position_profit_short, ("position_profit_short"));

	AddItem(d.position_profit, ("position_profit"));

	AddItem(d.margin_long, ("margin_long"));
	AddItem(d.margin_short, ("margin_short"));
	AddItem(d.margin, ("margin"));

	/*

	//成本, 现价与盈亏
			
	double open_cost_long_his; //多头开仓市值(昨仓)

	double open_cost_long_today; //多头开仓市值(今仓)

	double open_cost_short_today; //空头开仓市值(今仓)	

	double open_cost_short_his; //空头开仓市值(昨仓)
	

	double position_cost_long_today; //多头持仓成本(今仓)

	double position_cost_long_his; //多头持仓成本(昨仓)

	double position_cost_short_today; //空头持仓成本(今仓)	

	double position_cost_short_his; //空头持仓成本(昨仓)
	   	
	//保证金占用	

	double margin_long_today;//多头保证金占用(今仓)

	double margin_short_today;//空头保证金占用(今仓)

	double margin_long_his;//多头保证金占用(昨仓)

	double margin_short_his;//空头保证金占用(昨仓)
	
	double frozen_margin;//冻结保证金
	*/
}

void SerializerTradeBase::DefineStruct(Order& d)
{
	AddItem(d.seqno, ("seqno"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.order_id, ("order_id"));
	AddItem(d.exchange_id, ("exchange_id"));
	AddItem(d.instrument_id, ("instrument_id"));
	AddItemEnum(d.direction, ("direction"), {
		{ kDirectionBuy, ("BUY") },
		{ kDirectionSell, ("SELL") },
		});
	AddItemEnum(d.offset, ("offset"), {
		{ kOffsetOpen, ("OPEN") },
		{ kOffsetClose, ("CLOSE") },
		{ kOffsetCloseToday, ("CLOSETODAY") },
		});
	AddItem(d.volume_orign, ("volume_orign"));
	AddItemEnum(d.price_type, ("price_type"), {
		{ kPriceTypeLimit, ("LIMIT") },
		{ kPriceTypeAny, ("ANY") },
		{ kPriceTypeBest, ("BEST") },
		{ kPriceTypeFiveLevel, ("FIVELEVEL") },
		});
	AddItem(d.limit_price, ("limit_price"));
	AddItemEnum(d.time_condition, ("time_condition"), {
		{ kOrderTimeConditionIOC, ("IOC") },
		{ kOrderTimeConditionGFS, ("GFS") },
		{ kOrderTimeConditionGFD, ("GFD") },
		{ kOrderTimeConditionGTD, ("GTD") },
		{ kOrderTimeConditionGTC, ("GTC") },
		{ kOrderTimeConditionGFA, ("GFA") },
		});
	AddItemEnum(d.volume_condition, ("volume_condition"), {
		{ kOrderVolumeConditionAny, ("ANY") },
		{ kOrderVolumeConditionMin, ("MIN") },
		{ kOrderVolumeConditionAll, ("ALL") },
		});
	AddItem(d.insert_date_time, ("insert_date_time"));
	AddItem(d.exchange_order_id, ("exchange_order_id"));

	AddItemEnum(d.status, ("status"), {
		{ kOrderStatusAlive, ("ALIVE") },
		{ kOrderStatusFinished, ("FINISHED") },
		});
	AddItem(d.volume_left, ("volume_left"));
	AddItem(d.last_msg, ("last_msg"));
}

void SerializerTradeBase::DefineStruct(Trade& d)
{
	AddItem(d.seqno, ("seqno"));
	AddItem(d.user_id, ("user_id"));
	AddItem(d.trade_id, ("trade_id"));
	AddItem(d.exchange_id, ("exchange_id"));
	AddItem(d.instrument_id, ("instrument_id"));
	AddItem(d.order_id, ("order_id"));
	AddItem(d.exchange_trade_id, ("exchange_trade_id"));

	AddItemEnum(d.direction, ("direction"), {
		{ kDirectionBuy, ("BUY") },
		{ kDirectionSell, ("SELL") },
		});
	AddItemEnum(d.offset, ("offset"), {
		{ kOffsetOpen, ("OPEN") },
		{ kOffsetClose, ("CLOSE") },
		{ kOffsetCloseToday, ("CLOSETODAY") },
		});
	AddItem(d.volume, ("volume"));
	AddItem(d.price, ("price"));
	AddItem(d.trade_date_time, ("trade_date_time"));
	AddItem(d.commission, ("commission"));
}
