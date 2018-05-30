#include "stdafx.h"
#include "traderspi.h"

#include "trader_femas_open.h"
#include "tiny_serialize.h"



namespace trader_dll
{


struct FemasRtnDataOrder {
    FemasRtnDataOrder()
    {
        f = NULL;
    }
    CUstpFtdcOrderField* f;
};

struct FemasRtnDataTrade {
    FemasRtnDataTrade()
    {
        f = NULL;
    }
    CUstpFtdcTradeField* f;
};

struct FemasRtnDataTransfer {
    FemasRtnDataTransfer()
    {
        f = NULL;
    }
    CUstpFtdcTransferSerialFieldRspField* f;
};

struct FemasRtnDataPosition {
    FemasRtnDataPosition()
    {
        f = NULL;
        f2 = NULL;
    }
    CUstpFtdcClientPositionChangeField* f;
    CUstpFtdcRspInvestorPositionField* f2;
};

struct FemasRtnDataAccount {
    FemasRtnDataAccount()
    {
        f = NULL;
        f2 = NULL;
    }
    CUstpFtdcInvestorAccountChangeField* f;
    CUstpFtdcRspInvestorAccountField* f2;
};

struct FemasRtnBank {
    FemasRtnBank()
    {
        f = NULL;
    }
    CUstpFtdcAccountregisterRspField* f;
};

struct FemasRtnDataUser {
    std::string user_id;
    std::map<std::string, FemasRtnBank> banks;
    std::map<std::string, FemasRtnDataAccount> accounts;
    std::map<std::string, FemasRtnDataPosition> positions;
    std::map<std::string, FemasRtnDataOrder> orders;
    std::map<std::string, FemasRtnDataTrade> trades;
    std::map<std::string, FemasRtnDataTransfer> transfers;
};

struct FemasRtnDataItem {
    std::map<std::string, FemasRtnDataUser> trade;
};

struct FemasRtnDataPack {
    FemasRtnDataPack()
    {
        aid = "rtn_data";
        data.resize(1);
    }
    std::string aid;
    std::vector<FemasRtnDataItem> data;
};

static void DefineStruct(FemasRtnDataOrder& d, TinySerialize::SerializeBinder& tl)
{
    std::string order_type = "TRADE";
    std::string trade_type = "TAKEPROFIT";
    std::string exchange_id = d.f->ExchangeID;
    if (exchange_id == "ZCE")
        exchange_id = "CZCE";
    tl.AddItem(exchange_id, _T("exchange_id"));
    tl.AddItem(d.f->InstrumentID, _T("instrument_id"));
    std::string session_id = "0";
    tl.AddItem(session_id, _T("session_id"));
    tl.AddItem(d.f->UserOrderLocalID, _T("order_id"));
    tl.AddItemEnum(d.f->Direction, _T("direction"), {
        { USTP_FTDC_D_Buy, _T("BUY") },
        { USTP_FTDC_D_Sell, _T("SELL") },
    });
    tl.AddItemEnum(d.f->OffsetFlag, _T("offset"), {
        { USTP_FTDC_OF_Open, _T("OPEN") },
        { USTP_FTDC_OF_Close, _T("CLOSE") },
        { USTP_FTDC_OF_CloseToday, _T("CLOSETODAY") },
    });
    tl.AddItem(d.f->Volume, _T("volume_orign"));
    tl.AddItem(d.f->VolumeRemain, _T("volume_left"));
    //价格及其它属性
    tl.AddItemEnum(d.f->OrderPriceType, _T("price_type"), {
        { USTP_FTDC_OPT_AnyPrice, _T("ANY") },
        { USTP_FTDC_OPT_LimitPrice, _T("LIMIT") },
        { USTP_FTDC_OPT_BestPrice, _T("BEST") },
        { USTP_FTDC_OPT_FiveLevelPrice, _T("FIVELEVEL") },

    });
    if (d.f->OrderPriceType == USTP_FTDC_OPT_StopLosPrice
        || d.f->OrderPriceType == USTP_FTDC_OPT_LimitStopLosPrice) {
        trade_type = "STOPLOSS";
    }
    tl.AddItem(d.f->LimitPrice, _T("limit_price"));
    tl.AddItemEnum(d.f->OrderStatus, _T("status"), {
        { USTP_FTDC_OS_AllTraded, _T("FINISHED") },
        { USTP_FTDC_OS_PartTradedQueueing, _T("ALIVE") },
        { USTP_FTDC_OS_PartTradedNotQueueing, _T("FINISHED") },
        { USTP_FTDC_OS_NoTradeQueueing, _T("ALIVE") },
        { USTP_FTDC_OS_NoTradeNotQueueing, _T("FINISHED") },
        { USTP_FTDC_OS_Canceled, _T("FINISHED") },
        { USTP_FTDC_OS_AcceptedNoReply, _T("ALIVE") },
    });
    tl.AddItemEnum(d.f->TimeCondition, _T("time_condition"), {
        { USTP_FTDC_TC_IOC, _T("IOC") },
        { USTP_FTDC_TC_GFS, _T("GFS") },
        { USTP_FTDC_TC_GFD, _T("GFD") },
        { USTP_FTDC_TC_GTD, _T("GTD") },
        { USTP_FTDC_TC_GTC, _T("GTC") },
        { USTP_FTDC_TC_GFA, _T("GFA") },
    });
    tl.AddItemEnum(d.f->VolumeCondition, _T("volume_condition"), {
        { USTP_FTDC_VC_AV, _T("ANY") },
        { USTP_FTDC_VC_MV, _T("MIN") },
        { USTP_FTDC_VC_CV, _T("ALL") },
    });
    tl.AddItem(d.f->MinVolume, _T("min_volume"));
    tl.AddItem(d.f->InsertTime, _T("insert_date_time"));
    tl.AddItemEnum(d.f->ForceCloseReason, _T("force_close"), {
        //@todo:
        { USTP_FTDC_FCR_NotForceClose, _T("NOT") },
    });
    tl.AddItemEnum(d.f->HedgeFlag, _T("hedge_flag"), {
        { USTP_FTDC_CHF_Speculation, _T("SPECULATION") },
        { USTP_FTDC_CHF_Arbitrage, _T("ARBITRAGE") },
        { USTP_FTDC_CHF_Hedge, _T("HEDGE") },
        { USTP_FTDC_CHF_MarketMaker, _T("MARKETMAKER") },
    });
    tl.AddItem(d.f->OrderSysID, _T("exchange_order_id"));
    tl.AddItem(order_type, _T("order_type"));
    tl.AddItem(trade_type, _T("trade_type"));
}

static void DefineStruct(FemasRtnDataTrade& d, TinySerialize::SerializeBinder& tl)
{
    std::string exchange_id = d.f->ExchangeID;
    if (exchange_id == "ZCE")
        exchange_id = "CZCE";
    tl.AddItem(exchange_id, _T("exchange_id"));
    tl.AddItem(d.f->InstrumentID, _T("instrument_id"));
    std::string session_id = "0";
    tl.AddItem(session_id, _T("session_id"));
    tl.AddItem(d.f->UserOrderLocalID, _T("order_id"));
    tl.AddItem(d.f->TradeID, _T("exchange_trade_id"));
    tl.AddItemEnum(d.f->Direction, _T("direction"), {
        { USTP_FTDC_D_Buy, _T("BUY") },
        { USTP_FTDC_D_Sell, _T("SELL") },
    });
    tl.AddItemEnum(d.f->OffsetFlag, _T("offset"), {
        { USTP_FTDC_OF_Open, _T("OPEN") },
        { USTP_FTDC_OF_Close, _T("CLOSE") },
        { USTP_FTDC_OF_CloseToday, _T("CLOSETODAY") },
    });
    tl.AddItem(d.f->TradeVolume, _T("volume"));
    tl.AddItem(d.f->TradePrice, _T("price"));
    tl.AddItem(d.f->TradeTime, _T("trade_date_time"));
}

static void DefineStruct(FemasRtnDataTransfer& d, TinySerialize::SerializeBinder& tl)
{
    std::string dt = std::string(d.f->TradingDay) + "|" + std::string(d.f->TradeTime);
    tl.AddItem(dt, _T("datetime"));
    tl.AddItem(d.f->FutureSerial, _T("seq_no"));
    tl.AddItemEnum(d.f->TradeCode, _T("trade_type"), {
        { "15", _T("银行转期货") },
        { "16", _T("期货转银行") },
    });
    tl.AddItem(d.f->TradeAmount, _T("amount"));
    tl.AddItem(d.f->CurrencyID, _T("currency"));
    tl.AddItem(d.f->BankAccount, _T("bank_account"));
}

static void DefineStruct(trader_dll::FemasRtnDataPosition& d, TinySerialize::SerializeBinder& tl)
{
    if (d.f) {
        std::string exchange_id = d.f->ExchangeID;
        if (exchange_id == "ZCE")
            exchange_id = "CZCE";
        tl.AddItem(exchange_id, _T("exchange_id"));
        tl.AddItem(d.f->InstrumentID, _T("instrument_id"));
        tl.AddItemEnum(d.f->HedgeFlag, _T("hedge_flag"), {
            { USTP_FTDC_CHF_Speculation, _T("SPECULATION") },
            { USTP_FTDC_CHF_Arbitrage, _T("ARBITRAGE") },
            { USTP_FTDC_CHF_Hedge, _T("HEDGE") },
            { USTP_FTDC_CHF_MarketMaker, _T("MARKETMAKER") },
        });
        int volume_his = d.f->YdPosition;
        int volume_today = d.f->Position - d.f->YdPosition;
        int frozen = d.f->FrozenClosing;
        int frozen_today = d.f->FrozenClosing - d.f->YdFrozenClosing;
        if (d.f->Direction == USTP_FTDC_D_Buy) {
            tl.AddItem(volume_today, _T("volume_long_today"));
            tl.AddItem(volume_his, _T("volume_long_his"));
            tl.AddItem(frozen, _T("volume_long_frozen"));
            tl.AddItem(frozen_today, _T("volume_long_frozen_today"));
            tl.AddItem(d.f->PositionCost, _T("open_cost_long"));
            tl.AddItem(d.f->UsedMargin, _T("margin_long"));
        } else {
            tl.AddItem(volume_today, _T("volume_short_today"));
            tl.AddItem(volume_his, _T("volume_short_his"));
            tl.AddItem(frozen, _T("volume_short_frozen"));
            tl.AddItem(frozen_today, _T("volume_short_frozen_today"));
            tl.AddItem(d.f->PositionCost, _T("open_cost_short"));
            tl.AddItem(d.f->UsedMargin, _T("margin_short"));
        }
        d.f = NULL;
    }
    if (d.f2) {
        std::string exchange_id = d.f2->ExchangeID;
        if (exchange_id == "ZCE")
            exchange_id = "CZCE";
        tl.AddItem(exchange_id, _T("exchange_id"));
        tl.AddItem(d.f2->InstrumentID, _T("instrument_id"));
        tl.AddItemEnum(d.f2->HedgeFlag, _T("hedge_flag"), {
            { USTP_FTDC_CHF_Speculation, _T("SPECULATION") },
            { USTP_FTDC_CHF_Arbitrage, _T("ARBITRAGE") },
            { USTP_FTDC_CHF_Hedge, _T("HEDGE") },
            { USTP_FTDC_CHF_MarketMaker, _T("MARKETMAKER") },
        });
        int volume_his = d.f2->YdPosition;
        int volume_today = d.f2->Position - d.f2->YdPosition;
        int frozen = d.f2->FrozenClosing;
        int frozen_today = d.f2->FrozenClosing - d.f2->YdFrozenClosing;
        if (d.f2->Direction == USTP_FTDC_D_Buy) {
            tl.AddItem(volume_today, _T("volume_long_today"));
            tl.AddItem(volume_his, _T("volume_long_his"));
            tl.AddItem(frozen, _T("volume_long_frozen"));
            tl.AddItem(frozen_today, _T("volume_long_frozen_today"));
            tl.AddItem(d.f2->PositionCost, _T("open_cost_long"));
            tl.AddItem(d.f2->UsedMargin, _T("margin_long"));
        } else {
            tl.AddItem(volume_today, _T("volume_short_today"));
            tl.AddItem(volume_his, _T("volume_short_his"));
            tl.AddItem(frozen, _T("volume_short_frozen"));
            tl.AddItem(frozen_today, _T("volume_short_frozen_today"));
            tl.AddItem(d.f2->PositionCost, _T("open_cost_short"));
            tl.AddItem(d.f2->UsedMargin, _T("margin_short"));
        }
        tl.AddItem(d.f2->UsedMargin, _T("margin"));
        d.f2 = NULL;
    }
}

static void DefineStruct(trader_dll::FemasRtnDataAccount& d, TinySerialize::SerializeBinder& tl)
{
    if (d.f) {
        tl.AddItem(d.f->AccountID, _T("account_id"));
        tl.AddItem(d.f->Currency, _T("currency"));
        tl.AddItem(d.f->Available, _T("available"));
        tl.AddItem(d.f->PreBalance, _T("pre_balance"));
        tl.AddItem(d.f->PositionProfit, _T("position_profit"));
        double dynamic_rights = d.f->Available + d.f->FrozenMargin + d.f->FrozenFee + d.f->FrozenPremium;
        tl.AddItem(dynamic_rights, _T("balance"));
        double static_balance = dynamic_rights - d.f->PositionProfit;
        tl.AddItem(static_balance, _T("static_balance"));
        double risk = d.f->Risk / 1000;
        tl.AddItem(risk, _T("risk_ratio"));
        tl.AddItem(d.f->Margin, _T("margin"));
        tl.AddItem(d.f->FrozenMargin, _T("frozen_margin"));
        tl.AddItem(d.f->FrozenFee, _T("frozen_commission"));
        tl.AddItem(d.f->FrozenPremium, _T("frozen_premium"));
        tl.AddItem(d.f->Deposit, _T("deposit"));
        tl.AddItem(d.f->Withdraw, _T("withdraw"));
        tl.AddItem(d.f->Fee, _T("commission"));
        tl.AddItem(d.f->Premium, _T("preminum"));
        tl.AddItem(d.f->CloseProfit, _T("close_profit"));
        d.f = NULL;
    }
    if (d.f2) {
        tl.AddItem(d.f2->AccountID, _T("account_id"));
        //@todo: 缺currency字段
        //tl.AddItem(d.f2->Currency, _T("currency"));
        double dynamic_rights = d.f2->Available + d.f2->FrozenMargin + d.f2->FrozenFee + d.f2->FrozenPremium;
        tl.AddItem(dynamic_rights, _T("balance"));
        double static_balance = dynamic_rights - d.f2->PositionProfit;
        tl.AddItem(static_balance, _T("static_balance"));
        tl.AddItem(d.f2->Available, _T("available"));
        tl.AddItem(d.f2->PreBalance, _T("pre_balance"));
        tl.AddItem(d.f2->PositionProfit, _T("position_profit"));
        double risk = d.f2->Risk / 1000;
        tl.AddItem(risk, _T("risk_ratio"));
        tl.AddItem(d.f2->Margin, _T("margin"));
        tl.AddItem(d.f2->FrozenMargin, _T("frozen_margin"));
        tl.AddItem(d.f2->FrozenFee, _T("frozen_commission"));
        tl.AddItem(d.f2->FrozenPremium, _T("frozen_premium"));
        tl.AddItem(d.f2->Deposit, _T("deposit"));
        tl.AddItem(d.f2->Withdraw, _T("withdraw"));
        tl.AddItem(d.f2->Fee, _T("commission"));
        tl.AddItem(d.f2->Premium, _T("preminum"));
        tl.AddItem(d.f2->CloseProfit, _T("close_profit"));
        d.f2 = NULL;
    }
}

static void DefineStruct(trader_dll::FemasRtnDataUser& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.user_id, _T("user_id"));
    if (!d.banks.empty())
        tl.AddItem(d.banks, _T("banks"));
    if (!d.accounts.empty())
        tl.AddItem(d.accounts, _T("accounts"));
    if (!d.positions.empty())
        tl.AddItem(d.positions, _T("positions"));
    if (!d.orders.empty())
        tl.AddItem(d.orders, _T("orders"));
    if (!d.trades.empty())
        tl.AddItem(d.trades, _T("trades"));
    if (!d.transfers.empty())
        tl.AddItem(d.transfers, _T("transfers"));
}

