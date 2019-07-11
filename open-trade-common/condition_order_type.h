#pragma once

#include <string>
#include <vector>
#include <map>

//条件触发类型
enum class EContingentType
{
	market_open,//开盘触发
	time,//时间触发
	price,//价格触发
	price_range,//价格区间触发
	break_even//固定价格保本触发
};

//价格关系类型
enum class EPriceRelationType
{
	G,//大于
	GE,//大于等于
	L,//小于
	LE//小于等于
};

//下单买卖方向
enum class EOrderDirection
{
	buy,//买
	sell//卖
};

//下单开平方向
enum class EOrderOffset
{
	open,//开
	close,//平
	close_today,//平今
	reverse//反手
};

//手数类型
enum class EVolumeType
{
	num,//具体手数
	close_all//全平
};

//价格类型
enum class EPriceType
{
	contingent,//触发价格
	consideration,//对价
	market,//市价
	over,//超价
	limit,//限价
};

//逻辑操作符
enum class ELogicOperator
{
	logic_and,//逻辑与
	logic_or//逻辑或
};

//条件单的有效时长
enum class ETimeConditionType
{
	GFD,//当日有效
	GTD,//指定日期前有效
	GTC//撤消前一直有效
};

//条件单的状态
enum class EConditionOrderStatus
{
	live,//可用
	suspend,//暂停
	cancel,//撤消
	discard,//作废
	touched//触发
};

//市场状态
enum class EMarketStatus
{
	before_trading,//开盘前
	no_trading,//非交易
	continous, //连续交易
	auction_ordering,//集合竞价报单
	auction_balance,//集合竞价价格平衡
	auction_match,//集合竞价撮合
	is_closed//收盘
};

//触发条件
struct ContingentCondition
{
	ContingentCondition() :
		contingent_type(EContingentType::market_open)
		, exchange_id("")
		, instrument_id("")
		, is_touched(false)
		, market_status(EMarketStatus::before_trading)
		, contingent_time(0)
		, contingent_price(0)
		, price_relation_type(EPriceRelationType::G)
		, contingent_price_left(0)
		, contingent_price_right(0)
		, break_even_price(0)
	{
	}

	//条件触发类型
	EContingentType contingent_type;

	//交易所ID
	std::string exchange_id;

	//合约代码
	std::string instrument_id;

	//条件是否已经触发
	bool is_touched;

	//市场状态
	EMarketStatus market_status;

	//触发价格
	double contingent_price;

	//触发价格关系
	EPriceRelationType price_relation_type;

	//触发时间
	int contingent_time;

	//触发价格区间左边界
	double contingent_price_left;

	//触发价格区间右边界
	double contingent_price_right;

	//止盈价格
	double break_even_price;

	//更新市场状态
	void UpdateMarketStatus(EMarketStatus marketStatus);

	//更新市场时间
	void UpdateMarketTime(int marketTime);

	//更新市场价格
	void UpdatePrice(double price);
};

//条件触发后的订单
struct ContingentOrder
{
	ContingentOrder()
		:exchange_id("")
		, instrument_id("")
		, direction(EOrderDirection::buy)
		, offset(EOrderOffset::open)
		, volume_type(EVolumeType::num)
		, volume(0)
		, price_type(EPriceType::limit)
		, limit_price(0)
	{
	}

	std::string exchange_id;//交易所ID

	std::string instrument_id;//合约代码

	EOrderDirection direction;//方向

	EOrderOffset offset;//开平

	EVolumeType volume_type;//手数类型

	int volume;//具体手数

	EPriceType price_type;//价格类型

	double limit_price;//具体限价
};

//条件单
struct ConditionOrder
{
	ConditionOrder()
		:order_id("")
		, trading_day(0)
		, insert_date_time(0)
		, condition_list()
		, conditions_logic_oper(ELogicOperator::logic_and)
		, order_list()
		, time_condition_type(ETimeConditionType::GFD)
		, GTD_date(0)
		, is_cancel_ori_close_order(false)
		, status(EConditionOrderStatus::live)
		, changed(false)
	{
	}

