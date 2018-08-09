/////////////////////////////////////////////////////////////////////////
///@file ctp_spi.cpp
///@brief	CTP回调接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ctp_spi.h"

#include "ctp_define.h"
#include "../datetime.h"
#include "../rapid_serialize.h"
#include "trader_ctp.h"


namespace trader_dll
{

static std::string GuessExchangeId(std::string instrument_id)
{
    if (instrument_id.size() == 5
        && instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
        && instrument_id[1] >= 'A' && instrument_id[1] <= 'Z'
        ) {
        return "CZCE";
    }
    if (instrument_id.size() == 5
        && instrument_id[0] >= 'a' && instrument_id[0] <= 'z'
        && instrument_id[1] >= '0' && instrument_id[1] <= '9'
        ) {
        return "DCE";
    }
    if (instrument_id.size() == 5
        && instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
        ) {
        return "CFFEX";
    }
    if (instrument_id.size() == 6
        && instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
        && instrument_id[1] >= 'A' && instrument_id[1] <= 'Z'
        ) {
        return "CFFEX";
    }
    if (instrument_id.size() == 6
        && instrument_id[0] >= 'a' && instrument_id[0] <= 'z'
        && instrument_id[1] >= 'a' && instrument_id[1] <= 'z'
        ) {
        if ((instrument_id[0] == 'c' && instrument_id[1] == 's')
            || (instrument_id[0] == 'f' && instrument_id[1] == 'b')
            || (instrument_id[0] == 'b' && instrument_id[1] == 'b')
            || (instrument_id[0] == 'j' && instrument_id[1] == 'd')
            || (instrument_id[0] == 'p' && instrument_id[1] == 'p')
            || (instrument_id[0] == 'j' && instrument_id[1] == 'm'))
            return "DCE";
        if (instrument_id[0] == 's' && instrument_id[1] == 'c')
            return "INE";
        return "SHFE";
    }
    return "UNKNOWN";
}

void CCtpSpiHandler::OnFrontConnected()
{
    m_trader->m_bTraderApiConnected = true;
    m_trader->SendLoginRequest();
    m_trader->OutputNotify(0, u8"已经连接到交易前置");
}

void CCtpSpiHandler::OnFrontDisconnected(int nReason)
{
    m_trader->m_bTraderApiConnected = false;
    m_trader->OutputNotify(1, u8"已经断开与交易前置的连接");
}

void CCtpSpiHandler::OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
}

void CCtpSpiHandler::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID == 0) {
        m_trader->SetSession(pRspUserLogin->TradingDay, pRspUserLogin->FrontID, pRspUserLogin->SessionID, atoi(pRspUserLogin->MaxOrderRef));
        m_trader->OutputNotify(0, u8"登录成功");
        char json_str[1024];
        sprintf_s(json_str, sizeof(json_str), (u8"{"\
                           "\"aid\": \"rtn_data\","\
                           "\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
                           "\"user_id\" : \"%s\","\
                           "\"session_id\" : \"%d\","\
                           "\"max_order_id\" : %s"
                           "}}}}]}")
                , pRspUserLogin->UserID
                , pRspUserLogin->UserID
                , pRspUserLogin->SessionID
                , (pRspUserLogin->MaxOrderRef[0] == '\0') ? "0" : (pRspUserLogin->MaxOrderRef));
        m_trader->Output(json_str);
        m_trader->ReqConfirmSettlement();
        m_trader->m_need_query_account = true;
        m_trader->m_need_query_positions = true;
    } else {
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"交易服务器登录失败, " + GBKToUTF8(pRspInfo->ErrorMsg));
    }
}

