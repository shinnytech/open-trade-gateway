/////////////////////////////////////////////////////////////////////////
///@file trader_ctp.h
///@brief	CTP接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once
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
    virtual void ProcessInput(const char* json_str) override;

private:
    friend class CCtpSpiHandler;
    //框架函数
    virtual void OnInit() override;
    virtual void OnIdle() override;
    virtual void OnFinish() override;
    virtual bool NeedReset() override;

    //用户请求处理
    void OnClientReqInsertOrder(CtpActionInsertOrder d);
    void OnClientReqCancelOrder(CtpActionCancelOrder d);
    void OnClientReqTransfer(CThostFtdcReqTransferField f);
    void OnClientReqChangePassword(CThostFtdcUserPasswordUpdateField f);
    CThostFtdcInputOrderField m_input_order;
    CThostFtdcOrderActionField m_action_order;

    //数据更新发送
    void OnClientPeekMessage();
    void SendUserData();
    std::atomic_bool m_peeking_message;
    std::atomic_bool m_something_changed;
    std::atomic_bool m_position_ready;

    //登录相关
    void SendLoginRequest();
    void ReqAuthenticate();
    void ReqQrySettlementInfoConfirm();
    void ReqQrySettlementInfo();
    void ReqConfirmSettlement();
    void ReqQryBank();
    void ReqQryAccountRegister();
    void SetSession(std::string trading_day, int front_id, int session_id, int max_order_ref);
    std::string m_broker_id;
    int m_session_id;
    int m_front_id;
    int m_order_ref;

    //委托单单号映射表管理
    std::map<LocalOrderKey, RemoteOrderKey> m_ordermap_local_remote;
    std::map<RemoteOrderKey, LocalOrderKey> m_ordermap_remote_local;
    std::string m_trading_day;
    std::mutex m_ordermap_mtx;
    bool OrderIdLocalToRemote(const LocalOrderKey& local_order_key, RemoteOrderKey* remote_order_key);
    void OrderIdRemoteToLocal(const RemoteOrderKey& remote_order_key, LocalOrderKey* local_order_key);
    void FindLocalOrderId(const std::string& exchange_id, const std::string& order_sys_id, LocalOrderKey* local_order_key);
    void LoadFromFile();
    void SaveToFile();

    //CTP API 实例
    CCtpSpiHandler* m_spi;
    CThostFtdcTraderApi* m_api;

    //查询请求
    int ReqQryAccount(int reqid);
    int ReqQryPosition(int reqid);
    long long m_next_qry_dt;
    long long m_next_send_dt;
    std::atomic_bool m_logined;
    std::atomic_int m_req_position_id;
    std::atomic_int m_rsp_position_id;
    std::atomic_int m_req_account_id;
    std::atomic_int m_rsp_account_id;
    std::atomic_bool m_need_query_bank;
    std::atomic_bool m_need_query_register;
    std::atomic_bool m_need_query_settlement;
    std::atomic_llong m_req_login_dt;

    //用户操作反馈
    std::set<std::string> m_insert_order_set; //还需要给用户下单反馈的单号集合
    std::set<std::string> m_cancel_order_set; //还需要给用户撤单反馈的单号集合
    std::mutex m_order_action_mtx; //m_insert_order_set 和 m_cancel_order_set 的访问控制
};
}