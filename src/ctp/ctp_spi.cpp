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
    if (instrument_id.size() == 6
        && instrument_id[0] >= 'a' && instrument_id[0] <= 'z'
        && instrument_id[1] >= 'a' && instrument_id[1] <= 'z'
        ) {
        return "SHFE";
    }
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
    return "UNKNOWN";
}

static CtpRtnDataUser& GetUser(CtpRtnDataPack& pack, std::string user_id)
{
    CtpRtnDataUser& user = pack.data[0].trade[user_id];
    user.user_id = user_id;
    return user;
}

static CtpRtnDataPosition& GetPosition(CtpRtnDataPack& pack, std::string user_id, std::string position_key)
{
    CtpRtnDataPosition& position = GetUser(pack, user_id).positions[position_key];
    return position;
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
    CtpRtnDataPack pack;
    CtpRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
    trader_dll::RemoteOrderKey remote_key;
    remote_key.exchange_id = pOrder->ExchangeID;
    remote_key.instrument_id = pOrder->InstrumentID;
    remote_key.front_id = pOrder->FrontID;
    remote_key.session_id = pOrder->SessionID;
    remote_key.order_ref = pOrder->OrderRef;
    remote_key.order_sys_id = pOrder->OrderSysID;
    std::string local_key;
    m_trader->OrderIdRemoteToLocal(remote_key, &local_key);
    CtpRtnDataOrder& order = user.orders[local_key];
    order.local_key = local_key;
    order.f = pOrder;
    order.is_rtn = true;
    std::string symbol = std::string(pOrder->ExchangeID) + "." + pOrder->InstrumentID;
    ProcessOrder(&(user.units)
        , local_key
        , symbol
        , pOrder->Direction
        , pOrder->CombOffsetFlag[0]
        , pOrder->OrderStatus != THOST_FTDC_OST_Canceled ? pOrder->VolumeTotal : 0
    );
    OutputPack(pack);
}

void CCtpSpiHandler::OnRtnTrade(CThostFtdcTradeField* pTrade)
{
    std::string local_key;
    m_trader->FindLocalOrderId(pTrade->OrderSysID, &local_key);
    CtpRtnDataPack pack;
    CtpRtnDataUser& user = GetUser(pack, m_trader->m_user_id);
    std::string trade_key = local_key + "|" + std::string(pTrade->TradeID);
    CtpRtnDataTrade& trade = user.trades[trade_key];
    trade.local_key = local_key;
    trade.f = pTrade;
    std::string symbol = std::string(pTrade->ExchangeID) + "." + pTrade->InstrumentID;
    ProcessTrade(&(user.units)
        , local_key
        , symbol
        , pTrade->Direction
        , pTrade->OffsetFlag
        , pTrade->Volume
        , pTrade->Price
    );
    OutputPack(pack);
}

void CCtpSpiHandler::OutputPack(CtpRtnDataPack& pack)
{
    std::string json_str;
    SerializerCtp ss;
    ss.FromVar(pack);
    ss.ToString(&json_str);
    m_trader->Output(json_str);
}

void CCtpSpiHandler::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pRspInvestorPosition, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    static CtpRtnDataPack pack;
    if (!pRspInvestorPosition)
        return;
    std::string exchange_id = trader_dll::GuessExchangeId(pRspInvestorPosition->InstrumentID);
    std::string position_key = exchange_id + "." + pRspInvestorPosition->InstrumentID;
    CtpRtnDataPosition& position = GetPosition(pack, m_trader->m_user_id, position_key);
    position.exchange_id = exchange_id;
    position.ins_id = pRspInvestorPosition->InstrumentID;
    if (pRspInvestorPosition->PosiDirection == THOST_FTDC_PD_Long) {
        if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today) {
            position.volume_long_today += pRspInvestorPosition->Position;
            position.volume_long_frozen_today += pRspInvestorPosition->LongFrozen;
        } else {
            position.volume_long_his += pRspInvestorPosition->Position;
            position.volume_long_frozen += pRspInvestorPosition->LongFrozen;
        }
        position.position_cost_long += pRspInvestorPosition->PositionCost;
        position.open_cost_long += pRspInvestorPosition->OpenCost;
        position.margin_long += pRspInvestorPosition->UseMargin;
    } else {
        if (pRspInvestorPosition->PositionDate == THOST_FTDC_PSD_Today) {
            position.volume_short_today += pRspInvestorPosition->Position;
            position.volume_short_frozen_today += pRspInvestorPosition->ShortFrozen;
        } else {
            position.volume_short_his += pRspInvestorPosition->Position;
            position.volume_short_frozen += pRspInvestorPosition->ShortFrozen;
        }
        position.position_cost_short += pRspInvestorPosition->PositionCost;
        position.open_cost_short += pRspInvestorPosition->OpenCost;
        position.margin_short += pRspInvestorPosition->UseMargin;
    }
    if(bIsLast){
        OutputPack(pack);
        pack.clear();
    }
}

