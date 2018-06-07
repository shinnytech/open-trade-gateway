/////////////////////////////////////////////////////////////////////////
///@file trader_ctp.h
///@brief	CTP接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include <map>
#include "../trader_base.h"
#include "ctp_define.h"

class CThostFtdcTraderApi;

namespace trader_dll
{
class CCtpSpiHandler;

class TraderCtp
    : public TraderBase
{
public:
    TraderCtp(std::function<void(const std::string&)> callback);
    virtual ~TraderCtp();
    virtual void ProcessInput(const std::string& json_str) override;

private:
    friend class CCtpSpiHandler;
    virtual void OnIdle() override;
    virtual void OnInit() override;

    void OnClientReqInsertOrder();
    void OnClientReqCancelOrder();
    void OnRtnData();

    void SendLoginRequest();
    int ReqQryAccount();
    int ReqQryPosition();
    void ReqConfirmSettlement();
    void UpdateUserData();

    void LoadFromFile();
    void SaveToFile();
    bool m_bTraderApiConnected;
    CCtpSpiHandler* m_pThostTraderSpiHandler;
    CThostFtdcTraderApi* m_api;
    std::string m_broker_id;
    std::string m_user_id;
    std::string m_password;
    std::string m_product_info;
    int m_session_id;
    int m_front_id;
    int m_order_ref;
    bool m_running;
    RtnDataPack m_data_pack;
    RtnData& m_data;
    std::map<std::string, CtpPosition>& GetPositions();
    std::map<std::string, trader_dll::CtpOrder>& GetOrders();
    trader_dll::CtpAccount& GetAccount();
    void OrderIdLocalToRemote(const std::string& local_order_key, RemoteOrderKey* remote_order_key);
    void OrderIdRemoteToLocal(const RemoteOrderKey& remote_order_key, std::string* local_order_key);
    void FindLocalOrderId(const std::string& order_sys_id, std::string* local_order_key);
    void SetSession(std::string trading_day, int front_id, int session_id, int order_ref);
    virtual void Release() override;
    int m_next_qry_dt;
    bool m_need_query_positions;
    bool m_need_query_account;

    std::map<std::string, RemoteOrderKey> m_ordermap_local_remote;
    std::map<RemoteOrderKey, std::string> m_ordermap_remote_local;
    std::string m_trading_day;
    std::mutex m_ordermap_mtx;
    SerializerCtp ss;
    std::string m_user_file_name;
};
}