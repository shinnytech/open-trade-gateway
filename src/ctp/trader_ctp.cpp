/////////////////////////////////////////////////////////////////////////
///@file trader_ctp.cpp
///@brief	CTP接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trader_ctp.h"

#include "../utility.h"
#include "ctp_spi.h"
#include "ctp/ThostFtdcTraderApi.h"
#include "../rapid_serialize.h"
#include "../numset.h"



namespace trader_dll
{
TraderCtp::TraderCtp(std::function<void(const std::string&)> callback)
    : TraderBase(callback)
    , m_pThostTraderSpiHandler(NULL)
    , m_api(NULL)
    , m_data(m_data_pack.data[0])
{
    m_front_id = 0;
    m_session_id = 0;
    m_order_ref = 0;
    //m_quotes_map = &(m_data.quotes);
    m_running = true;
    m_next_qry_dt = 0;
}

TraderCtp::~TraderCtp(void)
{
    if (m_api)
        m_api->Release();
}

void TraderCtp::ProcessInput(const std::string& json_str)
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
    } else if (aid == "rtn_data") {
        OnRtnData();
    }
}

void TraderCtp::OnInit()
{
    std::string flow_file_name = GenerateUniqFileName();
    m_api = CThostFtdcTraderApi::CreateFtdcTraderApi(flow_file_name.c_str());
    m_pThostTraderSpiHandler = new CCtpSpiHandler(this);
    m_api->RegisterSpi(m_pThostTraderSpiHandler);
    m_broker_id = m_req_login.broker.ctp_broker_id;
    for (auto it = m_req_login.broker.trading_fronts.begin(); it != m_req_login.broker.trading_fronts.end(); ++it) {
        std::string& f = *it;
        m_api->RegisterFront((char*)(f.c_str()));
    }
    m_api->SubscribePrivateTopic(THOST_TERT_RESUME);
    m_api->SubscribePublicTopic(THOST_TERT_RESUME);
    m_user_id = m_req_login.user_name;
    m_password = m_req_login.password;
    m_product_info = m_req_login.broker.product_info;
    m_user_file_name = g_config.user_file_path + "/" + m_user_id;
    LoadFromFile();
    m_api->Init();
}

void TraderCtp::SendLoginRequest()
{
    CThostFtdcReqUserLoginField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.Password, m_password.c_str());
    strcpy_x(field.UserProductInfo, m_product_info.c_str());
    int ret = m_api->ReqUserLogin(&field, 33);
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
    m_api->ReqOrderInsert(&d.f, 0);
    char symbol[1024];
    sprintf_s(symbol, sizeof(symbol), "%s.%s", d.f.ExchangeID, d.f.InstrumentID);
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
    m_api->ReqOrderAction(&d.f, 0);
}

void TraderCtp::OnRtnData()
{
    rapidjson::Value* dt = rapidjson::Pointer("/data").Get(*(ss.m_doc));
    if (dt && dt->IsArray()) {
        for (auto& v : dt->GetArray())
            ss.ToVar(m_data, &v);
    }
}

int TraderCtp::ReqQryAccount()
{
    CThostFtdcQryTradingAccountField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    return m_api->ReqQryTradingAccount(&field, 0);
}

int TraderCtp::ReqQryPosition()
{
    CThostFtdcQryInvestorPositionField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    return m_api->ReqQryInvestorPosition(&field, 0);
}

void TraderCtp::ReqConfirmSettlement()
{
    CThostFtdcSettlementInfoConfirmField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    m_api->ReqSettlementInfoConfirm(&field, 0);
}

void TraderCtp::OnIdle()
{
    /*
    有空的时候就要做的几件事情:
        标记为需查询的项, 如果离上次查询时间够远, 应该发起查询
        标记为需通知主程序的项, 如果知道现在处于一波的结束位置, 应该向主程序推送
    */
    // 检查是否有需要向主程序发送的
    UpdateUserData();
    // 检查是否需要发持仓查询请求
    int now = GetTickCount();
    if (m_need_query_account && m_next_qry_dt < now) {
        if (ReqQryAccount() == 0) {
            m_need_query_account = false;
        } else {
            m_next_qry_dt = now + 1100;
        }
        return;
    }
    auto& orders = GetOrders();
    if (!orders.empty()){
        if (now < m_next_qry_dt)
            m_next_qry_dt = now + 1000;
        m_need_query_positions = true;
        orders.clear();
    }
    if (m_need_query_positions && m_next_qry_dt < now) {
        if (ReqQryPosition() == 0) {
            m_need_query_positions = false;
            m_need_query_account = true;
        }
        m_next_qry_dt = now + 1100;
    }
}

