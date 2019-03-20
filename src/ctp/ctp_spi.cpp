/////////////////////////////////////////////////////////////////////////
///@file ctp_spi.cpp
///@brief	CTP回调接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ctp_spi.h"

#include "../log.h"
#include "../datetime.h"
#include "../rapid_serialize.h"
#include "../md_service.h"
#include "ctp_define.h"
#include "trader_ctp.h"


namespace trader_dll
{

static std::string GuessExchangeId(std::string instrument_id)
{
    if (instrument_id.size() > 11) {
        //组合
        if ((instrument_id[0] == 'S' && instrument_id[1] == 'P' && instrument_id[2] == 'D')
        || (instrument_id[0] == 'I' && instrument_id[1] == 'P' && instrument_id[2] == 'S')
        )
            return "CZCE";
        else
            return "DCE";
    }
    if (instrument_id.size() > 8
        && instrument_id[0] == 'm'
        && instrument_id[5] == '-'
        && (instrument_id[6] == 'C' || instrument_id[6] == 'P')
        && instrument_id[7] == '-'
    ){
        //大连期权
        //"^DCE\.m(\d\d)(\d\d)-([CP])-(\d+)$"
        return "DCE";
    }
    if (instrument_id.size() > 7
        && instrument_id[0] == 'c'
        && (instrument_id[6] == 'C' || instrument_id[6] == 'P')
    ){
        //上海期权
        //"^SHFE\.cu(\d\d)(\d\d)([CP])(\d+)$"
        return "SHFE";
    }
    if (instrument_id.size() > 6
        && instrument_id[0] == 'S'
        && instrument_id[1] == 'R'
        && (instrument_id[5] == 'C' || instrument_id[5] == 'P')
    ){
        //郑州期权
        //"CZCE\.SR(\d)(\d\d)([CP])(\d+)"
        return "CZCE";
    }
    if (instrument_id.size() == 5
        && instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
        && instrument_id[1] >= 'A' && instrument_id[1] <= 'Z'
        ) {
        //郑州期货
        return "CZCE";
    }
    if (instrument_id.size() == 5
        && instrument_id[0] >= 'a' && instrument_id[0] <= 'z'
        && instrument_id[1] >= '0' && instrument_id[1] <= '9'
        ) {
        //大连期货
        return "DCE";
    }
    if (instrument_id.size() == 5
        && instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
        ) {
        //中金期货
        return "CFFEX";
    }
    if (instrument_id.size() == 6
        && instrument_id[0] >= 'A' && instrument_id[0] <= 'Z'
        && instrument_id[1] >= 'A' && instrument_id[1] <= 'Z'
        ) {
        //中金期货
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
            || (instrument_id[0] == 'e' && instrument_id[1] == 'g')
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
    Log(LOG_INFO, NULL, "ctp OnFrontConnected, instance=%p, UserID=%s", m_trader, m_trader->m_user_id.c_str());
    m_trader->ReqAuthenticate();
    m_trader->OutputNotify(0, u8"已经连接到交易前置");
}

void CCtpSpiHandler::OnFrontDisconnected(int nReason)
{
    Log(LOG_WARNING, NULL, "ctp OnFrontDisconnected, instance=%p, nReason=%d, UserID=%s", m_trader, nReason, m_trader->m_user_id.c_str());
    m_trader->m_logined.store(false);
    m_trader->OutputNotify(1, u8"已经断开与交易前置的连接");
}

void CCtpSpiHandler::OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if(pRspInfo){
        Log(LOG_INFO, NULL, "ctp OnRspError, instance=%p, UserID=%s, ErrMsg=%s", m_trader, m_trader->m_user_id.c_str(), GBKToUTF8(pRspInfo->ErrorMsg).c_str());
    }
}

void CCtpSpiHandler::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pRspInfo || pRspInfo->ErrorID != 0){
        Log(LOG_WARNING, NULL, "ctp OnRspAuthenticate, instance=%p, UserID=%s, ErrorID=%d, ErrMsg=%s"
            , m_trader, m_trader->m_user_id.c_str()
            , pRspInfo?pRspInfo->ErrorID:-999
            , pRspInfo?GBKToUTF8(pRspInfo->ErrorMsg).c_str():""
            );
        return;
    }
    Log(LOG_INFO, NULL, "ctp OnRspAuthenticate, instance=%p, UserID=%s"
        , m_trader, m_trader->m_user_id.c_str()
        );
    if (pRspInfo->ErrorID == 0){
        m_trader->SendLoginRequest();
    }
}