static void DefineStruct(trader_dll::FemasRtnBank& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.f->BankID, _T("id"));
    tl.AddItem(d.f->BankBrchID, _T("brch_id"));
    tl.AddItem(d.f->BankAccount, _T("account"));
    tl.AddItemEnum(d.f->BankID, _T("name"), {
        { "01", _T("工商银行") },
        { "02", _T("农业银行") },
        { "03", _T("中国银行") },
        { "04", _T("建设银行") },
        { "05", _T("交通银行") },
        { "06", _T("浦发银行") },
        { "07", _T("兴业银行") },
        { "08", _T("汇丰银行") },
        { "09", _T("光大银行") },
        { "10", _T("招商银行") },
        { "11", _T("中信银行") },
        { "12", _T("民生银行") },
        { "13", _T("平安银行") },
    });
}

static void DefineStruct(trader_dll::FemasRtnDataItem& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.trade, _T("trade"));
}

static void DefineStruct(trader_dll::FemasRtnDataPack& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.data, _T("data"));
}

static FemasRtnDataUser& GetUser(FemasRtnDataPack& pack, std::string user_id)
{
    FemasRtnDataUser& user = pack.data[0].trade[user_id];
    user.user_id = user_id;
    return user;
}

static FemasRtnDataPosition& GetPosition(FemasRtnDataPack& pack, std::string user_id, std::string exchange_id, std::string ins_id, char hedge_flag)
{
    if (exchange_id == "ZCE")
        exchange_id = "CZCE";
    std::string symbol = exchange_id + "." + ins_id;
    if (hedge_flag != USTP_FTDC_CHF_Speculation)
        symbol += std::string(hedge_flag, 1);
    FemasRtnDataPosition& position = GetUser(pack, user_id).positions[symbol];
    return position;
}

