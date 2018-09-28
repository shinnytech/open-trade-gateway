/////////////////////////////////////////////////////////////////////////
///@file trader_sim.cpp
///@brief	模拟交易实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trader_sim.h"

#include <regex>
#include <experimental/filesystem>
#include "../log.h"
#include "../rapid_serialize.h"
#include "../numset.h"
#include "../md_service.h"
#include "../utility.h"
#include "../config.h"

using namespace std::chrono;

namespace trader_dll
{

void SerializerSim::DefineStruct(ActionOrder& d)
{
    AddItem(d.aid, "aid");
    AddItem(d.order_id, "order_id");
    AddItem(d.user_id, "user_id");
    AddItem(d.exchange_id, "exchange_id");
    AddItem(d.ins_id, "instrument_id");
    AddItemEnum(d.direction, "direction", {
        { kDirectionBuy, "BUY" },
        { kDirectionSell, "SELL" },
        });
    AddItemEnum(d.offset, "offset", {
        { kOffsetOpen, "OPEN" },
        { kOffsetClose, "CLOSE" },
        { kOffsetCloseToday, "CLOSETODAY" },
        });
    AddItemEnum(d.price_type, "price_type", {
        { kPriceTypeLimit, "LIMIT" },
        { kPriceTypeAny, "ANY" },
        { kPriceTypeBest, "BEST" },
        { kPriceTypeFiveLevel, "FIVELEVEL" },
        });
    AddItemEnum(d.volume_condition, "volume_condition", {
        { kOrderVolumeConditionAny, "ANY" },
        { kOrderVolumeConditionMin, "MIN" },
        { kOrderVolumeConditionAll, "ALL" },
        });
    AddItemEnum(d.time_condition, "time_condition", {
        { kOrderTimeConditionIOC, "IOC" },
        { kOrderTimeConditionGFS, "GFS" },
        { kOrderTimeConditionGFD, "GFD" },
        { kOrderTimeConditionGTD, "GTD" },
        { kOrderTimeConditionGTC, "GTC" },
        { kOrderTimeConditionGFA, "GFA" },
        });
    AddItem(d.volume, "volume");
    AddItem(d.limit_price, "limit_price");
}

TraderSim::TraderSim(std::function<void(const std::string&)> callback)
    : TraderBase(callback)
{
    m_next_send_dt = 0;
    m_last_seq_no = 0;
    m_peeking_message = false;
    m_account = NULL;
}

void TraderSim::OnInit()
{
    LoadUserDataFile();
    m_account = &(m_data.m_accounts["CNY"]);
    if (m_account->static_balance < 1.0){
        m_account->user_id = m_user_id;
        m_account->currency = "CNY";
        m_account->pre_balance = 0;
        m_account->deposit = 1000000;
        m_account->withdraw = 0;
        m_account->close_profit = 0;
        m_account->commission = 0;
        m_account->static_balance = 1000000;
        m_account->position_profit = 0;
        m_account->float_profit = 0;
        m_account->balance = m_account->static_balance;
        m_account->frozen_margin = 0;
        m_account->frozen_commission = 0;
        m_account->margin = 0;
        m_account->available = m_account->static_balance;
        m_account->risk_ratio = 0;
        m_account->changed = true;
    }
    m_something_changed = true;
    char json_str[1024];
    sprintf(json_str, (u8"{"\
                        "\"aid\": \"rtn_data\","\
                        "\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
                        "\"user_id\" : \"%s\","\
                        "\"trading_day\" : \"%s\""
                        "}}}}]}")
            , m_user_id.c_str()
            , m_user_id.c_str()
            , g_config.trading_day.c_str()
            );
    Output(json_str);
    Log(LOG_INFO, NULL, "sim OnInit, UserID=%s", m_user_id.c_str());
}

void TraderSim::ProcessInput(const char* json_str)
{
    SerializerSim ss;
    if (!ss.FromString(json_str))
        return;
    rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ss.m_doc));
    if (!dt || !dt->IsString())
        return;
    std::string aid = dt->GetString();
    if (aid == "insert_order") {
        ActionOrder action_insert_order;
        ss.ToVar(action_insert_order);
        OnClientReqInsertOrder(action_insert_order);
    } else if (aid == "cancel_order") {
        ActionOrder action_cancel_order;
        ss.ToVar(action_cancel_order);
        OnClientReqCancelOrder(action_cancel_order);
    // } else if (aid == "req_transfer") {
    //     OnClientReqTransfer();
    } else if (aid == "peek_message") {
        OnClientPeekMessage();
    }
}

