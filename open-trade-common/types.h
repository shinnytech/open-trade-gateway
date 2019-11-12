/////////////////////////////////////////////////////////////////////////
///@file types.h
///@brief	基础类型定义
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "rapid_serialize.h"

#include <string>
#include <vector>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

const int kOptionClassCall = 1;
const int kOptionClassPut = 2;

const int kProductClassAll = 0xFFFFFFFF;
const int kProductClassFutures = 0x00000001;
const int kProductClassOptions = 0x00000002;
const int kProductClassCombination = 0x00000004;
const int kProductClassFOption = 0x00000008;
const int kProductClassFutureIndex = 0x00000010;
const int kProductClassFutureContinuous = 0x00000020;
const int kProductClassStock = 0x00000040;
const int kProductClassFuturePack = kProductClassFutures | kProductClassFutureIndex | kProductClassFutureContinuous;

const int kExchangeShfe = 0x00000001;
const int kExchangeCffex = 0x00000002;
const int kExchangeCzce = 0x00000004;
const int kExchangeDce = 0x00000008;
const int kExchangeIne = 0x00000010;
const int kExchangeKq = 0x00000020;
const int kExchangeSswe = 0x00000040;
const int kExchangeUser = 0x10000000;
const int kExchangeAll = 0xFFFFFFFF;

const int MAX_MSG_NUMS_IN = 10;

const int MAX_MSG_NUMS_OUT = 20;

const int MAX_MSG_LENTH = 1024;

const std::string BEGIN_OF_PACKAGE = "#BEGIN_OF_PACKAGE#";

const std::string END_OF_PACKAGE = "#END_OF_PACKAGE#";

const std::string CLOSE_CONNECTION_MSG = "#CLOSE_CONNECTION_MSG#";

const std::string GATE_WAY_MSG_QUEUE_NAME = "OPEN_TRADE_GATE_WAY_MSG_QUEUE";

const std::string USER_PRODUCT_INFO_NAME = "SHINNYOTG";

struct Instrument
{
	Instrument()
		:expired(false)
		,product_class(kProductClassFutures)
		,volume_multiple(0)
		,volume(0)
		,margin(0.0)
		,commission(0.0)
		,price_tick(NAN)
		,last_price(NAN)
		,pre_settlement(NAN)
		,upper_limit(NAN)
		,lower_limit(NAN)
		,ask_price1(NAN)
		,bid_price1(NAN)
		,settlement(NAN)
		,pre_close(NAN)
	{	
	}

	bool expired;

	long product_class;
	long volume_multiple;
	long volume;

	volatile double margin;
	volatile double commission;
	volatile double price_tick;

	volatile double last_price;
	volatile double pre_settlement;
	volatile double upper_limit;
	volatile double lower_limit;
	volatile double ask_price1;
	volatile double bid_price1;
	volatile double settlement;
	volatile double pre_close;
};

const int kNotifyTypeMessage = 1;
const int kNotifyTypeText = 2;

const int kOrderTypeTrade = 1;
const int kOrderTypeSwap = 2;
const int kOrderTypeExecute = 3;
const int kOrderTypeQuote = 4;

const int kTradeTypeTakeProfit = 1;
const int kTradeTypeStopLoss = 2;

const int kDirectionBuy = 1;
const int kDirectionSell = -1;
const int kDirectionUnknown = 0;

const int kOffsetOpen = 1;
const int kOffsetClose = -1;
const int kOffsetCloseToday = -2;
const int kOffsetUnknown = 0;

const int kOrderStatusAlive = 1;
const int kOrderStatusFinished = 2;

const int kPriceTypeUnknown = 0;
const int kPriceTypeLimit = 1;
const int kPriceTypeAny = 2;
const int kPriceTypeBest = 3;
const int kPriceTypeFiveLevel = 4;

const int kOrderVolumeConditionAny = 1;
const int kOrderVolumeConditionMin = 2;
const int kOrderVolumeConditionAll = 3;