void ApiLogger(const char* funcname, const char* fieldtype, void* field, int nRequestID = 0, long retcode = 0, const char* retmsg = NULL, bool bIsLast = false)
{
}

void CFemasOpenTraderSpiHandler::OnFrontConnected()
{
    m_trader->m_bTraderApiConnected = true;
    m_trader->ReqLogin();
    m_trader->OutputNotify(0, _T("已经连接到交易前置"));
}

void CFemasOpenTraderSpiHandler::OnQryFrontConnected()
{
    m_trader->OutputNotify(0, _T("已经连接到查询前置"));
}

void CFemasOpenTraderSpiHandler::OnFrontDisconnected(int nReason)
{
    ApiLogger("OnFrontDisconnected，已经断开与交易前置的连接", "", NULL);
    m_trader->m_bTraderApiConnected = false;
    m_trader->OutputNotify(1, _T("已经断开与交易前置的连接"));
}

void CFemasOpenTraderSpiHandler::OnQryFrontDisconnected(int nReason)
{
    ApiLogger("OnQryFrontDisconnected()", "", NULL);
    m_trader->OutputNotify(2, _T("已经断开与查询前置的连接"));
}

void CFemasOpenTraderSpiHandler::OnHeartBeatWarning(int nTimeLapse)
{
    ApiLogger("OnHeartBeatWarning()", "", NULL);
}