/*
处理行情变更时,需要更新持仓盈亏和对应账户资金的包
*/
void TraderCtp::UpdateUserData()
{
    bool pos_changed = false;
    double total_position_profit = 0;
    double total_float_profit = 0;
    auto& positions = GetPositions();
    for (auto it = positions.begin(); it != positions.end(); ++it) {
        CtpPosition& ps = it->second;
        if (IsValid(ps.position_profit()))
            total_position_profit += ps.position_profit();
        if (IsValid(ps.float_profit()))
            total_float_profit += ps.float_profit();
        ps.changed = true;
    }
    CtpAccount& acc = GetAccount();
    if (pos_changed || acc.changed) {
        double dv = total_position_profit - acc.position_profit;
        acc.position_profit = total_position_profit;
        acc.available += dv;
        acc.balance += dv;
        if (IsValid(acc.available) && IsValid(acc.balance) && !IsZero(acc.balance))
            acc.risk_ratio = 1.0 - acc.available / acc.balance;
        else
            acc.risk_ratio = NAN;
        acc.changed = true;
        std::string json_str;
        SerializerCtp nss;
        nss.FromVar(m_data_pack);
        nss.ToString(&json_str);
    }
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

std::map<std::string, trader_dll::CtpPosition>& TraderCtp::GetPositions()
{
    return m_data_pack.data[0].trade[m_user_id].m_positions;
}

std::map<std::string, trader_dll::CtpOrder>& TraderCtp::GetOrders()
{
    return m_data_pack.data[0].trade[m_user_id].m_orders;
}

trader_dll::CtpAccount& TraderCtp::GetAccount()
{
    return m_data_pack.data[0].trade[m_user_id].m_accounts[("CNY")];
}

void TraderCtp::OrderIdLocalToRemote(const std::string& local_order_key, RemoteOrderKey* remote_order_key)
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

void TraderCtp::OrderIdRemoteToLocal(const RemoteOrderKey& remote_order_key, std::string* local_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    auto it = m_ordermap_remote_local.find(remote_order_key);
    if (it == m_ordermap_remote_local.end()) {
        char buf[1024];
        sprintf(buf, "UNKNOWN.%s.%08x.%d", remote_order_key.order_ref.c_str(), remote_order_key.session_id, remote_order_key.front_id);
        *local_order_key = buf;
        m_ordermap_local_remote[*local_order_key] = remote_order_key;
        m_ordermap_remote_local[remote_order_key] = *local_order_key;
    }else{
        *local_order_key = it->second;
        RemoteOrderKey& r = const_cast<RemoteOrderKey&>(it->first);
        if (!remote_order_key.order_sys_id.empty())
            r.order_sys_id = remote_order_key.order_sys_id;
    }
}

void TraderCtp::FindLocalOrderId(const std::string& order_sys_id, std::string* local_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    for(auto it = m_ordermap_remote_local.begin(); it != m_ordermap_remote_local.end(); ++it){
        const RemoteOrderKey& rkey = it->first;
        if (rkey.order_sys_id == order_sys_id){
            *local_order_key = it->second;
            return;
        }
    }
}

void TraderCtp::SetSession(std::string trading_day, int front_id, int session_id, int order_ref)
{
    if(m_trading_day != trading_day){
        m_ordermap_local_remote.clear();
        m_ordermap_remote_local.clear();
    }
    m_trading_day = trading_day;
    m_front_id = front_id;
    m_session_id = session_id;
    m_order_ref = order_ref + 1;
}

void TraderCtp::Release()
{
    TraderBase::Release();
    m_api->Release();
}

}