const int kOrderTimeConditionIOC = 1;
const int kOrderTimeConditionGFS = 2;
const int kOrderTimeConditionGFD = 3;
const int kOrderTimeConditionGTD = 4;
const int kOrderTimeConditionGTC = 5;
const int kOrderTimeConditionGFA = 6;

const int kHedgeFlagSpeculation = 1;
const int kHedgeFlagArbitrage = 2;
const int kHedgeFlagHedge = 3;
const int kHedgeFlagMarketMaker = 4;

struct BrokerConfig
{
	BrokerConfig()
		:is_fens(false)
	{
	}

	std::string broker_name;
	std::string broker_type;
	bool is_fens;
	std::string ctp_broker_id;
	std::vector<std::string> trading_fronts;
	std::string product_info;
	std::string auth_code;
};

struct ReqLogin
{
	ReqLogin()
		:client_port(0)
	{
	}

	std::string aid;
	std::string bid;        //对应服务器brokers配置文件中的id号
	std::string user_name;  //用户输入的用户名
	std::string password;   //用户输入的密码
	BrokerConfig broker;    //用户可以强制输入broker信息
	std::string client_ip;
	int client_port;
	std::string client_system_info;
	std::string client_app_id;
	//为了支持次席而添加的功能
	std::string broker_id;
	std::string front;
};

struct qry_settlement_info
{
	qry_settlement_info() :
		aid("")
		,trading_day(0)
		,user_name("")
		,settlement_info("")
	{
	}

	std::string aid;

	int trading_day;

	std::string user_name;

	std::string settlement_info;
};

struct Order
{
	Order()
	{
		//委托单初始属性(由下单者在下单前确定, 不再改变)
		direction = kDirectionBuy;
		offset = kOffsetOpen;
		volume_orign = 0;
		price_type = kPriceTypeLimit;
		limit_price = 0.0;
		volume_condition = kOrderVolumeConditionAny;
		time_condition = kOrderTimeConditionGFD;

		//下单后获得的信息(由期货公司返回, 不会改变)
		insert_date_time = 0;
		frozen_margin = 0.0;

		//委托单当前状态
		status = kOrderStatusAlive;
		volume_left = 0;

		//内部使用
		changed = true;
	}

	//委托单初始属性(由下单者在下单前确定, 不再改变)
	std::string user_id;

	std::string order_id;

	std::string exchange_id;

	std::string instrument_id;

	std::string symbol() const
	{
		return exchange_id + "." + instrument_id;
	}

	long direction;
	long offset;
	int volume_orign;
	long price_type;
	double limit_price;
	long time_condition;
	long volume_condition;

	//下单后获得的信息(由期货公司返回, 不会改变)
	long long insert_date_time;
	std::string exchange_order_id;
	double frozen_margin;

	//委托单当前状态
	long status;
	int volume_left;
	std::string last_msg;

	//内部使用
	int seqno;
	bool changed;
};

struct Trade
{
	Trade()
	{
		direction = kDirectionBuy;
		offset = kOffsetOpen;
		volume = 0;
		price = 0.0;
		trade_date_time = 0; //epoch nano
		commission = 0;

		//内部使用
		changed = true;
	}

	std::string user_id;
	std::string trade_id;
	std::string exchange_id;
	std::string instrument_id;

	std::string symbol() const
	{
		return exchange_id + "." + instrument_id;
	}

	std::string order_id;
	std::string exchange_trade_id;

	int direction;
	int offset;
	int volume;
	double price;
	long long trade_date_time; //epoch nano
	double commission;

	//内部使用
	int seqno;
	bool changed;
};

