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
    TraderCtp(std::function<void()> callback);
    virtual void ProcessInput(const char* json_str) override;

private:
    friend class CCtpSpiHandler;
    //框架函数
    virtual void OnInit() override;
    virtual void OnIdle() override;
    virtual void OnFinish() override;

    //用户请求处理
    SerializerCtp ss;
    void OnClientReqInsertOrder();
    void OnClientReqCancelOrder();
    void OnClientReqTransfer();

    //数据更新发送
    void OnClientPeekMessage();
    void SendUserData();
    bool m_peeking_message;
    bool m_something_changed;

    //登录相关
    void SendLoginRequest();
    void ReqConfirmSettlement();
    void ReqQryBank();
    void ReqQryAccountRegister();
    void SetSession(std::string trading_day, int front_id, int session_id, int order_ref);
    std::string m_broker_id;
    int m_session_id;
    int m_front_id;
    int m_order_ref;

    //委托单单号映射表管理
    std::map<LocalOrderKey, RemoteOrderKey> m_ordermap_local_remote;
    std::map<RemoteOrderKey, LocalOrderKey> m_ordermap_remote_local;
    std::string m_trading_day;
    std::mutex m_ordermap_mtx;
    std::string m_user_file_name;
    void OrderIdLocalToRemote(const LocalOrderKey& local_order_key, RemoteOrderKey* remote_order_key);
    void OrderIdRemoteToLocal(const RemoteOrderKey& remote_order_key, LocalOrderKey* local_order_key);
    void FindLocalOrderId(const std::string& exchange_id, const std::string& order_sys_id, LocalOrderKey* local_order_key);
    void LoadFromFile();
    void SaveToFile();

    //CTP API 实例
    bool m_bTraderApiConnected;
    CCtpSpiHandler* m_spi;
    CThostFtdcTraderApi* m_api;

    //查询请求
    int ReqQryAccount();
    int ReqQryPosition();
    int m_next_qry_dt;
    std::atomic_bool m_need_query_positions;
    std::atomic_bool m_need_query_account;
};
}