void CFemasOpenTraderSpiHandler::OnPackageStart(int nTopicID, int nSequenceNo)
{
    ApiLogger("OnPackageStart()", "", NULL);
}

void CFemasOpenTraderSpiHandler::OnPackageEnd(int nTopicID, int nSequenceNo)
{
    ApiLogger("OnPackageEnd()", "", NULL);
}

void CFemasOpenTraderSpiHandler::OnRspError(CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    ApiLogger("OnRspError", pRspInfo->ErrorMsg, NULL);
}

void CFemasOpenTraderSpiHandler::OnRspUserLogin(CUstpFtdcRspUserLoginField* pRspUserLogin, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID == 0) {
        m_trader->OutputNotify(0, _T("登录成功"));
        m_trader->ReqEnablePush();
        char json_str[1024];
        sprintf(json_str, ("{"\
                           "\"aid\": \"rtn_data\","\
                           "\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
                           "\"user_id\" : \"%s\","\
                           "\"session_id\" : \"0\","\
                           "\"max_order_id\" : %s"
                           "}}}}]}")
                , pRspUserLogin->UserID
                , pRspUserLogin->UserID
                , (pRspUserLogin->MaxOrderLocalID[0] == '\0') ? "0" : (pRspUserLogin->MaxOrderLocalID));
        m_trader->Output(json_str);
    } else {
        m_trader->OutputNotify(pRspInfo->ErrorID, _T("交易服务器登录失败, ") + GbkToWide(pRspInfo->ErrorMsg));
    }
}

