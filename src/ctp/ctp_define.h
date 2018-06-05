/////////////////////////////////////////////////////////////////////////
///@file ctp_define.h
///@brief	CTP接口相关数据结构及序列化函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "ctp/ThostFtdcTraderApi.h"
#include "../datetime.h"
#include "../rapid_serialize.h"

namespace trader_dll
{

struct CtpAccount {
    CtpAccount() {
        position_profit = 0;
        balance = 0;
        available = 0;
        need_query = false;
        changed = true;
    }
    double position_profit;
    double balance;
    double available;
    double risk_ratio;
    bool need_query;
    bool changed;
};

struct CtpPosition
{
    CtpPosition() {
        need_query = false;
        changed = true;
    }
    //std::string symbol;
    std::string e_ins_id;
    std::string e_exchange_id;
    std::string e_symbol;
    int volume_long_today;
    int volume_long_his;
    int volume_short_today;
    int volume_short_his;
    double open_cost_long; //多头开仓市值
    double open_cost_short; //空头开仓市值
    double position_cost_long; //多头开仓市值
    double position_cost_short; //空头开仓市值
    double float_profit_long;
    double float_profit_short;
    double position_profit_long;
    double position_profit_short;
    double float_profit() const
    {
        return float_profit_long + float_profit_short;
    }
    double position_profit() const
    {
        return position_profit_long + position_profit_short;
    }
    int volume_long() const
    {
        return volume_long_today + volume_long_his;
    }
    int volume_short() const
    {
        return volume_short_today + volume_short_his;
    }
    bool need_query;
    bool changed;
};

struct CtpOrder {
    std::string instrument_id;
    std::string exchange_id;
    int session_id;
    int front_id;
    bool is_rtn;
};

struct CtpTrade {

};

struct TradeData {
    std::map<std::string, CtpAccount> m_accounts;
    std::map<std::string, CtpAccount> m_changed_accounts;
    std::map<std::string, CtpPosition> m_positions;
    std::map<std::string, CtpPosition> m_changed_positions;
    std::map<std::string, CtpOrder> m_orders;
};

struct RtnData {
    std::map<std::string, TradeData> trade;
};

struct RtnDataPack {
    RtnDataPack()
    {
        data.resize(1);
    }
    std::vector<RtnData> data;
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

struct OrderKeyPair
{
    std::string local_key;
    RemoteOrderKey remote_key;
};

struct OrderKeyFile
{
    std::string trading_day;
    std::vector<OrderKeyPair> items;
};

struct ReqLoginCtp {
    //req_login
    //服务器信息
    std::string broker_id;
    std::vector<std::string> trade_servers;
    //用户信息
    std::string user_id;
    std::string password;
    std::string product_info;
};

struct CtpActionInsertOrder {
    CtpActionInsertOrder() {
        memset(&f, 0, sizeof(f));
    }
    std::string local_key;
    CThostFtdcInputOrderField f;
};

struct CtpActionCancelOrder
{
    CtpActionCancelOrder() {
        memset(&f, 0, sizeof(f));
    }
    std::string local_key;
    CThostFtdcInputOrderActionField f;
};


struct CtpRtnDataOrder {
    CtpRtnDataOrder()
    {
        f = NULL;
        is_rtn = false;
    }
    std::string local_key;
    CThostFtdcOrderField* f;
    bool is_rtn;
};

struct CtpRtnDataTrade {
    CtpRtnDataTrade()
    {
        f = NULL;
    }
    std::string local_key;
    CThostFtdcTradeField* f;
};

struct CtpRtnDataPosition {
    //核心数据, 任意后台接口都需要实现
    std::string exchange_id;
    std::string ins_id;

    //非核心数据,除显示给用户看以外没有其它用途,可以不填
    double open_price_long; //多头开仓均价
    double open_price_short; //空头开仓均价
    double open_cost_long; //多头开仓市值
    double open_cost_short; //空头开仓市值

    double position_cost_long; //多头开仓市值
    double position_cost_short; //空头开仓市值
    int order_volume_buy_open;
    int order_volume_buy_close;
    int order_volume_sell_open;
    int order_volume_sell_close;

    int volume_long_today;
    int volume_long_his;
    int volume_long_frozen;
    int volume_long_frozen_today;

    int volume_short_today;
    int volume_short_his;
    int volume_short_frozen;
    int volume_short_frozen_today;