void CCtpSpiHandler::OnRtnOrder(CThostFtdcOrderField* pOrder)
{
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    //找到委托单
    trader_dll::RemoteOrderKey remote_key;
    remote_key.exchange_id = pOrder->ExchangeID;
    remote_key.instrument_id = pOrder->InstrumentID;
    remote_key.front_id = pOrder->FrontID;
    remote_key.session_id = pOrder->SessionID;
    remote_key.order_ref = pOrder->OrderRef;
    remote_key.order_sys_id = pOrder->OrderSysID;
    std::string local_key;
    m_trader->OrderIdRemoteToLocal(remote_key, &local_key);
    Order& order = m_trader->GetOrder(local_key);
    //委托单初始属性(由下单者在下单前确定, 不再改变)
    order.order_id = local_key;
    order.exchange_id = pOrder->ExchangeID;
    order.instrument_id = pOrder->InstrumentID;
    switch (pOrder->Direction)
    {
    case THOST_FTDC_D_Buy:
        order.direction = kDirectionBuy;
        break;
    case THOST_FTDC_D_Sell:
        order.direction = kDirectionSell;
        break;
    default:
        break;
    }
    switch (pOrder->CombOffsetFlag[0])
    {
    case THOST_FTDC_OF_Open:
        order.direction = kOffsetOpen;
        break;
    case THOST_FTDC_OF_CloseToday:
        order.direction = kOffsetCloseToday;
        break;
    case THOST_FTDC_OF_Close:
    case THOST_FTDC_OF_CloseYesterday:
    case THOST_FTDC_OF_ForceOff:
    case THOST_FTDC_OF_LocalForceClose:
        order.direction = kOffsetClose;
        break;
    default:
        break;
    }
    order.volume_orign = pOrder->VolumeTotalOriginal;
    switch (pOrder->OrderPriceType)
    {
    case THOST_FTDC_OPT_AnyPrice:
        order.price_type = kPriceTypeAny;
        break;
    case THOST_FTDC_OPT_LimitPrice:
        order.price_type = kPriceTypeLimit;
        break;
    case THOST_FTDC_OPT_BestPrice:
        order.price_type = kPriceTypeBest;
        break;
    case THOST_FTDC_OPT_FiveLevelPrice:
        order.price_type = kPriceTypeFiveLevel;
        break;
    default:
        break;
    }
    order.limit_price = pOrder->LimitPrice;
    switch (pOrder->TimeCondition)
    {
    case THOST_FTDC_TC_IOC:
        order.time_condition = kOrderTimeConditionIOC;
        break;
    case THOST_FTDC_TC_GFS:
        order.time_condition = kOrderTimeConditionGFS;
        break;
    case THOST_FTDC_TC_GFD:
        order.time_condition = kOrderTimeConditionGFD;
        break;
    case THOST_FTDC_TC_GTD:
        order.time_condition = kOrderTimeConditionGTD;
        break;
    case THOST_FTDC_TC_GTC:
        order.time_condition = kOrderTimeConditionGTC;
        break;
    case THOST_FTDC_TC_GFA:
        order.time_condition = kOrderTimeConditionGFA;
        break;
    default:
        break;
    }
    switch (pOrder->VolumeCondition)
    {
    case THOST_FTDC_VC_AV:
        order.volume_condition = kOrderVolumeConditionAny;
        break;
    case THOST_FTDC_VC_MV:
        order.volume_condition = kOrderVolumeConditionMin;
        break;
    case THOST_FTDC_VC_CV:
        order.volume_condition = kOrderVolumeConditionAll;
        break;
    default:
        break;
    }
    //下单后获得的信息(由期货公司返回, 不会改变)
    DateTime dt;
    dt.time.microsecond = 0;
    sscanf(pOrder->InsertDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
    sscanf(pOrder->InsertTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
    order.insert_date_time = DateTimeToEpochNano(&dt);
    order.exchange_order_id = pOrder->OrderSysID;
    //委托单当前状态
    switch (pOrder->OrderStatus)
    {
    case THOST_FTDC_OST_AllTraded:
    case THOST_FTDC_OST_PartTradedNotQueueing:
    case THOST_FTDC_OST_NoTradeNotQueueing:
    case THOST_FTDC_OST_Canceled:
        order.status = kOrderStatusFinished;
        break;
    case THOST_FTDC_OST_PartTradedQueueing:
    case THOST_FTDC_OST_NoTradeQueueing:
    case THOST_FTDC_OST_Unknown:
        order.status = kOrderStatusAlive;
        break;
    default:
        break;
    }
    order.volume_left = pOrder->VolumeTotal;
    order.changed = true;
    //要求重新查询持仓
    m_trader->m_need_query_positions = true;
    m_trader->m_something_changed = true;
    m_trader->SendUserData();
}

void CCtpSpiHandler::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    std::string local_key;
    m_trader->FindLocalOrderId(pTrade->OrderSysID, &local_key);
    std::string trade_key = local_key + "|" + std::string(pTrade->TradeID);
    Trade& trade = m_trader->GetTrade(trade_key);
    trade.trade_id = trade_key;
    trade.exchange_id = pTrade->ExchangeID;
    trade.instrument_id = pTrade->InstrumentID;
    trade.order_id = local_key;
    trade.exchange_trade_id = pTrade->TradeID;
    switch (pTrade->Direction)
    {
    case THOST_FTDC_D_Buy:
        trade.direction = kDirectionBuy;
        break;
    case THOST_FTDC_D_Sell:
        trade.direction = kDirectionSell;
        break;
    default:
        break;
    }
    switch (pTrade->OffsetFlag)
    {
    case THOST_FTDC_OF_Open:
        trade.direction = kOffsetOpen;
        break;
    case THOST_FTDC_OF_CloseToday:
        trade.direction = kOffsetCloseToday;
        break;
    case THOST_FTDC_OF_Close:
    case THOST_FTDC_OF_CloseYesterday:
    case THOST_FTDC_OF_ForceOff:
    case THOST_FTDC_OF_LocalForceClose:
        trade.direction = kOffsetClose;
        break;
    default:
        break;
    }
    trade.volume = pTrade->Volume;
    trade.price = pTrade->Price;

    DateTime dt;
    dt.time.microsecond = 0;
    sscanf(pTrade->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
    sscanf(pTrade->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
    trade.trade_date_time = DateTimeToEpochNano(&dt);
    trade.commission = 0.0;
    trade.changed = true;
    m_trader->m_something_changed = true;
    m_trader->SendUserData();
}

void CCtpSpiHandler::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pRspInvestorPosition, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pRspInvestorPosition)
        return;
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    std::string exchange_id = trader_dll::GuessExchangeId(pRspInvestorPosition->InstrumentID);
    std::string position_key = exchange_id + "." + pRspInvestorPosition->InstrumentID;
    Position& position = m_trader->GetPosition(position_key);
    position.exchange_id = exchange_id;
    position.instrument_id = pRspInvestorPosition->InstrumentID;
    if (pRspInvestorPosition->PosiDirection == THOST_FTDC_PD_Long) {
        if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today) {
            position.volume_long_today = pRspInvestorPosition->Position;
            position.volume_long_frozen_today = pRspInvestorPosition->LongFrozen;
        } else {
            position.volume_long_his = pRspInvestorPosition->Position;
            position.volume_long_frozen_his = pRspInvestorPosition->LongFrozen;
        }
        position.position_cost_long = pRspInvestorPosition->PositionCost;
        position.open_cost_long = pRspInvestorPosition->OpenCost;
        position.margin_long = pRspInvestorPosition->UseMargin;
    } else {
        if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today) {
            position.volume_short_today = pRspInvestorPosition->Position;
            position.volume_short_frozen_today = pRspInvestorPosition->ShortFrozen;
        } else {
            position.volume_short_his = pRspInvestorPosition->Position;
            position.volume_short_frozen_his = pRspInvestorPosition->ShortFrozen;
        }
        position.position_cost_short = pRspInvestorPosition->PositionCost;
        position.open_cost_short = pRspInvestorPosition->OpenCost;
        position.margin_short = pRspInvestorPosition->UseMargin;
    }
    position.changed = true;
    if(bIsLast){
        m_trader->m_something_changed = true;
        m_trader->SendUserData();
    }
}