void CCtpSpiHandler::OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspUserLogin, instance=%p, UserID=%s, ErrMsg=%s, TradingDay=%s, FrontId=%d, SessionId=%d, MaxOrderRef=%s"
        , m_trader, m_trader->m_user_id.c_str(), GBKToUTF8(pRspInfo->ErrorMsg).c_str()
        , pRspUserLogin->TradingDay, pRspUserLogin->FrontID, pRspUserLogin->SessionID, pRspUserLogin->MaxOrderRef
        );
    m_trader->m_position_ready = false;
    m_trader->m_req_login_dt.store(0);
    if (pRspInfo->ErrorID != 0){
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"交易服务器登录失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
        return;
    }
    m_trader->SetSession(pRspUserLogin->TradingDay, pRspUserLogin->FrontID, pRspUserLogin->SessionID, atoi(pRspUserLogin->MaxOrderRef));
    m_trader->m_logined.store(true);
    m_trader->OutputNotify(0, u8"登录成功");
    char json_str[1024];
    sprintf(json_str, (u8"{"\
                        "\"aid\": \"rtn_data\","\
                        "\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
                        "\"user_id\" : \"%s\","\
                        "\"trading_day\" : \"%s\""
                        "}}}}]}")
            , pRspUserLogin->UserID
            , pRspUserLogin->UserID
            , pRspUserLogin->TradingDay
            );
    m_trader->Output(json_str);
    if(g_config.auto_confirm_settlement)
        m_trader->ReqConfirmSettlement();
    else if (m_settlement_info.empty())
        m_trader->ReqQrySettlementInfoConfirm();
    m_trader->m_req_position_id++;
    m_trader->m_req_account_id++;
    m_trader->m_need_query_bank.store(true);
    m_trader->m_need_query_register.store(true);
}

void CCtpSpiHandler::OnRspUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspUserPasswordUpdate, instance=%p, UserID=%s", m_trader, m_trader->m_user_id.c_str());
    if (!pRspInfo)
        return;
    if (pRspInfo->ErrorID == 0){
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"修改密码成功");
    }else{
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"修改密码失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspQrySettlementInfoConfirm, instance=%p, UserID=%s, ConfirmDate=%s", m_trader, m_trader->m_user_id.c_str(), pSettlementInfoConfirm?pSettlementInfoConfirm->ConfirmDate:"");
    if (pSettlementInfoConfirm && std::string(pSettlementInfoConfirm->ConfirmDate) >= m_trader->m_trading_day)
        return;
    m_trader->m_need_query_settlement.store(true);
}

void CCtpSpiHandler::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!pSettlementInfo)
        return;
    Log(LOG_INFO, NULL, "ctp OnRspQrySettlementInfo, instance=%p, UserID=%s", m_trader, m_trader->m_user_id.c_str());
    m_settlement_info += pSettlementInfo->Content;
    if(bIsLast){
        m_trader->m_need_query_settlement.store(false);
        m_trader->OutputNotify(0, GBKToUTF8(m_settlement_info.c_str()), "INFO", "SETTLEMENT");
    }
}

