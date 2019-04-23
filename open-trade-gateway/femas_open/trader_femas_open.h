#pragma once
#include <vector>
#include <map>
#include "../trader_base.h"

class CUstpFtdcTraderApi;

namespace trader_dll
{
class CFemasOpenTraderSpiHandler;

class TraderFemasOpen
    : public TraderBase
{
public:
    TraderFemasOpen();
    virtual ~TraderFemasOpen();

private:
    friend class CFemasOpenTraderSpiHandler;

    void OnClientReqLogin(const std::string& json_str);
    void OnClientReqInsertOrder(const std::string& json_str);
    void OnClientReqCancelOrder(const std::string& json_str);
    void OnClientReqTransfer(const std::string& json_str);
    void OnClientReqQueryBankAccount(const std::string& json_str);
    void ReqLogin();
    void ReqQryAccount();
    void ReqQryPosition();
    void ReqQryOrder();
    void ReqQryTrade();
    void ReqEnablePush();
    void ReqQueryBank();
    void ReqQueryTransferLog();
    bool m_bTraderApiConnected;
    CFemasOpenTraderSpiHandler* m_pThostTraderSpiHandler;
    CUstpFtdcTraderApi* m_api;
    std::string m_broker_id;
    std::string m_user_id;
    std::string m_password;
    std::string m_product_info;
};
}