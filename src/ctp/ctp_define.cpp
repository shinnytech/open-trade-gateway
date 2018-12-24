/////////////////////////////////////////////////////////////////////////
///@file ctp_define.cpp
///@brief	CTP接口相关数据结构及序列化函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ctp_define.h"

#include "../utility.h"

namespace trader_dll {

void SerializerCtp::DefineStruct(CThostFtdcReqTransferField& d)
{
    AddItem(d.BankID, ("bank_id"));
    //AddItem(d.BankBranchID, ("bank_brch_id"));
    AddItem(d.AccountID, ("future_account"));
    AddItem(d.Password, ("future_password"));
    //AddItem(d.BankAccount, ("bank_account"));
    AddItem(d.BankPassWord, ("bank_password"));
    AddItem(d.CurrencyID, ("currency"));
    AddItem(d.TradeAmount, ("amount"));
}

void SerializerCtp::DefineStruct(OrderKeyFile& d)
{
    AddItem(d.trading_day, "trading_day");
    AddItem(d.items, "items");
}

void SerializerCtp::DefineStruct(CThostFtdcTransferSerialField& d)
{
    AddItem(d.CurrencyID, ("currency"));
    AddItem(d.TradeAmount, ("amount"));
    if (d.TradeCode == "")
        d.TradeAmount = 0 - d.TradeAmount;
    DateTime dt;
    dt.time.microsecond = 0;
    sscanf(d.TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
    sscanf(d.TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
    long long trade_date_time = DateTimeToEpochNano(&dt);
    AddItem(trade_date_time, ("datetime"));
    AddItem(d.ErrorID, ("error_id"));
    AddItem(d.ErrorMsg, ("error_msg"));
}

void SerializerCtp::DefineStruct(OrderKeyPair& d)
{
    AddItem(d.local_key, "local");
    AddItem(d.remote_key, "remote");
}

void SerializerCtp::DefineStruct(LocalOrderKey& d)
{
    AddItem(d.user_id, "user_id");
    AddItem(d.order_id, "order_id");
}

void SerializerCtp::DefineStruct(RemoteOrderKey& d)
{
    AddItem(d.exchange_id, "exchange_id");
    AddItem(d.instrument_id, "instrument_id");
    AddItem(d.session_id, "session_id");
    AddItem(d.front_id, "front_id");
    AddItem(d.order_ref, "order_ref");
}

void SerializerCtp::DefineStruct(CtpActionInsertOrder& d)
{
    AddItem(d.local_key.user_id, ("user_id"));
    AddItem(d.local_key.order_id, ("order_id"));
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
    //AddItemEnum(d.f.CombHedgeFlag[0], ("hedge_flag"), {
    //    { THOST_FTDC_HF_Speculation, ("SPECULATION") },
    //    { THOST_FTDC_HF_Arbitrage, ("ARBITRAGE") },
    //    { THOST_FTDC_HF_Hedge, ("HEDGE") },
    //    { THOST_FTDC_HF_MarketMaker, ("MARKETMAKER") },
    //    });
    d.f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
    d.f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
    d.f.ContingentCondition = THOST_FTDC_CC_Immediately;
}

void SerializerCtp::DefineStruct(CtpActionCancelOrder& d)
{
    AddItem(d.local_key.user_id, ("user_id"));
    AddItem(d.local_key.order_id, ("order_id"));
}

void SerializerCtp::DefineStruct(CThostFtdcUserPasswordUpdateField& d)
{
    AddItem(d.OldPassword, ("old_password"));
    AddItem(d.NewPassword, ("new_password"));
}

}