void CCtpSpiHandler::OnRtnOrder(CThostFtdcOrderField* pOrder)
{
    Log(LOG_INFO, NULL, "ctp OnRtnOrder, instance=%p, UserID=%s, InstrumentId=%s, OrderRef=%s, Session=%d", m_trader, m_trader->m_user_id.c_str(), pOrder->InstrumentID, pOrder->OrderRef, pOrder->SessionID);
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    //找到委托单
    trader_dll::RemoteOrderKey remote_key;
    remote_key.exchange_id = pOrder->ExchangeID;
    remote_key.instrument_id = pOrder->InstrumentID;
    remote_key.front_id = pOrder->FrontID;
    remote_key.session_id = pOrder->SessionID;
    remote_key.order_ref = pOrder->OrderRef;
    remote_key.order_sys_id = pOrder->OrderSysID;
    trader_dll::LocalOrderKey local_key;
    m_trader->OrderIdRemoteToLocal(remote_key, &local_key);
    Order& order = m_trader->GetOrder(local_key.order_id);
    //委托单初始属性(由下单者在下单前确定, 不再改变)
    order.seqno = m_trader->m_data_seq++;
    order.user_id = local_key.user_id;
    order.order_id = local_key.order_id;
    order.exchange_id = pOrder->ExchangeID;
    order.instrument_id = pOrder->InstrumentID;
    auto ins = md_service::GetInstrument(order.symbol());
    if (!ins){
        Log(LOG_ERROR, NULL, "ctp OnRtnOrder, instrument not exist, instance=%p, UserID=%s, symbol=%s", m_trader, m_trader->m_user_id.c_str(), order.symbol().c_str());
        return;
    }
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
        order.offset = kOffsetOpen;
        break;
    case THOST_FTDC_OF_CloseToday:
        order.offset = kOffsetCloseToday;
        break;
    case THOST_FTDC_OF_Close:
    case THOST_FTDC_OF_CloseYesterday:
    case THOST_FTDC_OF_ForceOff:
    case THOST_FTDC_OF_LocalForceClose:
        order.offset = kOffsetClose;
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
    order.last_msg = GBKToUTF8(pOrder->StatusMsg);
    order.changed = true;
    //要求重新查询持仓
    m_trader->m_req_position_id++;
    m_trader->m_req_account_id++;
    m_trader->m_something_changed = true;
    m_trader->SendUserData();
    //发送下单成功通知
    if (pOrder->OrderStatus != THOST_FTDC_OST_Canceled 
     && pOrder->OrderStatus != THOST_FTDC_OST_Unknown 
     && pOrder->OrderStatus != THOST_FTDC_OST_NoTradeNotQueueing 
     && pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing 
    ){
        std::unique_lock<std::mutex> lck(m_trader->m_order_action_mtx);
        auto it = m_trader->m_insert_order_set.find(pOrder->OrderRef);
        if (it != m_trader->m_insert_order_set.end()){
            m_trader->m_insert_order_set.erase(it);
            m_trader->OutputNotify(1, u8"下单成功");
        }
    }
    if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled
     && pOrder->VolumeTotal > 0){
        std::unique_lock<std::mutex> lck(m_trader->m_order_action_mtx);
        auto it = m_trader->m_cancel_order_set.find(order.order_id);
        if (it != m_trader->m_cancel_order_set.end()){
            m_trader->m_cancel_order_set.erase(it);
            m_trader->OutputNotify(1, u8"撤单成功");
        } else {
            auto it2 = m_trader->m_insert_order_set.find(pOrder->OrderRef);
            if (it2 != m_trader->m_insert_order_set.end()){
                m_trader->m_insert_order_set.erase(it2);
                m_trader->OutputNotify(1, u8"下单失败, " + order.last_msg, "WARNING");
            }
        }
    }
}