void TraderSim::OnFinish()
{
    Log(LOG_INFO, NULL, "TraderSim::OnFinish %p", this);
    SaveUserDataFile();
}

void TraderSim::OnIdle()
{
    //有空的时候, 标记为需查询的项, 如果离上次查询时间够远, 应该发起查询
    long long now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (m_peeking_message && (m_next_send_dt < now)){
        m_next_send_dt = now + 100;
        SendUserData();
    }
}

void TraderSim::OnClientReqInsertOrder(ActionOrder action_insert_order)
{
    std::string symbol = action_insert_order.exchange_id + "." + action_insert_order.ins_id;
    std::string order_key = action_insert_order.order_id;
    if(order_key.empty())
        order_key = std::to_string(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    auto it = m_data.m_orders.find(order_key);
    if (it != m_data.m_orders.end()) {
        OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:单号重复");
        return;
    }
    const md_service::Instrument* ins = md_service::GetInstrument(symbol);
    Order* order = &(m_data.m_orders[order_key]);
    order->user_id = action_insert_order.user_id;
    order->exchange_id = action_insert_order.exchange_id;
    order->instrument_id = action_insert_order.ins_id;
    order->order_id = action_insert_order.order_id;
    order->exchange_order_id = order->order_id;
    order->direction = action_insert_order.direction;
    order->offset = action_insert_order.offset;
    order->price_type = action_insert_order.price_type;
    order->limit_price = action_insert_order.limit_price;
    order->volume_orign = action_insert_order.volume;
    order->volume_left = action_insert_order.volume;
    order->status = kOrderStatusAlive;
    order->volume_condition = action_insert_order.volume_condition;
    order->time_condition = action_insert_order.time_condition;
    order->insert_date_time = GetLocalEpochNano();
    order->seqno = m_last_seq_no++;
    if (action_insert_order.user_id.substr(0, m_user_id.size()) != m_user_id){
        OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:下单指令中的用户名错误");
        order->status = kOrderStatusFinished;
        return;
    }
    if (!ins) {
        OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:合约不合法");
        order->status = kOrderStatusFinished;
        return;
    }
    if (ins->product_class != md_service::kProductClassFutures) {
        OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:模拟交易只支持期货合约");
        order->status = kOrderStatusFinished;
        return;
    }
    if (action_insert_order.volume <= 0) {
        OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:下单手数应该大于0");
        order->status = kOrderStatusFinished;
        return;
    }
    double xs = action_insert_order.limit_price / ins->price_tick;
    if (xs - int(xs + 0.5) >= 0.001) {
        OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:下单价格不是价格单位的整倍数");
        order->status = kOrderStatusFinished;
        return;
    }
    Position* position = &(m_data.m_positions[symbol]);
    position->ins = ins;
    position->instrument_id = ins->ins_id;
    position->exchange_id = ins->exchange_id;
    position->user_id = m_user_id;
    if (action_insert_order.offset == kOffsetOpen) {
        if (position->ins->margin * action_insert_order.volume > m_account->available) {
            OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:开仓保证金不足");
            order->status = kOrderStatusFinished;
            return;
        }
    } else {
        if ((action_insert_order.direction == kDirectionBuy && position->volume_short < action_insert_order.volume + position->volume_short_frozen_today)
            || (action_insert_order.direction == kDirectionSell && position->volume_long < action_insert_order.volume + position->volume_long_frozen_today)) {
            OutputNotify(1, u8"下单, 已被服务器拒绝, 原因:平仓手数超过持仓量");
            order->status = kOrderStatusFinished;
            return;
        }
    }
    m_alive_order_set.insert(order);
    UpdateOrder(order);
    return;
}

void TraderSim::OnClientReqCancelOrder(ActionOrder action_cancel_order)
{
    if (action_cancel_order.user_id.substr(0, m_user_id.size()) != m_user_id) {
        OutputNotify(1, u8"撤单, 已被服务器拒绝, 原因:撤单指令中的用户名错误");
        return;
    }
    for (auto it_order = m_alive_order_set.begin(); it_order != m_alive_order_set.end(); ++it_order) {
        Order* order = *it_order;
        if (order->order_id == action_cancel_order.order_id
            && order->status == kOrderStatusAlive) {
            order->status = kOrderStatusFinished;
            UpdateOrder(order);
            return;
        }
    }
    OutputNotify(1, u8"要撤销的单不存在");
    return;
}

void TraderSim::OnClientPeekMessage()
{
    m_peeking_message = true;
    SendUserData();
}

void TraderSim::SendUserData()
{
    if (!m_peeking_message)
        return;
    //尝试匹配成交
    TryOrderMatch();
    //重算所有持仓项的持仓盈亏和浮动盈亏
    double total_position_profit = 0;
    double total_float_profit = 0;
    double total_margin = 0;
    double total_frozen_margin = 0.0;
    for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end(); ++it) {
        const std::string& symbol = it->first;
        Position& ps = it->second;
        if (!ps.ins)
            ps.ins = md_service::GetInstrument(symbol);
        if (!ps.ins){
            Log(LOG_ERROR, NULL, "miss symbol %s when processing position", symbol);
            continue;
        }
        double last_price = ps.ins->last_price;
        if (!IsValid(last_price))
            last_price = ps.ins->pre_settlement;
        if (last_price != ps.last_price || ps.changed) {
            ps.last_price = last_price;
            ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
            ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
            ps.position_profit = ps.position_profit_long + ps.position_profit_short;
            ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
            ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
            ps.float_profit = ps.float_profit_long + ps.float_profit_short;
            ps.changed = true;
            m_something_changed = true;
        }
        if (IsValid(ps.position_profit))
            total_position_profit += ps.position_profit;
        if (IsValid(ps.float_profit))
            total_float_profit += ps.float_profit;
        if (IsValid(ps.margin))
            total_margin += ps.margin;
        total_frozen_margin += ps.frozen_margin;
    }
    //重算资金账户
    if(m_something_changed){
        m_account->position_profit = total_position_profit;
        m_account->float_profit = total_float_profit;
        m_account->balance = m_account->static_balance + m_account->float_profit + m_account->close_profit - m_account->commission;
        m_account->margin = total_margin;
        m_account->frozen_margin = total_frozen_margin;
        m_account->available = m_account->balance - m_account->margin - m_account->frozen_margin;
        if (IsValid(m_account->available) && IsValid(m_account->balance) && !IsZero(m_account->balance))
            m_account->risk_ratio = 1.0 - m_account->available / m_account->balance;
        else
            m_account->risk_ratio = NAN;
        m_account->changed = true;
    }
    if (!m_something_changed)
        return;
    //构建数据包
    SerializerTradeBase nss;
    rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");
    rapidjson::Value node_data;
    nss.FromVar(m_data, &node_data);
    rapidjson::Value node_user_id;
    node_user_id.SetString(m_user_id, nss.m_doc->GetAllocator());
    rapidjson::Value node_user;
    node_user.SetObject();
    node_user.AddMember(node_user_id, node_data, nss.m_doc->GetAllocator());
    rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc, node_user);
    std::string json_str;
    nss.ToString(&json_str);
    //发送
    Output(json_str);
    m_something_changed = false;
    m_peeking_message = false;
}

