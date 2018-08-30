/////////////////////////////////////////////////////////////////////////
///@file trader_ctp.cpp
///@brief	CTP接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trader_ctp.h"

#include "log.h"
#include "ctp/ThostFtdcTraderApi.h"
#include "ctp_spi.h"
#include "../rapid_serialize.h"
#include "../numset.h"
#include "../md_service.h"
#include "../utility.h"


namespace trader_dll
{
TraderCtp::TraderCtp(std::function<void()> callback)
    : TraderBase(callback)
    , m_spi(NULL)
    , m_api(NULL)
{
    m_front_id = 0;
    m_session_id = 0;
    m_order_ref = 0;
    m_next_qry_dt = 0;
    m_next_send_dt = 0;

    m_peeking_message = false;
    m_something_changed = false;
}

void TraderCtp::ProcessInput(const char* json_str)
{
    if (!ss.FromString(json_str))
        return;
    rapidjson::Value* dt = rapidjson::Pointer("/aid").Get(*(ss.m_doc));
    if (!dt || !dt->IsString())
        return;
    std::string aid = dt->GetString();
    if (aid == "insert_order") {
        OnClientReqInsertOrder();
    } else if (aid == "cancel_order") {
        OnClientReqCancelOrder();
    } else if (aid == "req_transfer") {
        OnClientReqTransfer();
    } else if (aid == "peek_message") {
        OnClientPeekMessage();
    }
}

void TraderCtp::OnInit()
{
    std::string flow_file_name = GenerateUniqFileName();
    m_api = CThostFtdcTraderApi::CreateFtdcTraderApi(flow_file_name.c_str());
    m_spi = new CCtpSpiHandler(this);
    m_api->RegisterSpi(m_spi);
    m_broker_id = m_req_login.broker.ctp_broker_id;
    for (auto it = m_req_login.broker.trading_fronts.begin(); it != m_req_login.broker.trading_fronts.end(); ++it) {
        std::string& f = *it;
        m_api->RegisterFront((char*)(f.c_str()));
    }
    m_api->SubscribePrivateTopic(THOST_TERT_RESUME);
    m_api->SubscribePublicTopic(THOST_TERT_RESUME);
    m_user_file_name = m_user_file_path + "/" + m_user_id;
    LoadFromFile();
    m_api->Init();
    Log(LOG_INFO, NULL, "ctp Init, UserID=%s", m_user_id.c_str());
}

void TraderCtp::SendLoginRequest()
{
    CThostFtdcReqUserLoginField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_req_login.broker.ctp_broker_id.c_str());
    strcpy_x(field.UserID, m_req_login.user_name.c_str());
    strcpy_x(field.Password, m_req_login.password.c_str());
    strcpy_x(field.UserProductInfo, m_req_login.broker.product_info.c_str());
    int ret = m_api->ReqUserLogin(&field, 1);
    Log(LOG_INFO, NULL, "ctp ReqUserLogin, UserID=%s, ret=%d", field.UserID, ret);
    assert(ret == 0);
}

void TraderCtp::OnClientReqInsertOrder()
{
    CtpActionInsertOrder d;
    ss.ToVar(d);
    strcpy_x(d.f.BrokerID, m_broker_id.c_str());
    strcpy_x(d.f.UserID, m_user_id.c_str());
    strcpy_x(d.f.InvestorID, m_user_id.c_str());
    RemoteOrderKey rkey;
    rkey.exchange_id = d.f.ExchangeID;
    rkey.instrument_id = d.f.InstrumentID;
    OrderIdLocalToRemote(d.local_key, &rkey);
    strcpy_x(d.f.OrderRef, rkey.order_ref.c_str());
    int r = m_api->ReqOrderInsert(&d.f, 0);
    Log(LOG_INFO, NULL, "ctp ReqOrderInsert, InvestorID=%s, InstrumentID=%s, OrderRef=%s, ret=%d", d.f.InvestorID, d.f.InstrumentID, d.f.OrderRef, r);
    SaveToFile();
}

void TraderCtp::OnClientReqCancelOrder()
{
    CtpActionCancelOrder d;
    ss.ToVar(d);
    RemoteOrderKey rkey;
    OrderIdLocalToRemote(d.local_key, &rkey);
    strcpy_x(d.f.BrokerID, m_broker_id.c_str());
    strcpy_x(d.f.UserID, m_user_id.c_str());
    strcpy_x(d.f.InvestorID, m_user_id.c_str());
    strcpy_x(d.f.OrderRef, rkey.order_ref.c_str());
    strcpy_x(d.f.ExchangeID, rkey.exchange_id.c_str());
    strcpy_x(d.f.InstrumentID, rkey.instrument_id.c_str());
    d.f.SessionID = rkey.session_id;
    d.f.FrontID = rkey.front_id;
    d.f.ActionFlag = THOST_FTDC_AF_Delete;
    d.f.LimitPrice = 0;
    d.f.VolumeChange = 0;
    int r = m_api->ReqOrderAction(&d.f, 0);
    Log(LOG_INFO, NULL, "ctp ReqOrderAction, InvestorID=%s, InstrumentID=%s, OrderRef=%s, ret=%d", d.f.InvestorID, d.f.InstrumentID, d.f.OrderRef, r);
}