void CCtpSpiHandler::OnRspQryTradingAccount(CThostFtdcTradingAccountField* pRspInvestorAccount, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast)
{
    if (!pRspInvestorAccount)
        return;
    CtpRtnDataPack pack;
    CtpRtnDataAccount& account = GetUser(pack, pRspInvestorAccount->AccountID).accounts["CNY"];
    account.f = pRspInvestorAccount;
    OutputPack(pack);
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

void CCtpSpiHandler::ProcessTrade(std::map<std::string, CtpRtnTradeUnit>* units, std::string order_id, std::string symbol, long direction, long offset, int volume, double price)
{
    ProcessUnitTrade(units, "", symbol, direction, offset, volume, price);
    size_t pos = 0;
    while (true) {
        pos = order_id.find(".", pos);
        if (pos == std::string::npos)
            return;
        std::string unit_id = order_id.substr(0, pos);
        ProcessUnitTrade(units, unit_id, symbol, direction, offset, volume, price);
        pos++;
    }
}

void CCtpSpiHandler::ProcessUnitTrade(std::map<std::string, CtpRtnTradeUnit>* units, std::string unit_id, std::string symbol, long direction, long offset, int volume, double price)
{
    auto& unit = m_trade_units[unit_id];
    auto& position = unit.positions[symbol];
    position.unit_id = unit_id;
    position.symbol = symbol;
    if (offset == THOST_FTDC_OF_Open) {
        if (direction == THOST_FTDC_D_Buy) {
            position.volume_long += volume;
            position.cost_long += volume * price;
        } else {
            position.volume_short += volume;
            position.cost_short += volume * price;
        }
    } else {
        double close_profit = 0;
        if (direction == THOST_FTDC_D_Buy) {
            int v = std::min(volume, position.volume_short);
            if (v > 0) {
                double c = position.cost_short / position.volume_short * v;
                close_profit = c - v * price;
                position.cost_short -= c;
                position.volume_short -= v;
            }
        } else {
            int v = std::min(volume, position.volume_long);
            if (v > 0) {
                double c = position.cost_long / position.volume_long * v;
                close_profit = v * price - c;
                position.cost_long -= c;
                position.volume_long -= v;
            }
        }
    }
    (*units)[unit_id] = unit;
}

void CCtpSpiHandler::ProcessOrder(std::map<std::string, CtpRtnTradeUnit>* units, std::string order_id, std::string symbol, long direction, long offset, int volume)
{
    int delta_volume = GetOrderChangeVolume(order_id, volume);
    ProcessUnitOrder(units, "", symbol, direction, offset, delta_volume);
    size_t pos = 0;
    while (true) {
        pos = order_id.find(".", pos);
        if (pos == std::string::npos)
            return;
        std::string unit_id = order_id.substr(0, pos);
        ProcessUnitOrder(units, unit_id, symbol, direction, offset, delta_volume);
        pos++;
    }
}

void CCtpSpiHandler::ProcessUnitOrder(std::map<std::string, CtpRtnTradeUnit>* units, std::string unit_id, std::string symbol, long direction, long offset, int volume_change)
{
    auto& unit = m_trade_units[unit_id];
    auto& position = unit.positions[symbol];
    if (offset == THOST_FTDC_OF_Open) {
        if (direction == THOST_FTDC_D_Buy)
            position.order_volume_buy_open += volume_change;
        else
            position.order_volume_sell_open += volume_change;
    } else {
        if (direction == THOST_FTDC_D_Buy)
            position.order_volume_buy_close += volume_change;
        else
            position.order_volume_sell_close += volume_change;
    }
    (*units)[unit_id] = unit;
}

int CCtpSpiHandler::GetOrderChangeVolume(std::string order_id, int current_volume)
{
    int prev_volume = m_order_volume[order_id];
    int delta = current_volume - prev_volume;
    m_order_volume[order_id] = current_volume;
    return delta;
}
}