void TraderSim::TryOrderMatch()
{
    for(auto it_order = m_alive_order_set.begin(); it_order != m_alive_order_set.end(); ){
        Order* order = *it_order;
        if (order->status == kOrderStatusFinished) {
            it_order = m_alive_order_set.erase(it_order);
            continue;
        } else {
            CheckOrderTrade(order);
            ++it_order;
        }
    }
}

void TraderSim::CheckOrderTrade(Order* order)
{
    auto ins = md_service::GetInstrument(order->symbol());
    if (order->price_type == kPriceTypeLimit){
        if (order->limit_price - 0.0001 > ins->upper_limit) {
            OutputNotify(1, u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格超出涨停板");
            order->status = kOrderStatusFinished;
            UpdateOrder(order);
            return;
        }
        if (order->limit_price + 0.0001 < ins->lower_limit) {
            OutputNotify(1, u8"下单,已被服务器拒绝,原因:已撤单报单被拒绝价格跌破跌停板");
            order->status = kOrderStatusFinished;
            UpdateOrder(order);
            return;
        }
    }
    if (order->direction == kDirectionBuy && IsValid(ins->ask_price1)
        && (order->price_type == kPriceTypeAny || order->limit_price >= ins->ask_price1))
        DoTrade(order, order->volume_left, ins->ask_price1);
    if (order->direction == kDirectionSell && IsValid(ins->bid_price1)
        && (order->price_type == kPriceTypeAny || order->limit_price <= ins->bid_price1))
        DoTrade(order, order->volume_left, ins->bid_price1);
}

void TraderSim::DoTrade(Order* order, int volume, double price)
{
    //创建成交记录
    std::string trade_id = std::to_string(m_last_seq_no++);
    Trade* trade = &(m_data.m_trades[trade_id]);
    trade->trade_id = trade_id;
    trade->seqno = m_last_seq_no++;
    trade->user_id = order->user_id;
    trade->order_id = order->order_id;
    trade->exchange_trade_id = trade_id;
    trade->exchange_id = order->exchange_id;
    trade->instrument_id = order->instrument_id;
    trade->direction = order->direction;
    trade->offset = order->offset;
    trade->volume = volume;
    trade->price = price;
    trade->trade_date_time = GetLocalEpochNano();
    //调整委托单数据
    assert(volume <= order->volume_left);
    assert(order->status == kOrderStatusAlive);
    order->volume_left -= volume;
    if (order->volume_left == 0){
        order->status = kOrderStatusFinished;
    }
    order->seqno = m_last_seq_no++;
    order->changed = true;
    //调整持仓数据
    Position* position = &(m_data.m_positions[order->symbol()]);
    double commission = position->ins->commission * volume;
    trade->commission = commission;
    if (order->offset == kOffsetOpen) {
        if (order->direction == kDirectionBuy) {
            position->volume_long_today += volume;
            position->open_cost_long += price * volume * position->ins->volume_multiple;
            position->position_cost_long += price * volume * position->ins->volume_multiple;
            position->open_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);
            position->position_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);
        } else {
            position->volume_short_today += volume;
            position->open_cost_short += price * volume * position->ins->volume_multiple;
            position->position_cost_short += price * volume * position->ins->volume_multiple;
            position->open_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
            position->position_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
        }
    } else {
        double close_profit = 0;
        if (order->direction == kDirectionBuy) {
            position->open_cost_short = position->open_cost_short * (position->volume_short - volume) / position->volume_short;
            position->position_cost_short = position->position_cost_short * (position->volume_short - volume) / position->volume_short;
            close_profit = (position->position_price_short - price) * volume * position->ins->volume_multiple;
            position->volume_short_today -= volume;
        } else {
            position->open_cost_long = position->open_cost_long * (position->volume_long - volume) / position->volume_long;
            position->position_cost_long = position->position_cost_long * (position->volume_long - volume) / position->volume_long;
            close_profit = (price - position->position_price_long) * volume * position->ins->volume_multiple;
            position->volume_long_today -= volume;
        }
        m_account->close_profit += close_profit;
    }
    //调整账户资金
    m_account->commission += commission;
    UpdatePositionVolume(position);
}