void TraderCtp::OnClientReqTransfer()
{
    CThostFtdcReqTransferField f;
    memset(&f, 0, sizeof(f));
    ss.ToVar(f);
    strcpy_x(f.BrokerID, m_broker_id.c_str());
    strcpy_x(f.UserID, m_user_id.c_str());
    strcpy_x(f.AccountID, m_user_id.c_str());
    strcpy_x(f.BankBranchID, "0000");
    f.SecuPwdFlag = THOST_FTDC_BPWDF_BlankCheck;	// 核对密码
    f.BankPwdFlag = THOST_FTDC_BPWDF_NoCheck;	// 核对密码
    f.VerifyCertNoFlag = THOST_FTDC_YNI_No;
    if (f.TradeAmount >= 0) {
        strcpy_x(f.TradeCode, "202001");
        int r = m_api->ReqFromBankToFutureByFuture(&f, 0);
        Log(LOG_INFO, NULL, "ctp ReqFromBankToFutureByFuture, UserID=%s, TradeAmount=%f, ret=%d", f.UserID, f.TradeAmount, r);
    } else {
        strcpy_x(f.TradeCode, "202002");
        f.TradeAmount = -f.TradeAmount;
        int r = m_api->ReqFromFutureToBankByFuture(&f, 0);
        Log(LOG_INFO, NULL, "ctp ReqFromFutureToBankByFuture, UserID=%s, TradeAmount=%f, ret=%d", f.UserID, f.TradeAmount, r);
    }
}

//void TraderCtp::OnClientReqQueryBankAccount()
//{
//    CUstpFtdcAccountFieldReqField field;
//    memset(&field, 0, sizeof(field));
//    strcpy_x(field.BrokerID, m_broker_id.c_str());
//    strcpy_x(field.UserID, m_user_id.c_str());
//    strcpy_x(field.InvestorID, m_user_id.c_str());
//    if (TinySerialize::JsonStringToVar(field, json_str)) {
//        m_api->ReqQueryBankAccountMoneyByFuture(&field, 0);
//    }
//}

int TraderCtp::ReqQryAccount()
{
    CThostFtdcQryTradingAccountField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    int r = m_api->ReqQryTradingAccount(&field, 0);
    Log(LOG_INFO, NULL, "ctp ReqQryTradingAccount, InvestorID=%s, ret=%d", field.InvestorID, r);
    return r;
}

int TraderCtp::ReqQryPosition()
{
    CThostFtdcQryInvestorPositionField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    int r = m_api->ReqQryInvestorPosition(&field, 0);
    Log(LOG_INFO, NULL, "ctp ReqQryInvestorPosition, InvestorID=%s, ret=%d", field.InvestorID, r);
    return r;
}

void TraderCtp::ReqConfirmSettlement()
{
    CThostFtdcSettlementInfoConfirmField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    int r = m_api->ReqSettlementInfoConfirm(&field, 0);
    Log(LOG_INFO, NULL, "ctp ReqSettlementInfoConfirm, InvestorID=%s, ret=%d", field.InvestorID, r);
}

void TraderCtp::ReqQryBank()
{
    CThostFtdcQryContractBankField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    m_api->ReqQryContractBank(&field, 0);
    int r = m_api->ReqQryContractBank(&field, 0);
    Log(LOG_INFO, NULL, "ctp ReqQryContractBank, ret=%d", r);
}

void TraderCtp::ReqQryAccountRegister()
{
    CThostFtdcQryAccountregisterField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    m_api->ReqQryAccountregister(&field, 0);
    int r = m_api->ReqQryAccountregister(&field, 0);
    Log(LOG_INFO, NULL, "ctp ReqQryAccountregister, ret=%d", r);
}

using namespace std::chrono;

void TraderCtp::OnIdle()
{
    //有空的时候, 标记为需查询的项, 如果离上次查询时间够远, 应该发起查询
    long long now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (m_peeking_message && (m_next_send_dt < now)){
        m_next_send_dt = now + 100;
        std::lock_guard<std::mutex> lck(m_data_mtx);
        SendUserData();
    }
    if (m_next_qry_dt >= now)
        return;
    if (m_need_query_positions.exchange(false)) {
        if (ReqQryPosition() != 0) {
            m_need_query_positions.store(true);
        }
        m_need_query_account.store(true);
        m_next_qry_dt = now + 1100;
        return;
    }
    if (m_need_query_account.exchange(false)) {
        if (ReqQryAccount() != 0) {
            m_need_query_account.store(true);
        }
        m_next_qry_dt = now + 1100;
        return;
    }
}

void TraderCtp::OnClientPeekMessage()
{
    std::lock_guard<std::mutex> lck(m_data_mtx);
    m_peeking_message = true;
    //向客户端发送账户信息
    SendUserData();
}