void CFemasOpenTraderSpiHandler::OnRspUserPasswordUpdate(CUstpFtdcUserPasswordUpdateField* pUserPasswordUpdate, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspNotifyConfirm(CUstpFtdcInputNotifyConfirmField* pInputNotifyConfirm, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryNotifyConfirm(CUstpFtdcNotifyConfirmField* pNotifyConfirm, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspOrderInsert(CUstpFtdcInputOrderField* pInputOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        m_trader->OutputNotify(pRspInfo->ErrorID, _T("下单失败,") + GbkToWide(pRspInfo->ErrorMsg));
    }
}

void CFemasOpenTraderSpiHandler::OnRspQryTrade(CUstpFtdcTradeField* pTrade, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pTrade)
        return;
    FemasRtnDataPack pack;
    FemasRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
    std::string trade_key = std::string(pTrade->OrderSysID) + "|" + std::string(pTrade->TradeID);
    FemasRtnDataTrade& trade = user.trades[trade_key];
    trade.f = pTrade;
    std::string json_str;
    TinySerialize::VarToJsonString(pack, json_str);
    m_trader->Output(json_str);
}

void CFemasOpenTraderSpiHandler::OnRtnOrder(CUstpFtdcOrderField* pOrder)
{
    FemasRtnDataPack pack;
    FemasRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
    std::string order_key = pOrder->UserOrderLocalID;
    FemasRtnDataOrder& order = user.orders[order_key];
    order.f = pOrder;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OnRtnTrade(CUstpFtdcTradeField* pTrade)
{
    FemasRtnDataPack pack;
    FemasRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
    std::string trade_key = std::string(pTrade->OrderSysID) + "|" + std::string(pTrade->TradeID);
    FemasRtnDataTrade& trade = user.trades[trade_key];
    trade.f = pTrade;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OnErrRtnOrderAction(CUstpFtdcOrderActionField* pOrderAction, CUstpFtdcRspInfoField* pRspInfo)
{
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(pRspInfo->ErrorID, _T("撤单失败:") + GbkToWide(pRspInfo->ErrorMsg));
}

void CFemasOpenTraderSpiHandler::OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField* pInstrumentStatus)
{
}

void CFemasOpenTraderSpiHandler::OnRtnQuote(CUstpFtdcRtnQuoteField* pRtnQuote)
{
}

void CFemasOpenTraderSpiHandler::OnErrRtnQuoteInsert(CUstpFtdcInputQuoteField* pInputQuote, CUstpFtdcRspInfoField* pRspInfo)
{
}

void CFemasOpenTraderSpiHandler::OnErrRtnQuoteAction(CUstpFtdcOrderActionField* pOrderAction, CUstpFtdcRspInfoField* pRspInfo)
{
}

void CFemasOpenTraderSpiHandler::OnRtnForQuote(CUstpFtdcReqForQuoteField* pReqForQuote)
{
}

void CFemasOpenTraderSpiHandler::OnRtnMarginCombinationLeg(CUstpFtdcMarginCombinationLegField* pMarginCombinationLeg)
{
}

void CFemasOpenTraderSpiHandler::OnRtnMarginCombAction(CUstpFtdcInputMarginCombActionField* pInputMarginCombAction)
{
}

void CFemasOpenTraderSpiHandler::OnRspQueryUserLogin(CUstpFtdcRspUserLoginField* pRspUserLogin, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID == 0) {
        m_trader->ReqEnablePush();
        m_trader->ReqQryAccount();
        m_trader->ReqQryPosition();
        m_trader->ReqQryOrder();
        m_trader->ReqQryTrade();
        m_trader->ReqQueryBank();
        m_trader->ReqQueryTransferLog();
    } else {
        m_trader->OutputNotify(pRspInfo->ErrorID, _T("查询服务器登录失败, ") + GbkToWide(pRspInfo->ErrorMsg));
    }
}

void CFemasOpenTraderSpiHandler::OnRspQryInvestorFee(CUstpFtdcInvestorFeeField* pInvestorFee, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInvestorMargin(CUstpFtdcInvestorMarginField* pInvestorMargin, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInvestorCombPosition(CUstpFtdcRspInvestorCombPositionField* pRspInvestorCombPosition, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInvestorLegPosition(CUstpFtdcRspInvestorLegPositionField* pRspInvestorLegPosition, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInstrumentGroup(CUstpFtdcRspInstrumentGroupField* pRspInstrumentGroup, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryClientMarginCombType(CUstpFtdcRspClientMarginCombTypeField* pRspClientMarginCombType, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspExecOrderInsert(CUstpFtdcInputExecOrderField* pInputExecOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspExecOrderAction(CUstpFtdcInputExecOrderActionField* pInputExecOrderAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRtnExecOrder(CUstpFtdcExecOrderField* pExecOrder)
{
}

void CFemasOpenTraderSpiHandler::OnErrRtnExecOrderInsert(CUstpFtdcInputExecOrderField* pInputExecOrder, CUstpFtdcRspInfoField* pRspInfo)
{
}

void CFemasOpenTraderSpiHandler::OnErrRtnExecOrderAction(CUstpFtdcInputExecOrderActionField* pInputExecOrderAction, CUstpFtdcRspInfoField* pRspInfo)
{
}

void CFemasOpenTraderSpiHandler::OnRspQrySystemTime(CUstpFtdcRspQrySystemTimeField* pRspQrySystemTime, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRtnRiskNotify(CUstpFtdcRiskNotifyField* pRiskNotify)
{
}

void CFemasOpenTraderSpiHandler::OnRspQuerySettlementInfo(CUstpFtdcSettlementRspField* pSettlementRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspTradingAccountPasswordUpdate(CUstpFtdcTradingAccountPasswordUpdateRspField* pTradingAccountPasswordUpdateRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQueryEWarrantOffset(CUstpFtdcEWarrantOffsetFieldRspField* pEWarrantOffsetFieldRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQueryTransferSeriaOffset(CUstpFtdcTransferSerialFieldRspField* pTransferSerialFieldRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID != 0) {
        m_trader->OutputNotify(0, _T("OnRspQueryTransferSeriaOffset, ") + GbkToWide(pRspInfo->ErrorMsg));
    } else {
        FemasRtnDataPack pack;
        FemasRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
        std::string trade_key = std::string(pTransferSerialFieldRsp->FutureSerial);
        FemasRtnDataTransfer& trade = user.transfers[trade_key];
        trade.f = pTransferSerialFieldRsp;
        std::string json_str;
        TinySerialize::VarToJsonString(pack, json_str);
        m_trader->Output(json_str);
    }
}

void CFemasOpenTraderSpiHandler::OnRspQueryAccountregister(CUstpFtdcAccountregisterRspField* pAccountregisterRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pAccountregisterRsp)
        return;
    FemasRtnDataPack pack;
    FemasRtnBank& bank = pack.data[0].trade[pAccountregisterRsp->AccountID].banks[pAccountregisterRsp->BankID];
    bank.f = pAccountregisterRsp;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OnRspFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(0, _T("OnRspFromBankToFutureByFuture, ") + GbkToWide(pRspInfo->ErrorMsg));
}

void CFemasOpenTraderSpiHandler::OnRtnFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq)
{
    m_trader->ReqQueryTransferLog();
    //if (pTransferFieldReq)
    //	m_trader->OutputNotify(0, _T("OnRtnFromBankToFutureByFuture, "));
}

void CFemasOpenTraderSpiHandler::OnRspFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(0, _T("OnRspFromFutureToBankByFuture, ") + GbkToWide(pRspInfo->ErrorMsg));
}

void CFemasOpenTraderSpiHandler::OnRtnFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq)
{
    m_trader->ReqQueryTransferLog();
}

void CFemasOpenTraderSpiHandler::OnRtnFromFutureToBankByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq)
{
    m_trader->ReqQueryTransferLog();
}

void CFemasOpenTraderSpiHandler::OnRtnFromBankToFutureByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq)
{
    m_trader->ReqQueryTransferLog();
}

void CFemasOpenTraderSpiHandler::OnRspQueryBankAccountMoneyByFuture(CUstpFtdcAccountFieldRspField* pAccountFieldRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID != 0) {
        m_trader->OutputNotify(0, _T("OnRspQueryBankAccountMoneyByFuture, ") + GbkToWide(pRspInfo->ErrorMsg));
    } else {
        wchar_t buf[1024];
        wsprintf(buf, _T("%.2f"), pAccountFieldRsp->CurrentBalance);
        m_trader->OutputNotify(0, buf, L"TEXT");
    }
}

void CFemasOpenTraderSpiHandler::OnRtnQueryBankBalanceByFuture(CUstpFtdcAccountFieldRtnField* pAccountFieldRtn)
{
}

void CFemasOpenTraderSpiHandler::OnRtnOpenAccountByBank(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField)
{
}

void CFemasOpenTraderSpiHandler::OnRtnCancelAccountByBank(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField)
{
}

void CFemasOpenTraderSpiHandler::OnRspFromBankToFutureByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspFromFutureToBankByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRtnFromBankToFutureByJSD(CUstpFtdcJSDMoneyField* pJSDMoney)
{
}

void CFemasOpenTraderSpiHandler::OnRtnFromFutureToBankByJSD(CUstpFtdcJSDMoneyField* pJSDMoney)
{
}

void CFemasOpenTraderSpiHandler::OnRtnTransferInvestorAccountChanged(CUstpFtdcTransferFieldReqField* pTransferFieldReq)
{
    m_trader->ReqQueryTransferLog();
    //if (pTransferFieldReq)
    //	m_trader->OutputNotify(0, _T("OnRtnTransferInvestorAccountChanged, "));
}

void CFemasOpenTraderSpiHandler::OnRspSignUpAccountByFuture(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspCancleAccountByFuture(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQuerySignUpOrCancleAccountStatus(CUstpFtdcQuerySignUpOrCancleAccountStatusRspFieldField* pQuerySignUpOrCancleAccountStatusRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspChangeAccountByFuture(CUstpFtdcChangeAccountRspFieldField* pChangeAccountRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField* pSimTradeAccountInfo, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspCheckOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField* pSimTradeAccountInfo, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspCFMMCTradingAccountKey(CUstpFtdcCFMMCTradingAccountKeyRspField* pCFMMCTradingAccountKeyRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRtnMsgNotify(CUstpFtdcMsgNotifyField* pMsgNotify)
{
}

void CFemasOpenTraderSpiHandler::OnRtnClientPositionChange(CUstpFtdcClientPositionChangeField* pClientPositionChange)
{
    FemasRtnDataPack pack;
    FemasRtnDataPosition& position = GetPosition(pack, m_trader->m_user_id, pClientPositionChange->ExchangeID, pClientPositionChange->InstrumentID, pClientPositionChange->HedgeFlag);
    position.f = pClientPositionChange;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OnRtnInvestorAccountChange(CUstpFtdcInvestorAccountChangeField* pInvestorAccountChange)
{
    FemasRtnDataPack pack;
    FemasRtnDataAccount& account = GetUser(pack, pInvestorAccountChange->AccountID).accounts[pInvestorAccountChange->Currency];
    account.f = pInvestorAccountChange;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OutputPack(FemasRtnDataPack& pack)
{
    std::string json_str;
    TinySerialize::VarToJsonString(pack, json_str);
    m_trader->Output(json_str);
}

void CFemasOpenTraderSpiHandler::OnRspQryEnableRtnMoneyPositoinChange(CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQueryHisOrder(CUstpFtdcOrderField* pOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQueryHisTrade(CUstpFtdcTradeField* pTrade, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryUserInvestor(CUstpFtdcRspUserInvestorField* pRspUserInvestor, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryTradingCode(CUstpFtdcRspTradingCodeField* pRspTradingCode, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField* pRspInvestorPosition, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pRspInvestorPosition)
        return;
    FemasRtnDataPack pack;
    FemasRtnDataPosition& position = GetPosition(pack, m_trader->m_user_id, pRspInvestorPosition->ExchangeID, pRspInvestorPosition->InstrumentID, pRspInvestorPosition->HedgeFlag);
    position.f2 = pRspInvestorPosition;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OnRspSubscribeTopic(CUstpFtdcDisseminationField* pDissemination, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryComplianceParam(CUstpFtdcRspComplianceParamField* pRspComplianceParam, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryTopic(CUstpFtdcDisseminationField* pDissemination, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInvestorAccount(CUstpFtdcRspInvestorAccountField* pRspInvestorAccount, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pRspInvestorAccount)
        return;
    FemasRtnDataPack pack;
    FemasRtnDataAccount& account = GetUser(pack, pRspInvestorAccount->AccountID).accounts["CNY"];
    account.f2 = pRspInvestorAccount;
    OutputPack(pack);
}

void CFemasOpenTraderSpiHandler::OnErrRtnOrderInsert(CUstpFtdcInputOrderField* pInputOrder, CUstpFtdcRspInfoField* pRspInfo)
{
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(pRspInfo->ErrorID, _T("下单失败, ") + GbkToWide(pRspInfo->ErrorMsg));
}

void CFemasOpenTraderSpiHandler::OnRspOrderAction(CUstpFtdcOrderActionField* pOrderAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(pRspInfo->ErrorID, _T("撤单失败, ") + GbkToWide(pRspInfo->ErrorMsg));
}

void CFemasOpenTraderSpiHandler::OnRspQuoteInsert(CUstpFtdcInputQuoteField* pInputQuote, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQuoteAction(CUstpFtdcQuoteActionField* pQuoteAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspForQuote(CUstpFtdcReqForQuoteField* pReqForQuote, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspMarginCombAction(CUstpFtdcInputMarginCombActionField* pInputMarginCombAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRtnFlowMessageCancel(CUstpFtdcFlowMessageCancelField* pFlowMessageCancel)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryInstrument(CUstpFtdcRspInstrumentField* pRspInstrument, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryExchange(CUstpFtdcRspExchangeField* pRspExchange, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CFemasOpenTraderSpiHandler::OnRspQryOrder(CUstpFtdcOrderField* pOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pOrder)
        return;
    FemasRtnDataPack pack;
    FemasRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
    std::string order_key = pOrder->UserOrderLocalID;
    FemasRtnDataOrder& order = user.orders[order_key];
    order.f = pOrder;
    OutputPack(pack);
}

}