void TraderSim::UpdateOrder(Order* order)
{
    order->seqno = m_last_seq_no++;
    order->changed = true;
    Position& position = GetPosition(order->symbol());
    assert(position.ins);
    UpdatePositionVolume(&position);
}

void TraderSim::UpdatePositionVolume(Position* position)
{
    position->frozen_margin = 0;
    position->volume_long_frozen_today = 0;
    position->volume_short_frozen_today = 0;
    for (auto it_order = m_alive_order_set.begin(); it_order != m_alive_order_set.end(); ++it_order) {
        Order* order = *it_order;
        if (order->status != kOrderStatusAlive)
            continue;
        if(position->instrument_id != order->instrument_id)
            continue;
        if (order->offset == kOffsetOpen) {
            position->frozen_margin += position->ins->margin * order->volume_left;
        } else {
            if (order->direction == kDirectionBuy)
                position->volume_short_frozen_today += order->volume_left;
            else
                position->volume_long_frozen_today += order->volume_left;
        }
    }
    position->volume_long_frozen = position->volume_long_frozen_his + position->volume_long_frozen_today;
    position->volume_short_frozen = position->volume_short_frozen_his + position->volume_short_frozen_today;
    position->volume_long = position->volume_long_his + position->volume_long_today;
    position->volume_short = position->volume_short_his + position->volume_short_today;
    position->margin_long = position->ins->margin * position->volume_long;
    position->margin_short = position->ins->margin * position->volume_short;
    position->margin = position->margin_long + position->margin_short;
    if (position->volume_long > 0) {
        position->open_price_long = position->open_cost_long / (position->volume_long * position->ins->volume_multiple);
        position->position_price_long = position->position_cost_long / (position->volume_long * position->ins->volume_multiple);
    }
    if (position->volume_short > 0) {
        position->open_price_short = position->open_cost_short / (position->volume_short * position->ins->volume_multiple);
        position->position_price_short = position->position_cost_short / (position->volume_short * position->ins->volume_multiple);
    }
    position->changed = true;
}