void TraderCtp::SendUserData()
{
    if (!m_peeking_message)
        return;
    //重算所有持仓项的持仓盈亏和浮动盈亏
    double total_position_profit = 0;
    double total_float_profit = 0;
    for (auto it = m_data.m_positions.begin(); it != m_data.m_positions.end(); ++it) {
        const std::string& symbol = it->first;
        Position& ps = it->second;
        if (!ps.ins)
            ps.ins = md_service::GetInstrument(symbol);
        if (!ps.ins){
            Log(LOG_ERROR, NULL, "miss symbol %s when processing position", symbol);
            continue;
        }
        ps.volume_long = ps.volume_long_his + ps.volume_long_today;
        ps.volume_short = ps.volume_short_his + ps.volume_short_today;
        ps.margin = ps.margin_long + ps.margin_short;
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
            if (ps.volume_long > 0) {
                ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
                ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
            }
            if (ps.volume_short > 0) {
                ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
                ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
            }
            ps.changed = true;
            m_something_changed = true;
        }
        if (IsValid(ps.position_profit))
            total_position_profit += ps.position_profit;
        if (IsValid(ps.float_profit))
            total_float_profit += ps.float_profit;
    }
    //重算资金账户
    if(m_something_changed){
        Account& acc = GetAccount("CNY");
        double dv = total_position_profit - acc.position_profit;
        acc.position_profit = total_position_profit;
        acc.float_profit = total_float_profit;
        acc.available += dv;
        acc.balance += dv;
        if (IsValid(acc.available) && IsValid(acc.balance) && !IsZero(acc.balance))
            acc.risk_ratio = 1.0 - acc.available / acc.balance;
        else
            acc.risk_ratio = NAN;
        acc.changed = true;
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

void TraderCtp::SaveToFile()
{
    SerializerCtp s;
    OrderKeyFile kf;
    kf.trading_day = m_trading_day;
    for(auto it = m_ordermap_local_remote.begin(); it != m_ordermap_local_remote.end(); ++it){
        OrderKeyPair item;
        item.local_key = it->first;
        item.remote_key = it->second;
        kf.items.push_back(item);
    }
    s.FromVar(kf);
    s.ToFile(m_user_file_name.c_str());
}

void TraderCtp::LoadFromFile()
{
    SerializerCtp s;
    if(s.FromFile(m_user_file_name.c_str())){
        OrderKeyFile kf;
        s.ToVar(kf);
        for (auto it = kf.items.begin(); it != kf.items.end(); ++it) {
            m_ordermap_local_remote[it->local_key] = it->remote_key;
            m_ordermap_remote_local[it->remote_key] = it->local_key;
        }
        m_trading_day = kf.trading_day;
    }
}

void TraderCtp::OrderIdLocalToRemote(const LocalOrderKey& local_order_key, RemoteOrderKey* remote_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    auto it = m_ordermap_local_remote.find(local_order_key);
    if (it == m_ordermap_local_remote.end()){
        remote_order_key->session_id = m_session_id;
        remote_order_key->front_id = m_front_id;
        remote_order_key->order_ref = std::to_string(m_order_ref++);
        m_ordermap_local_remote[local_order_key] = *remote_order_key;
        m_ordermap_remote_local[*remote_order_key] = local_order_key;
    } else {
        *remote_order_key = it->second;
    }
}

void TraderCtp::OrderIdRemoteToLocal(const RemoteOrderKey& remote_order_key, LocalOrderKey* local_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    auto it = m_ordermap_remote_local.find(remote_order_key);
    if (it == m_ordermap_remote_local.end()) {
        char buf[1024];
        sprintf(buf, "UNKNOWN.%s.%08x.%d", remote_order_key.order_ref.c_str(), remote_order_key.session_id, remote_order_key.front_id);
        local_order_key->order_id = buf;
        local_order_key->user_id = m_user_id;
        m_ordermap_local_remote[*local_order_key] = remote_order_key;
        m_ordermap_remote_local[remote_order_key] = *local_order_key;
    }else{
        *local_order_key = it->second;
        RemoteOrderKey& r = const_cast<RemoteOrderKey&>(it->first);
        if (!remote_order_key.order_sys_id.empty())
            r.order_sys_id = remote_order_key.order_sys_id;
    }
}

void TraderCtp::FindLocalOrderId(const std::string& exchange_id, const std::string& order_sys_id, LocalOrderKey* local_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    for(auto it = m_ordermap_remote_local.begin(); it != m_ordermap_remote_local.end(); ++it){
        const RemoteOrderKey& rkey = it->first;
        if (rkey.order_sys_id == order_sys_id
            && rkey.exchange_id == exchange_id){
            *local_order_key = it->second;
            return;
        }
    }
}

void TraderCtp::SetSession(std::string trading_day, int front_id, int session_id, int order_ref)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    if(m_trading_day != trading_day){
        m_ordermap_local_remote.clear();
        m_ordermap_remote_local.clear();
    }
    m_trading_day = trading_day;
    m_front_id = front_id;
    m_session_id = session_id;
    m_order_ref = order_ref + 1;
}

void TraderCtp::OnFinish()
{
    m_api->Release();
    delete m_spi;
}

}