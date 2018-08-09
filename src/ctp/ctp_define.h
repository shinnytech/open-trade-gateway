/////////////////////////////////////////////////////////////////////////
///@file ctp_define.h
///@brief	CTP接口相关数据结构及序列化函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "ctp/ThostFtdcTraderApi.h"
#include "../datetime.h"
#include "../rapid_serialize.h"

namespace md_service {
struct Instrument;
}

namespace trader_dll
{
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

class SerializerCtp
    : public RapidSerialize::Serializer<SerializerCtp>
{
public:
    using RapidSerialize::Serializer<SerializerCtp>::Serializer;

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
    void DefineStruct(trader_dll::CtpActionInsertOrder& d)
    {
        AddItem(d.local_key, ("order_id"));
        AddItem(d.f.ExchangeID, ("exchange_id"));
        AddItem(d.f.InstrumentID, ("instrument_id"));
        AddItemEnum(d.f.Direction, ("direction"), {
            { THOST_FTDC_D_Buy, ("BUY") },
            { THOST_FTDC_D_Sell, ("SELL") },
            });
        AddItemEnum(d.f.CombOffsetFlag[0], ("offset"), {
            { THOST_FTDC_OF_Open, ("OPEN") },
            { THOST_FTDC_OF_Close, ("CLOSE") },
            { THOST_FTDC_OF_CloseToday, ("CLOSETODAY") },
            { THOST_FTDC_OF_CloseYesterday, ("CLOSE") },
            { THOST_FTDC_OF_ForceOff, ("CLOSE") },
            { THOST_FTDC_OF_LocalForceClose, ("CLOSE") },
            });
        AddItem(d.f.LimitPrice, ("limit_price"));
        AddItem(d.f.VolumeTotalOriginal, ("volume"));
        AddItemEnum(d.f.OrderPriceType, ("price_type"), {
            { THOST_FTDC_OPT_LimitPrice, ("LIMIT") },
            { THOST_FTDC_OPT_AnyPrice, ("ANY") },
            { THOST_FTDC_OPT_BestPrice, ("BEST") },
            { THOST_FTDC_OPT_FiveLevelPrice, ("FIVELEVEL") },
            });
        AddItemEnum(d.f.VolumeCondition, ("volume_condition"), {
            { THOST_FTDC_VC_AV, ("ANY") },
            { THOST_FTDC_VC_MV, ("MIN") },
            { THOST_FTDC_VC_CV, ("ALL") },
            });
        AddItemEnum(d.f.TimeCondition, ("time_condition"), {
            { THOST_FTDC_TC_IOC, ("IOC") },
            { THOST_FTDC_TC_GFS, ("GFS") },
            { THOST_FTDC_TC_GFD, ("GFD") },
            { THOST_FTDC_TC_GTD, ("GTD") },
            { THOST_FTDC_TC_GTC, ("GTC") },
            { THOST_FTDC_TC_GFA, ("GFA") },
            });
        AddItemEnum(d.f.CombHedgeFlag[0], ("hedge_flag"), {
            { THOST_FTDC_HF_Speculation, ("SPECULATION") },
            { THOST_FTDC_HF_Arbitrage, ("ARBITRAGE") },
            { THOST_FTDC_HF_Hedge, ("HEDGE") },
            { THOST_FTDC_HF_MarketMaker, ("MARKETMAKER") },
            });
        d.f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
        d.f.ContingentCondition = THOST_FTDC_CC_Immediately;
    }

    void DefineStruct(trader_dll::CtpActionCancelOrder& d)
    {
        AddItem(d.local_key, "order_id");
    }
};
}