void TraderSim::LoadUserDataFile()
{
    if (m_user_file_path.empty())
        return;
    //选出最新的一个存档文件
    std::regex my_filter(m_user_id + "\\..+");
    std::vector<std::string> saved_files;
    for (auto& p : std::experimental::filesystem::v1::directory_iterator(m_user_file_path)) {
        if (!std::experimental::filesystem::v1::is_regular_file(p.status()))
            continue;
        std::smatch what;
        std::string file_name = p.path().filename().c_str();
        if (!std::regex_match(file_name.cbegin(), file_name.cend(), what, my_filter))
            continue;
        saved_files.push_back(p.path().c_str());
    }
    if (saved_files.empty())
        return;
    std::sort(saved_files.begin(), saved_files.end());
    auto fn = saved_files.back();
    //加载存档文件
    SerializerTradeBase nss;
    nss.FromFile(fn.c_str());
    nss.ToVar(m_data);
    //重建内存中的索引表和指针
    for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end();) {
        Position& position = it->second;
        position.ins = md_service::GetInstrument(position.symbol());
        if (position.ins)
            ++it;
        else
            it = m_data.m_positions.erase(it);
    }
    /*如果不是当天的存档文件, 则需要调整
        委托单和成交记录全部清空
        用户权益转为昨权益, 平仓盈亏
        持仓手数全部移动到昨仓, 持仓均价调整到昨结算价
    */
    if (fn.substr(fn.size()-8, 8) != g_config.trading_day){
        m_data.m_orders.clear();
        m_data.m_trades.clear();
        for (auto it = m_data.m_accounts.begin(); it != m_data.m_accounts.end(); ++it) {
            Account& item = it->second;
            item.pre_balance += (item.close_profit - item.commission + item.deposit - item.withdraw);
            item.close_profit = 0;
            item.commission = 0;
            item.withdraw = 0;
            item.deposit = 0;
            item.changed = true;
        }
        for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end(); ++it) {
            Position& item = it->second;
            item.volume_long_his = item.volume_long_his + item.volume_long_today;
            item.volume_long_today = 0;
            item.volume_long_frozen_today = 0;
            item.volume_short_his = item.volume_short_his + item.volume_short_today;
            item.volume_short_today = 0;
            item.volume_short_frozen_today = 0;
            item.changed = true;
        }
    } else {
        for (auto it = m_data.m_orders.begin(); it != m_data.m_orders.end(); ++it){
            m_alive_order_set.insert(&(it->second));
        }
    }
}

void TraderSim::SaveUserDataFile()
{
    if (m_user_file_path.empty())
        return;
    std::string fn = m_user_file_path + "/" + m_user_id + "." + g_config.trading_day;
    SerializerTradeBase nss;
    nss.dump_all = true;
    nss.FromVar(m_data);
    nss.ToFile(fn.c_str());
}

}