void CCtpSpiHandler::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
    Log(LOG_INFO, NULL, "ctp OnRtnTrade, instance=%p, UserID=%s, InstrumentId=%s, OrderRef=%s", m_trader, m_trader->m_user_id.c_str(), pTrade->InstrumentID, pTrade->OrderRef);
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    LocalOrderKey local_key;
    m_trader->FindLocalOrderId(pTrade->ExchangeID, pTrade->OrderSysID, &local_key);
    std::string trade_key = local_key.order_id + "|" + std::string(pTrade->TradeID);
    Trade& trade = m_trader->GetTrade(trade_key);
    trade.seqno = m_trader->m_data_seq++;
    trade.trade_id = trade_key;
    trade.user_id = local_key.user_id;
    trade.order_id = local_key.order_id;
    trade.exchange_id = pTrade->ExchangeID;
    trade.instrument_id = pTrade->InstrumentID;
    trade.exchange_trade_id = pTrade->TradeID;
    auto ins = md_service::GetInstrument(trade.symbol());
    if (!ins){
        Log(LOG_ERROR, NULL, "ctp OnRtnTrade, instrument not exist, instance=%p, UserID=%s, symbol=%s", m_trader, m_trader->m_user_id.c_str(), trade.symbol().c_str());
        return;
    }
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
        trade.offset = kOffsetOpen;
        break;
    case THOST_FTDC_OF_CloseToday:
        trade.offset = kOffsetCloseToday;
        break;
    case THOST_FTDC_OF_Close:
    case THOST_FTDC_OF_CloseYesterday:
    case THOST_FTDC_OF_ForceOff:
    case THOST_FTDC_OF_LocalForceClose:
        trade.offset = kOffsetClose;
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
    if (pRspInvestorPosition){
        Log(LOG_INFO, NULL, "ctp OnRspQryInvestorPosition, instance=%p, nRequestID=%d, bIsLast=%d, UserID=%s, InstrumentId=%s, ExchangeId=%s"
            , m_trader, nRequestID, bIsLast, m_trader->m_user_id.c_str(), pRspInvestorPosition->InstrumentID, pRspInvestorPosition->ExchangeID);
        std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
        std::string exchange_id = GuessExchangeId(pRspInvestorPosition->InstrumentID);
        std::string symbol = exchange_id + "." + pRspInvestorPosition->InstrumentID;
        auto ins = md_service::GetInstrument(symbol);
        if (!ins){
            Log(LOG_ERROR, NULL, "ctp OnRspQryInvestorPosition, instrument not exist, instance=%p, UserID=%s, symbol=%s", m_trader, m_trader->m_user_id.c_str(), symbol.c_str());
            return;
        }
        Position& position = m_trader->GetPosition(symbol);
        position.user_id = pRspInvestorPosition->InvestorID;
        position.exchange_id = exchange_id;
        position.instrument_id = pRspInvestorPosition->InstrumentID;
        if (pRspInvestorPosition->PosiDirection == THOST_FTDC_PD_Long) {
            if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today) {
                position.volume_long_today = pRspInvestorPosition->Position;
                position.volume_long_frozen_today = pRspInvestorPosition->ShortFrozen;
                position.position_cost_long_today = pRspInvestorPosition->PositionCost;
                position.open_cost_long_today = pRspInvestorPosition->OpenCost;
                position.margin_long_today = pRspInvestorPosition->UseMargin;
            } else {
                position.volume_long_his = pRspInvestorPosition->Position;
                position.volume_long_frozen_his = pRspInvestorPosition->ShortFrozen;
                position.position_cost_long_his = pRspInvestorPosition->PositionCost;
                position.open_cost_long_his = pRspInvestorPosition->OpenCost;
                position.margin_long_his = pRspInvestorPosition->UseMargin;
            }
            position.position_cost_long = position.position_cost_long_today + position.position_cost_long_his;
            position.open_cost_long = position.open_cost_long_today + position.open_cost_long_his;
            position.margin_long = position.margin_long_today + position.margin_long_his;
        } else {
            if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today) {
                position.volume_short_today = pRspInvestorPosition->Position;
                position.volume_short_frozen_today = pRspInvestorPosition->LongFrozen;
                position.position_cost_short_today = pRspInvestorPosition->PositionCost;
                position.open_cost_short_today = pRspInvestorPosition->OpenCost;
                position.margin_short_today = pRspInvestorPosition->UseMargin;
            } else {
                position.volume_short_his = pRspInvestorPosition->Position;
                position.volume_short_frozen_his = pRspInvestorPosition->LongFrozen;
                position.position_cost_short_his = pRspInvestorPosition->PositionCost;
                position.open_cost_short_his = pRspInvestorPosition->OpenCost;
                position.margin_short_his = pRspInvestorPosition->UseMargin;
            }
            position.position_cost_short = position.position_cost_short_today + position.position_cost_short_his;
            position.open_cost_short = position.open_cost_short_today + position.open_cost_short_his;
            position.margin_short = position.margin_short_today + position.margin_short_his;
        }
        position.changed = true;
    }
    if(bIsLast){
        m_trader->m_rsp_position_id.store(nRequestID);
        m_trader->m_something_changed = true;
        m_trader->m_position_ready = true;
        m_trader->SendUserData();
    }
}

void CCtpSpiHandler::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pRspInvestorAccount, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (bIsLast){
        m_trader->m_rsp_account_id.store(nRequestID);
    }
    Log(LOG_INFO, NULL, "ctp OnRspQryTradingAccount, instance=%p, UserID=%s, ErrorID=%d", m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999);
    if (!pRspInvestorAccount)
        return;
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    Account& account = m_trader->GetAccount(pRspInvestorAccount->CurrencyID);

    //账号及币种
    account.user_id = pRspInvestorAccount->AccountID;
    account.currency = pRspInvestorAccount->CurrencyID;
    //本交易日开盘前状态
    account.pre_balance = pRspInvestorAccount->PreBalance;
    //本交易日内已发生事件的影响
    account.deposit = pRspInvestorAccount->Deposit;
    account.withdraw = pRspInvestorAccount->Withdraw;
    account.close_profit = pRspInvestorAccount->CloseProfit;
    account.commission = pRspInvestorAccount->Commission;
    account.premium = pRspInvestorAccount->CashIn;
    account.static_balance = pRspInvestorAccount->PreBalance - pRspInvestorAccount->PreCredit
                            -pRspInvestorAccount->PreMortgage + pRspInvestorAccount->Mortgage
                            -pRspInvestorAccount->Withdraw + pRspInvestorAccount->Deposit;
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

