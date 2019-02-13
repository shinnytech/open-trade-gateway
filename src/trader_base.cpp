/////////////////////////////////////////////////////////////////////////
///@file trade_base.cpp
///@brief	交易后台接口基类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trader_base.h"
#include "md_service.h"

namespace trader_dll
{

TraderBase::TraderBase(std::function<void(const std::string&)> callback)
    : m_send_callback(callback)
{
    m_running = true;
    m_finished = false;
    m_notify_seq = 0;
    m_data_seq = 0;
}

TraderBase::~TraderBase()
{
}

void TraderBase::Output(const std::string& json)
{
    if (m_running) {
        m_send_callback(json);
    }
}


using namespace std::chrono_literals;
void TraderBase::Run()
{
    OnInit();
    while (m_running) {
        std::string input_str;
        auto dead_line = std::chrono::system_clock::now() + 100ms;
        while(m_in_queue.pop_front(&input_str, dead_line)){
            ProcessInput(input_str.c_str());
        }
        OnIdle();
    }
    OnFinish();
    m_finished = true;
}

Account& TraderBase::GetAccount(const std::string account_key)
{
    return m_data.m_accounts[account_key];
}

Position& TraderBase::GetPosition(const std::string symbol)
{
    Position& position = m_data.m_positions[symbol];
    return position;
}

Order& TraderBase::GetOrder(const std::string order_id)
{
    return m_data.m_orders[order_id];
}

trader_dll::Trade& TraderBase::GetTrade(const std::string trade_key)
{
    return m_data.m_trades[trade_key];
}

trader_dll::Bank& TraderBase::GetBank(const std::string& bank_id)
{
    return m_data.m_banks[bank_id];
}

trader_dll::TransferLog& TraderBase::GetTransferLog(const std::string& seq_id)
{
    return m_data.m_transfers[seq_id];
}

void TraderBase::Start(const ReqLogin& req_login)
{
    m_running = true;
    if (!g_config.user_file_path.empty())
        m_user_file_path = g_config.user_file_path + "/" + req_login.bid;
    m_req_login = req_login;
    m_user_id = m_req_login.user_name;
    m_data.user_id = m_user_id;
    m_worker_thread = std::thread(std::bind(&TraderBase::Run, this));
}

void TraderBase::Stop()
{
    m_running = false;
}

void TraderBase::OutputNotify(long notify_code, const std::string& notify_msg, const char* level, const char* type)
{
    //构建数据包
    SerializerTradeBase nss;
    rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");
    // rapidjson::Value node_user_id;
    // node_user_id.SetString(m_user_id, nss.m_doc->GetAllocator());
    rapidjson::Value node_message;
    node_message.SetObject();
    node_message.AddMember("type", rapidjson::Value(type, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
    node_message.AddMember("level", rapidjson::Value(level, nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
    node_message.AddMember("code", notify_code, nss.m_doc->GetAllocator());
    node_message.AddMember("content", rapidjson::Value(notify_msg.c_str(), nss.m_doc->GetAllocator()).Move(), nss.m_doc->GetAllocator());
    rapidjson::Pointer("/data/0/notify/N" + std::to_string(m_notify_seq++)).Set(*nss.m_doc, node_message);
    std::string json_str;
    nss.ToString(&json_str);
    //发送
    Output(json_str);
}

void SerializerTradeBase::DefineStruct(ReqLogin& d)
{
    AddItem(d.aid, "aid");
    AddItem(d.bid, "bid");
    AddItem(d.user_name, "user_name");
    AddItem(d.password, "password");
}

void SerializerTradeBase::DefineStruct(Bank& d)
{
    AddItem(d.bank_id, ("id"));
    AddItem(d.bank_name, ("name"));
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

Order::Order()
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

std::string Order::symbol() const
{
    return exchange_id + "." + instrument_id;
}

std::string Trade::symbol() const
{
    return exchange_id + "." + instrument_id;
}

std::string Position::symbol() const
{
    return exchange_id + "." + instrument_id;
}

Trade::Trade()
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

Position::Position()
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
}

Account::Account()
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

}