struct Position
{
	Position()
	{
		//持仓手数与冻结手数
		volume_long_today = 0;
		volume_long_his = 0;
		volume_long = 0;
		volume_long_frozen_today = 0;
		volume_long_frozen_his = 0;
		volume_long_frozen = 0;
		volume_short_today = 0;
		volume_short_his = 0;
		volume_short = 0;
		volume_short_frozen_today = 0;
		volume_short_frozen_his = 0;
		volume_short_frozen = 0;

		volume_long_yd = 0;
		volume_short_yd = 0;

		pos_long_his = 0;
		pos_long_today = 0;
		pos_short_his = 0;
		pos_short_today = 0;

		//成本, 现价与盈亏
		open_cost_long_today = 0.0;
		open_cost_short_today = 0.0;
		open_cost_long_his = 0.0;
		open_cost_short_his = 0.0;
		position_cost_long_today = 0.0;
		position_cost_short_today = 0.0;
		position_cost_long_his = 0.0;
		position_cost_short_his = 0.0;

		open_price_long = 0.0;
		open_price_short = 0.0;
		open_cost_long = 0.0;
		open_cost_short = 0.0;
		position_price_long = 0.0; //多头持仓均价
		position_price_short = 0.0; //空头持仓均价
		position_cost_long = 0.0; //多头持仓成本
		position_cost_short = 0.0; //空头持仓成本
		last_price = 0.0;
		float_profit_long = 0.0;
		float_profit_short = 0.0;
		float_profit = 0.0;
		position_profit_long = 0.0;
		position_profit_short = 0.0;
		position_profit = 0.0;

		//保证金占用
		margin_long = 0.0;
		margin_short = 0.0;
		margin_long_today = 0.0;
		margin_short_today = 0.0;
		margin_long_his = 0.0;
		margin_short_his = 0.0;
		margin = 0.0;
		frozen_margin = 0.0;

		//内部使用
		ins = NULL;
		changed = true;
		market_status = 1;
	}

	//交易所和合约代码
	std::string user_id;
	std::string exchange_id;
	std::string instrument_id;
	std::string symbol() const
	{
		return exchange_id + "." + instrument_id;
	}

	//多头持仓手数
	int volume_long_today;
	int volume_long_his;
	int volume_long;

	//多头冻结手数
	int volume_long_frozen_today;
	int volume_long_frozen_his;
	int volume_long_frozen;
	
	//空头持仓手数
	int volume_short_today;
	int volume_short_his;
	int volume_short;

	//空头冻结手数
	int volume_short_frozen_today;
	int volume_short_frozen_his;
	int volume_short_frozen;

	//今日开盘前的双向持仓手数
	int volume_long_yd;
	int volume_short_yd;
		
	//今昨仓持仓情况
	int pos_long_his;//多头持仓(昨仓)
	int pos_long_today;//多头持仓(今仓)
	int pos_short_his;//空头持仓(昨仓)
	int pos_short_today;//空头持仓(今仓)

	//成本, 现价与盈亏
	double open_price_long; //多头开仓均价

	double open_price_short; //空头开仓均价

	double open_cost_long; //多头开仓市值

	double open_cost_short; //空头开仓市值
	
	double open_cost_long_his; //多头开仓市值(昨仓)

	double open_cost_long_today; //多头开仓市值(今仓)

	double open_cost_short_today; //空头开仓市值(今仓)	

	double open_cost_short_his; //空头开仓市值(昨仓)

	double position_price_long; //多头持仓均价

	double position_price_short; //空头持仓均价

	double position_cost_long; //多头持仓成本

	double position_cost_short; //空头持仓成本

	double position_cost_long_today; //多头持仓成本(今仓)

	double position_cost_long_his; //多头持仓成本(昨仓)

	double position_cost_short_today; //空头持仓成本(今仓)	

	double position_cost_short_his; //空头持仓成本(昨仓)

	double last_price;//最新价

	double float_profit_long;//多头浮动盈亏

	double float_profit_short;//空头浮动盈亏

	double float_profit;//浮动盈亏

	double position_profit_long;//多头持仓盈亏

	double position_profit_short;//空头持仓盈亏

	double position_profit;//持仓盈亏

	//保证金占用
	double margin_long;//多头保证金占用

