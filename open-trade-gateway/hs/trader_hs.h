#pragma once
#include <vector>
#include <map>
#include "../trader_base.h"

class CConfigInterface;
class CConnectionInterface;
class CTradeCallback;
class CFutuRequestMode;

namespace trader_dll
{
class HsCallback;

struct HsRemoteOrderKey
{
    std::string exchange_id;
    std::string instrument_id;
    std::string session_id;
    std::string order_ref;
    bool operator < (const HsRemoteOrderKey& key) const
    {
        return order_ref < key.order_ref;
    }
};

class TraderHs
    : public TraderBase
{
public:
    TraderHs();
    virtual ~TraderHs();

protected:
    void OnClientReqLogin(const std::string& json_str);
    void ReqSubscribePush();
    void OnClientReqInsertOrder(const std::string& json_str);
    void OnClientReqCancelOrder(const std::string& json_str);
    void OnConnect(const std::string& binary_str);
    void SendPack(int func_no, void* content, int len);
    void OnHsRspLogin(const std::string& binary_str);
    void OnHsRspQryAccount(const std::string& binary_str);
    void OnHsRspQryPosition(const std::string& binary_str);
    void OnHsRspInsertOrder(const std::string& binary_str);
    void OnHsRspCancelOrder(const std::string& binary_str);
    void OnHsRspSubscribe(const std::string& binary_str);
    void OnHsHeartbeat(const std::string& binary_str);
    void OnHsPush(const std::string& binary_str);

    void ReqQryPosition();
    void ReqQryAccount();
    void ReqQryTrade();

private:
    CConnectionInterface* m_api;
    CConfigInterface* m_config;
    HsCallback* m_callback;

    std::string m_user_id;
    std::string m_password;
    std::string m_client_id;
    std::string m_user_token;
    int m_branch_no;
    int m_op_branch_no;
    std::string m_session_no;


    std::map<std::string, HsRemoteOrderKey> m_ordermap_local_remote;
    std::map<HsRemoteOrderKey, std::string> m_ordermap_remote_local;
    void OrderIdLocalToRemote(const std::string& local_order_key, HsRemoteOrderKey* remote_order_key);
    void OrderIdRemoteToLocal(const HsRemoteOrderKey& remote_order_key, std::string* local_order_key);
    std::mutex m_ordermap_mtx;
    int m_order_ref;
};
}