    double float_profit_long;
    double float_profit_short;
    double position_profit_long;
    double position_profit_short;
    double margin_long;
    double margin_short;
};

struct CtpRtnDataAccount {
    CtpRtnDataAccount()
    {
        f = NULL;
    }
    CThostFtdcTradingAccountField* f;
};

struct CtpUnitPosition
{
    CtpUnitPosition() {
        volume_long = 0;
        volume_short = 0;
        cost_long = 0;
        cost_short = 0;
        order_volume_buy_open = 0;
        order_volume_buy_close = 0;
        order_volume_sell_open = 0;
        order_volume_sell_close = 0;
    }
    std::string unit_id;
    std::string symbol;
    //持仓情况
    int volume_long;
    int volume_short;
    double cost_long;
    double cost_short;
    //挂单情况
    int order_volume_buy_open;
    int order_volume_sell_open;
    int order_volume_buy_close;
    int order_volume_sell_close;
};

struct CtpRtnTradeUnit
{
    //当前持仓
    CtpRtnTradeUnit() {
    }
    std::map<std::string, CtpUnitPosition> positions;
};

struct CtpRtnDataUser {
    std::string user_id;
    std::map<std::string, CtpRtnDataAccount> accounts;
    std::map<std::string, CtpRtnDataPosition> positions;
    std::map<std::string, CtpRtnDataOrder> orders;
    std::map<std::string, CtpRtnDataTrade> trades;
    std::map<std::string, CtpRtnTradeUnit> units;
};

struct CtpRtnDataItem {
    std::map<std::string, CtpRtnDataUser> trade;
};

struct CtpRtnDataPack {
    CtpRtnDataPack()
    {
        aid = "rtn_data";
        data.resize(1);
    }
    std::string aid;
    std::vector<CtpRtnDataItem> data;
    void clear() { data.clear(); data.resize(1); }
};

class SerializerCtp
    : public RapidSerialize::Serializer<SerializerCtp>
{
public:
    using RapidSerialize::Serializer<SerializerCtp>::Serializer;

    void DefineStruct(trader_dll::CtpOrder& d)
    {
        if (!is_save) {
            AddItem(d.instrument_id, "instrument_id");
            AddItem(d.exchange_id, "exchange_id");
            AddItem(d.session_id, "session_id");
            AddItem(d.front_id, "front_id");
            AddItem(d.is_rtn, "is_rtn");
        }
    }
    void DefineStruct(trader_dll::CtpPosition& d)
    {
        if (is_save) {
            AddItem(d.position_profit_long, "position_profit_long");
            AddItem(d.float_profit_long, "float_profit_long");
            AddItem(d.position_profit_short, "position_profit_short");
            AddItem(d.float_profit_short, "float_profit_short");
        } else {
            AddItem(d.e_ins_id, "instrument_id");
            AddItem(d.e_exchange_id, "exchange_id");
            d.e_symbol = d.e_exchange_id + "." + d.e_ins_id;
            AddItem(d.volume_long_today, "volume_long_today");
            AddItem(d.volume_long_his, "volume_long_his");
            AddItem(d.volume_short_today, "volume_short_today");
            AddItem(d.volume_short_his, "volume_short_his");
            AddItem(d.open_cost_long, "open_cost_long");
            AddItem(d.open_cost_short, "open_cost_short");
            AddItem(d.position_cost_long, "position_cost_long");
            AddItem(d.position_cost_short, "position_cost_short");
            d.changed = true;
        }
    }

    void DefineStruct(trader_dll::CtpAccount& d)
    {
        if (is_save) {
            AddItem(d.position_profit, "position_profit");
            AddItem(d.balance, "balance");
            AddItem(d.available, "available");
            AddItem(d.risk_ratio, "risk_ratio");
        } else {
            AddItem(d.position_profit, "position_profit");
            AddItem(d.balance, "balance");
            AddItem(d.available, "available");
            d.changed = true;
        }
    }

    void DefineStruct(trader_dll::TradeData& d)
    {
        if (is_save) {
            d.m_changed_accounts.clear();
            for (auto it = d.m_accounts.begin(); it != d.m_accounts.end(); ++it) {
                if (it->second.changed) {
                    d.m_changed_accounts[it->first] = it->second;
                    it->second.changed = false;
                }
            }
            d.m_changed_positions.clear();
            for (auto it = d.m_positions.begin(); it != d.m_positions.end(); ++it) {
                if (it->second.changed) {
                    d.m_changed_positions[it->first] = it->second;
                    it->second.changed = false;
                }
            }
            if (!d.m_changed_accounts.empty())
                AddItem(d.m_changed_accounts, "accounts");
            if (!d.m_changed_positions.empty())
                AddItem(d.m_changed_positions, "positions");
        } else {
            AddItem(d.m_accounts, "accounts");
            AddItem(d.m_positions, "positions");
            AddItem(d.m_orders, "orders");
        }
    }

    void DefineStruct(trader_dll::RtnData& d)
    {
        if (is_save) {
            AddItem(d.trade, "trade");
        } else {
            AddItem(d.trade, "trade");
            //AddItem(d.quotes, "quotes");
        }
    }

    void DefineStruct(trader_dll::RtnDataPack& d)
    {
        AddItem(d.data, "data");
    }

    void DefineStruct(trader_dll::OrderKeyFile& d)
    {
        AddItem(d.trading_day, "trading_day");
        AddItem(d.items, "items");
    }

    void DefineStruct(trader_dll::OrderKeyPair& d)
    {
        AddItem(d.local_key, "local");
        AddItem(d.remote_key, "remote");
    }

    void DefineStruct(trader_dll::RemoteOrderKey& d)
    {
        AddItem(d.exchange_id, "exchange_id");
        AddItem(d.instrument_id, "instrument_id");
        AddItem(d.session_id, "session_id");
        AddItem(d.front_id, "front_id");
        AddItem(d.order_ref, "order_ref");
    }
    void DefineStruct(trader_dll::ReqLoginCtp& d)
    {
        AddItem(d.broker_id, "broker_id");
        AddItem(d.trade_servers, "trade_servers");
        AddItem(d.user_id, "user_id");
        AddItem(d.password, "password");
        AddItem(d.product_info, "product_info");
    }

    void DefineStruct(trader_dll::CtpActionInsertOrder& d)
    {
        AddItem(d.local_key, "order_id");
        AddItem(d.f.ExchangeID, "exchange_id");
        AddItem(d.f.InstrumentID, "instrument_id");
        AddItemEnum(d.f.Direction, "direction", {
            { THOST_FTDC_D_Buy, "BUY" },
            { THOST_FTDC_D_Sell, "SELL" },
            });
        AddItemEnum(d.f.CombOffsetFlag[0], "offset", {
            { THOST_FTDC_OF_Open, "OPEN" },
            { THOST_FTDC_OF_Close, "CLOSE" },
            { THOST_FTDC_OF_CloseToday, "CLOSETODAY" },
            { THOST_FTDC_OF_CloseYesterday, "CLOSE" },
            { THOST_FTDC_OF_ForceOff, "CLOSE" },
            { THOST_FTDC_OF_LocalForceClose, "CLOSE" },
            });
        AddItem(d.f.LimitPrice, "limit_price");
        AddItem(d.f.VolumeTotalOriginal, "volume");
        d.f.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
        d.f.VolumeCondition = THOST_FTDC_VC_AV;
        d.f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
        d.f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
        d.f.TimeCondition = THOST_FTDC_TC_GFD;
        d.f.ContingentCondition = THOST_FTDC_CC_Immediately;
    }

    void DefineStruct(trader_dll::CtpActionCancelOrder& d)
    {
        AddItem(d.local_key, "order_id");
    }

    void DefineStruct(trader_dll::CtpRtnDataOrder& d)
    {
        std::string order_type = "TRADE";
        std::string trade_type = "TAKEPROFIT";
        AddItem(d.local_key, "order_id");
        AddItem(d.f->ExchangeID, "exchange_id");
        AddItem(d.f->InstrumentID, "instrument_id");
        AddItem(d.f->SessionID, "session_id");
        AddItem(d.f->FrontID, "front_id");
        AddItemEnum(d.f->Direction, "direction", {
            { THOST_FTDC_D_Buy, "BUY" },
            { THOST_FTDC_D_Sell, "SELL" },
            });
        AddItemEnum(d.f->CombOffsetFlag[0], "offset", {
            { THOST_FTDC_OF_Open, "OPEN" },
            { THOST_FTDC_OF_Close, "CLOSE" },
            { THOST_FTDC_OF_CloseToday, "CLOSETODAY" },
            { THOST_FTDC_OF_CloseYesterday, "CLOSE" },
            { THOST_FTDC_OF_ForceOff, "CLOSE" },
            { THOST_FTDC_OF_LocalForceClose, "CLOSE" },
            });
        AddItem(d.f->VolumeTotalOriginal, "volume_orign");
        AddItem(d.f->VolumeTotal, "volume_left");
        //价格及其它属性
        AddItemEnum(d.f->OrderPriceType, "price_type", {
            { THOST_FTDC_OPT_AnyPrice, "ANY" },
            { THOST_FTDC_OPT_LimitPrice, "LIMIT" },
            { THOST_FTDC_OPT_BestPrice, "BEST" },
            { THOST_FTDC_OPT_FiveLevelPrice, "FIVELEVEL" },

            });
        AddItem(d.f->LimitPrice, "limit_price");
        AddItemEnum(d.f->OrderStatus, "status", {
            { THOST_FTDC_OST_AllTraded, "FINISHED" },
            { THOST_FTDC_OST_PartTradedQueueing, "ALIVE" },
            { THOST_FTDC_OST_PartTradedNotQueueing, "FINISHED" },
            { THOST_FTDC_OST_NoTradeQueueing, "ALIVE" },
            { THOST_FTDC_OST_NoTradeNotQueueing, "FINISHED" },
            { THOST_FTDC_OST_Canceled, "FINISHED" },
            { THOST_FTDC_OST_Unknown, "ALIVE" },
            });
        AddItemEnum(d.f->TimeCondition, "time_condition", {
            { THOST_FTDC_TC_IOC, "IOC" },
            { THOST_FTDC_TC_GFS, "GFS" },
            { THOST_FTDC_TC_GFD, "GFD" },
            { THOST_FTDC_TC_GTD, "GTD" },
            { THOST_FTDC_TC_GTC, "GTC" },
            { THOST_FTDC_TC_GFA, "GFA" },
            });
        AddItemEnum(d.f->VolumeCondition, "volume_condition", {
            { THOST_FTDC_VC_AV, "ANY" },
            { THOST_FTDC_VC_MV, "MIN" },
            { THOST_FTDC_VC_CV, "ALL" },
            });
        AddItem(d.f->MinVolume, "min_volume");
        AddItemEnum(d.f->ForceCloseReason, "force_close", {
            //@todo:
            { THOST_FTDC_FCC_NotForceClose, "NOT" },
            });
        AddItemEnum(d.f->CombHedgeFlag[0], "hedge_flag", {
            { THOST_FTDC_HF_Speculation, "SPECULATION" },
            { THOST_FTDC_HF_Arbitrage, "ARBITRAGE" },
            { THOST_FTDC_HF_Hedge, "HEDGE" },
            { THOST_FTDC_HF_MarketMaker, "MARKETMAKER" },
            });
        AddItem(d.f->OrderSysID, "exchange_order_id");
        AddItem(order_type, "order_type");
        AddItem(trade_type, "trade_type");
        std::string msg = GBKToUTF8(d.f->StatusMsg);
        AddItem(msg, "last_msg");
        AddItem(d.is_rtn, "is_rtn");

        if (is_save) {
            DateTime dt;
            dt.time.microsecond = 0;
            sscanf(d.f->InsertDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
            sscanf(d.f->InsertTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
            long long epoch = DateTimeToEpochNano(&dt);
            AddItem(epoch, "insert_date_time");
        }
    }

    void DefineStruct(trader_dll::CtpRtnDataTrade& d)
    {
        AddItem(d.local_key, "order_id");
        AddItem(d.f->ExchangeID, "exchange_id");
        AddItem(d.f->InstrumentID, "instrument_id");
        AddItem(d.f->TradeID, "exchange_trade_id");
        AddItemEnum(d.f->Direction, "direction", {
            { THOST_FTDC_D_Buy, "BUY" },
            { THOST_FTDC_D_Sell, "SELL" },
            });
        AddItemEnum(d.f->OffsetFlag, "offset", {
            { THOST_FTDC_OF_Open, "OPEN" },
            { THOST_FTDC_OF_Close, "CLOSE" },
            { THOST_FTDC_OF_CloseToday, "CLOSETODAY" },
            { THOST_FTDC_OF_CloseYesterday, "CLOSE" },
            { THOST_FTDC_OF_ForceOff, "CLOSE" },
            { THOST_FTDC_OF_LocalForceClose, "CLOSE" },
            });
        AddItem(d.f->Volume, "volume");
        AddItem(d.f->Price, "price");
        if (is_save) {
            DateTime dt;
            dt.time.microsecond = 0;
            sscanf(d.f->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
            sscanf(d.f->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
            long long epoch = DateTimeToEpochNano(&dt);
            AddItem(epoch, "trade_date_time");
        }
    }

    void DefineStruct(trader_dll::CtpRtnDataPosition& d)
    {
        AddItem(d.exchange_id, "exchange_id");
        AddItem(d.ins_id, "instrument_id");
        AddItem(d.open_price_long, "open_price_long");
        AddItem(d.open_price_short, "open_price_short");
        AddItem(d.open_cost_long, "open_cost_long");
        AddItem(d.open_cost_short, "open_cost_short");
        AddItem(d.float_profit_long, "float_profit_long");
        AddItem(d.float_profit_short, "float_profit_short");
        AddItem(d.position_cost_long, "position_cost_long");
        AddItem(d.position_cost_short, "position_cost_short");
        AddItem(d.position_profit_long, "position_profit_long");
        AddItem(d.position_profit_short, "position_profit_short");
        AddItem(d.order_volume_buy_open, "order_volume_buy_open");
        AddItem(d.order_volume_buy_close, "order_volume_buy_close");
        AddItem(d.order_volume_sell_open, "order_volume_sell_open");
        AddItem(d.order_volume_sell_close, "order_volume_sell_close");
        AddItem(d.volume_long_today, "volume_long_today");
        AddItem(d.volume_long_his, "volume_long_his");
        AddItem(d.volume_long_frozen, "volume_long_frozen");
        AddItem(d.volume_long_frozen_today, "volume_long_frozen_today");
        AddItem(d.volume_short_today, "volume_short_today");
        AddItem(d.volume_short_his, "volume_short_his");
        AddItem(d.volume_short_frozen, "volume_short_frozen");
        AddItem(d.volume_short_frozen_today, "volume_short_frozen_today");
        AddItem(d.margin_long, "margin_long");
        AddItem(d.margin_short, "margin_short");
    }

    void DefineStruct(trader_dll::CtpRtnDataAccount& d)
    {
        if (d.f) {
            AddItem(d.f->AccountID, "account_id");
            AddItem(d.f->CurrencyID, "currency");
            AddItem(d.f->Available, "available");
            AddItem(d.f->PreBalance, "pre_balance");
            AddItem(d.f->PositionProfit, "position_profit");
            double dynamic_rights = d.f->Balance;
            AddItem(dynamic_rights, "balance");
            double static_balance = dynamic_rights - d.f->PositionProfit;
            AddItem(static_balance, "static_balance");
            double risk = (d.f->Balance - d.f->Available) / d.f->Balance;
            AddItem(risk, "risk_ratio");
            AddItem(d.f->CashIn, "premium");
            AddItem(d.f->CurrMargin, "margin");
            AddItem(d.f->FrozenMargin, "frozen_margin");
            AddItem(d.f->FrozenCommission, "frozen_commission");
            AddItem(d.f->FrozenCash, "frozen_premium");
            AddItem(d.f->Deposit, "deposit");
            AddItem(d.f->Withdraw, "withdraw");
            AddItem(d.f->Commission, "commission");
            AddItem(d.f->CloseProfit, "close_profit");
            d.f = NULL;
        }
    }

    void DefineStruct(trader_dll::CtpRtnDataUser& d)
    {
        AddItem(d.user_id, "user_id");
        if (!d.accounts.empty())
            AddItem(d.accounts, "accounts");
        if (!d.positions.empty())
            AddItem(d.positions, "positions");
        if (!d.orders.empty())
            AddItem(d.orders, "orders");
        if (!d.trades.empty())
            AddItem(d.trades, "trades");
        if (!d.units.empty())
            AddItem(d.units, "units");
    }

    void DefineStruct(trader_dll::CtpRtnDataItem& d)
    {
        AddItem(d.trade, "trade");
    }

    void DefineStruct(trader_dll::CtpRtnDataPack& d)
    {
        AddItem(d.aid, "aid");
        AddItem(d.data, "data");
    }

    void DefineStruct(trader_dll::CtpRtnTradeUnit& d)
    {
        AddItem(d.positions, "positions");
    }

    void DefineStruct(trader_dll::CtpUnitPosition& d)
    {
        AddItem(d.unit_id, "unit_id");
        AddItem(d.symbol, "symbol");

        AddItem(d.volume_long, "volume_long");
        AddItem(d.volume_short, "volume_short");
        AddItem(d.cost_long, "cost_long");
        AddItem(d.cost_short, "cost_short");

        AddItem(d.order_volume_buy_open, "order_volume_buy_open");
        AddItem(d.order_volume_sell_open, "order_volume_sell_open");
        AddItem(d.order_volume_buy_close, "order_volume_buy_close");
        AddItem(d.order_volume_sell_close, "order_volume_sell_close");
    }
};
}