	double margin_short;//空头保证金占用

	double margin_long_today;//多头保证金占用(今仓)

	double margin_short_today;//空头保证金占用(今仓)

	double margin_long_his;//多头保证金占用(昨仓)

	double margin_short_his;//空头保证金占用(昨仓)

	double margin;//保证金占用

	double frozen_margin;//冻结保证金
	
	//内部使用
	const Instrument* ins;

	bool changed;

	//当前市场状态:0,开盘前;1:交易中;2:收盘后
	int market_status;
};

struct Account
{
	Account()
	{
		//本交易日开盘前状态
		pre_balance = 0.0;

		//本交易日内已发生事件的影响
		deposit = 0.0;
		withdraw = 0.0;
		close_profit = 0.0;
		commission = 0.0;
		premium = 0.0;
		static_balance = 0.0;

		//当前持仓盈亏
		float_profit = 0.0;
		position_profit = 0.0;

		//当前权益
		balance = 0.0;

		//保证金占用, 冻结及风险度
		margin = 0.0;
		frozen_margin = 0.0;
		frozen_commission = 0.0;
		frozen_premium = 0.0;
		available = 0.0;
		risk_ratio = 0.0;

		//内部使用
		changed = true;
	}

	//账号
	std::string user_id;

	//币种
	std::string currency;

	//本交易日开盘前状态(昨权益)
	double pre_balance;

	//本交易日内已发生事件的影响	

	double deposit;//入金

	double withdraw;//出金

	double close_profit;//平仓盈亏

	double commission;//手续费

	double premium;//暂时无用

	double static_balance;//静态权益

	//当前持仓盈亏
	double position_profit;//持仓盈亏

	double float_profit;//浮动盈亏

	//当前权益(动态权益)
	double balance;

	//保证金占用, 冻结及风险度
	double margin;//保证金
	double frozen_margin;//冻结保证金
	double frozen_commission;//冻结手续费
	double frozen_premium;//暂时无用
	double available;//可用资金
	double risk_ratio;//风险度

	//内部使用
	bool changed;
};

struct Bank 
{
	Bank() 
	{
		changed = false;
	}
	std::string bank_id;
	std::string bank_name;
	bool changed;
};

struct TransferLog 
{
	TransferLog() 
	{
		changed = true;
	}

	long long datetime;
	std::string currency;
	double amount;
	long error_id;
	std::string error_msg;
	bool changed;
};

struct User 
{
	std::string user_id;
	std::string trading_day;
	bool m_trade_more_data;
	std::map<std::string, Account> m_accounts;
	std::map<std::string, Position> m_positions;
	std::map<std::string, Order> m_orders;
	std::map<std::string, Trade> m_trades;
	std::map<std::string, Bank> m_banks;
	std::map<std::string, TransferLog> m_transfers;	
};

struct ActionInsertOrder 
{
	std::string order_id;
	std::string user_id;
	std::string exchange_id;
	std::string ins_id;
	long direction;
	long offset;
	int volume;
	long price_type;
	double limit_price;
	long volume_condition;
	long time_condition;
	long hedge_flag;
};

struct ActionCancelOrder 
{
	std::string order_id;
	std::string user_id;
};

struct Notify
{
	long type;
	long code;
	std::string content;
};

typedef std::array<char, 64> InsMapKeyType;

typedef Instrument InsMapMappedType;

typedef std::pair<const InsMapKeyType, InsMapMappedType> InsMapValueType;

typedef boost::interprocess::allocator<InsMapValueType, boost::interprocess::managed_shared_memory::segment_manager> ShmemAllocator;

struct CharArrayComparer
{
	bool operator()(const std::array<char, 64>& a, const std::array<char, 64>& b) const
	{
		return strncmp(a.data(), b.data(), 64) < 0;
	}
};

typedef boost::interprocess::map<InsMapKeyType, InsMapMappedType, CharArrayComparer, ShmemAllocator> InsMapType;