	//条件单的订单号
	std::string order_id;

	//条件单生成时的交易日
	int trading_day;

	//条件单生成的时间
	long long insert_date_time;

	//条件单的条件列表
	std::vector<ContingentCondition> condition_list;

	//多个条件之间的逻辑操作符
	ELogicOperator conditions_logic_oper;

	//条件触发后的订单列表
	std::vector<ContingentOrder> order_list;

	//条件单的有效时长
	ETimeConditionType time_condition_type;

	//有效时期
	int GTD_date;

	//可平不足时是否撤原平仓挂单
	bool is_cancel_ori_close_order;

	//条件单状态
	EConditionOrderStatus status;

	//内部使用
	bool changed;
};

//条件单数据
struct ConditionOrderData
{
	ConditionOrderData()
		:broker_id("")
		, user_id("")
		, user_password("")
		, trading_day("")
		, condition_orders()
	{
	}

	//bid
	std::string broker_id;

	//用户名
	std::string user_id;

	//用户密码
	std::string user_password;

	//交易日
	std::string trading_day;

	//条件单列表
	std::map<std::string,ConditionOrder> condition_orders;
};

//条件单历史数据
struct ConditionOrderHisData
{
	ConditionOrderHisData()
		:broker_id("")
		, user_id("")
		, user_password("")
		, trading_day("")		
		, his_condition_orders()
	{
	}

	//bid
	std::string broker_id;

	//用户名
	std::string user_id;

	//用户密码
	std::string user_password;

	//交易日
	std::string trading_day;

	std::vector<ConditionOrder> his_condition_orders;
};

struct req_insert_condition_order
{
	req_insert_condition_order();

	std::string aid;

	//用户名
	std::string user_id;

	//条件单的订单号
	std::string order_id;

	//条件单的条件列表
	std::vector<ContingentCondition> condition_list;

	//多个条件之间的逻辑操作符
	ELogicOperator conditions_logic_oper;

	//条件触发后的订单列表
	std::vector<ContingentOrder> order_list;

	//条件单的有效时长
	ETimeConditionType time_condition_type;

	//有效时期
	int GTD_date;

	//可平不足时是否撤原平仓挂单
	bool is_cancel_ori_close_order;
};

struct req_cancel_condition_order
{
	req_cancel_condition_order();

	std::string aid;

	//用户名
	std::string user_id;

	//条件单的订单号
	std::string order_id;
};

struct req_pause_condition_order
{
	req_pause_condition_order();

	std::string aid;

	//用户名
	std::string user_id;

	//条件单的订单号
	std::string order_id;
};

struct req_resume_condition_order
{
	req_resume_condition_order();

	std::string aid;

	//用户名
	std::string user_id;

	//条件单的订单号
	std::string order_id;
};

struct qry_histroy_condition_order
{
	qry_histroy_condition_order();

	std::string aid;

	//用户名
	std::string user_id;

	//插入条件单的日期,自然日
	int action_day;
};

struct time_span
{
	time_span();

	int begin;

	int end;
};

struct weekday_time_span
{
	weekday_time_span();

	int weekday;

	std::vector<time_span> time_span_list;
};

struct condition_order_config
{
	condition_order_config();

	bool run_server;

	std::vector<weekday_time_span> auto_start_ctp_time;

	bool has_auto_start_ctp;

	std::vector<weekday_time_span> auto_close_ctp_time;

	bool has_auto_close_ctp;

	std::vector<weekday_time_span> auto_restart_process_time;

};

struct req_ccos_status
{
	req_ccos_status();

	std::string aid;

	bool run_server;
};

struct req_start_trade_instance
{
	req_start_trade_instance();

	std::string aid;

	std::string bid;

	std::string user_name; 

	std::string password;
};

typedef std::map<std::string, req_start_trade_instance> req_start_trade_key_map;

struct req_reconnect_trade_instance
{
	req_reconnect_trade_instance();

	std::string aid;

	std::vector<int> connIds;
};