void CCtpSpiHandler::OnRspQryContractBank(CThostFtdcContractBankField *pContractBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspQryContractBank, instance=%p, UserID=%s, ErrorID=%d", m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999);
    if (!pContractBank){
        m_trader->m_need_query_bank.store(false);
        return;
    }
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    Bank& bank = m_trader->GetBank(pContractBank->BankID);
    bank.bank_id = pContractBank->BankID;
    bank.bank_name = GBKToUTF8(pContractBank->BankName);
    if (bIsLast) {
        m_trader->m_need_query_bank.store(false);
    }
}


void CCtpSpiHandler::OnRspQryAccountregister(CThostFtdcAccountregisterField *pAccountregister, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspQryAccountregister, instance=%p, UserID=%s, ErrorID=%d", m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999);
    if (!pAccountregister){
        m_trader->m_need_query_register.store(false);
	return;
    }
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    Bank& bank = m_trader->GetBank(pAccountregister->BankID);
    bank.changed = true;
    if (bIsLast) {
        m_trader->m_need_query_register.store(false);
        m_trader->m_something_changed = true;
        m_trader->SendUserData();
    }
}

void CCtpSpiHandler::OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspOrderInsert, instance=%p, UserID=%s, ErrorID=%d", m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999);
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
        //找到委托单
        trader_dll::RemoteOrderKey remote_key;
        remote_key.exchange_id = pInputOrder->ExchangeID;
        remote_key.instrument_id = pInputOrder->InstrumentID;
        remote_key.front_id = m_trader->m_front_id;
        remote_key.session_id = m_trader->m_session_id;
        remote_key.order_ref = pInputOrder->OrderRef;
        trader_dll::LocalOrderKey local_key;
        m_trader->OrderIdRemoteToLocal(remote_key, &local_key);
        Order& order = m_trader->GetOrder(local_key.order_id);
        //委托单初始属性(由下单者在下单前确定, 不再改变)
        order.seqno = 0;
        order.user_id = local_key.user_id;
        order.order_id = local_key.order_id;
        order.exchange_id = pInputOrder->ExchangeID;
        order.instrument_id = pInputOrder->InstrumentID;
        switch (pInputOrder->Direction)
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
        switch (pInputOrder->CombOffsetFlag[0])
        {
            case THOST_FTDC_OF_Open:
                order.offset = kOffsetOpen;
                break;
            case THOST_FTDC_OF_CloseToday:
                order.offset = kOffsetCloseToday;
                break;
            case THOST_FTDC_OF_Close:
            case THOST_FTDC_OF_CloseYesterday:
            case THOST_FTDC_OF_ForceOff:
            case THOST_FTDC_OF_LocalForceClose:
                order.offset = kOffsetClose;
                break;
            default:
                break;
        }
        order.volume_orign = pInputOrder->VolumeTotalOriginal;
        switch (pInputOrder->OrderPriceType)
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
        order.limit_price = pInputOrder->LimitPrice;
        switch (pInputOrder->TimeCondition)
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
        switch (pInputOrder->VolumeCondition)
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
        //委托单当前状态
        order.volume_left = pInputOrder->VolumeTotalOriginal;
        order.status = kOrderStatusFinished;
        order.last_msg = GBKToUTF8(pRspInfo->ErrorMsg);
        order.changed = true;
        m_trader->m_something_changed = true;
        m_trader->SendUserData();
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"下单失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnRspOrderAction(CThostFtdcInputOrderActionField* pOrderAction, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspOrderAction, instance=%p, UserID=%s, ErrorID=%d", m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999);
    if (pRspInfo->ErrorID != 0)
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"撤单失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
}