void CCtpSpiHandler::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pRspInvestorAccount, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pRspInvestorAccount)
        return;
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    Account& account = m_trader->GetAccount(pRspInvestorAccount->CurrencyID);

    //账号及币种
    account.account_id = pRspInvestorAccount->AccountID;
    account.currency = pRspInvestorAccount->CurrencyID;
    //本交易日开盘前状态
    account.pre_balance = pRspInvestorAccount->PreBalance;
    //本交易日内已发生事件的影响
    account.deposit = pRspInvestorAccount->Deposit;
    account.withdraw = pRspInvestorAccount->Withdraw;
    account.close_profit = pRspInvestorAccount->CloseProfit;
    account.commission = pRspInvestorAccount->Commission;
    account.premium = pRspInvestorAccount->CashIn;
    account.static_balance = pRspInvestorAccount->Balance - pRspInvestorAccount->PositionProfit;
    //当前持仓盈亏
    account.position_profit = pRspInvestorAccount->PositionProfit;
    account.float_profit = 0;
    //当前权益
    account.balance = pRspInvestorAccount->Balance;
    //保证金占用, 冻结及风险度
    account.margin = pRspInvestorAccount->CurrMargin;
    account.frozen_margin = pRspInvestorAccount->FrozenMargin;
    account.frozen_commission = pRspInvestorAccount->FrozenCommission;
    account.frozen_premium = pRspInvestorAccount->FrozenCash;
    account.available = pRspInvestorAccount->Available;
    account.changed = true;
    if (bIsLast) {
        m_trader->m_something_changed = true;
        m_trader->SendUserData();
    }
}

void CCtpSpiHandler::OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"下单失败, " + GBKToUTF8(pRspInfo->ErrorMsg));
    }
}

void CCtpSpiHandler::OnRspOrderAction(CThostFtdcInputOrderActionField* pOrderAction, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"撤单失败, " + GBKToUTF8(pRspInfo->ErrorMsg));
}

}