void CCtpSpiHandler::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    Log(LOG_INFO, NULL, "ctp OnErrRtnOrderInsert, instance=%p, UserID=%s, ErrorID=%d, ErrorMsg=%s"
        , m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999
        , pRspInfo?GBKToUTF8(pRspInfo->ErrorMsg).c_str():""
        );
    if (pInputOrder && pRspInfo && pRspInfo->ErrorID != 0){
        if (memcmp(&m_trader->m_input_order, pInputOrder, sizeof(m_trader->m_input_order)) == 0)
            m_trader->OutputNotify(pRspInfo->ErrorID, u8"下单失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
    Log(LOG_INFO, NULL, "ctp OnErrRtnOrderAction, instance=%p, UserID=%s, ErrorID=%d, ErrorMsg=%s"
        , m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999
        , pRspInfo?GBKToUTF8(pRspInfo->ErrorMsg).c_str():""
        );
    if (pOrderAction && pRspInfo && pRspInfo->ErrorID != 0){
        if (strcmp(m_trader->m_action_order.OrderRef, pOrderAction->OrderRef) == 0)
            m_trader->OutputNotify(pRspInfo->ErrorID, u8"撤单失败, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnRspQryTransferSerial(CThostFtdcTransferSerialField *pTransferSerial, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Log(LOG_INFO, NULL, "ctp OnRspQryTransferSerial, instance=%p, UserID=%s, ErrorID=%d", m_trader, m_trader->m_user_id.c_str(), pRspInfo?pRspInfo->ErrorID:-999);
    if (!pTransferSerial)
        return;
    std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
    TransferLog& d = m_trader->GetTransferLog(std::to_string(pTransferSerial->PlateSerial));
    d.currency = pTransferSerial->CurrencyID;
    d.amount = pTransferSerial->TradeAmount;
    if (pTransferSerial->TradeCode == std::string("202002"))
        d.amount = 0 - d.amount;
    DateTime dt;
    dt.time.microsecond = 0;
    sscanf(pTransferSerial->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
    sscanf(pTransferSerial->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
    d.datetime = DateTimeToEpochNano(&dt);
    d.error_id = pTransferSerial->ErrorID;
    d.error_msg = GBKToUTF8(pTransferSerial->ErrorMsg);
    if (bIsLast) {
        m_trader->m_something_changed = true;
        m_trader->SendUserData();
    }
}

void CCtpSpiHandler::OnRtnFromBankToFutureByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
    if (!pRspTransfer)
        return;
    Log(LOG_INFO, NULL, "ctp OnRtnFromBankToFutureByFuture, instance=%p, UserID=%s", m_trader, m_trader->m_user_id.c_str());
    if(pRspTransfer->ErrorID == 0){
        std::lock_guard<std::mutex> lck(m_trader->m_data_mtx);
        TransferLog& d = m_trader->GetTransferLog(std::to_string(pRspTransfer->PlateSerial));
        d.currency = pRspTransfer->CurrencyID;
        d.amount = pRspTransfer->TradeAmount;
        if (pRspTransfer->TradeCode == std::string("202002"))
            d.amount = 0 - d.amount;
        DateTime dt;
        dt.time.microsecond = 0;
        sscanf(pRspTransfer->TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
        sscanf(pRspTransfer->TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
        d.datetime = DateTimeToEpochNano(&dt);
        d.error_id = pRspTransfer->ErrorID;
        d.error_msg = GBKToUTF8(pRspTransfer->ErrorMsg);
        m_trader->m_something_changed = true;
        m_trader->SendUserData();
        m_trader->m_req_account_id++;
    } else {
        m_trader->OutputNotify(pRspTransfer->ErrorID, u8"银期错误, " + GBKToUTF8(pRspTransfer->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnRtnFromFutureToBankByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
    return OnRtnFromBankToFutureByFuture(pRspTransfer);
}

void CCtpSpiHandler::OnErrRtnBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"银行资金转期货错误, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{
    if (pRspInfo && pRspInfo->ErrorID != 0) {
        m_trader->OutputNotify(pRspInfo->ErrorID, u8"期货资金转银行错误, " + GBKToUTF8(pRspInfo->ErrorMsg), "WARNING");
    }
}

void CCtpSpiHandler::OnRtnTradingNotice(CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo)
{
    auto s = GBKToUTF8(pTradingNoticeInfo->FieldContent);
    if (!s.empty())
        m_trader->OutputNotify(0, s);
}

}
