/////////////////////////////////////////////////////////////////////////
///@file trade_server.h
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <libwebsockets.h>

namespace trader_dll{
class TraderBase;
}

class TraderServer
{
public:
    TraderServer();
    ~TraderServer();
    int RunOnce();
    void InitBrokerList();
    void CleanUp();
    int OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

private:
    //期货公司列表
    std::string m_broker_list_str;

    //websocket服务
    void OnNetworkConnected(struct lws* wsi);
    void OnNetworkInput(struct lws* wsi, const char* req_json);
    void SendJson(struct lws* wsi, const std::string& utf8_msg);
    void CallWrite(struct lws* wsi);
    struct lws_context* ws_context;
    std::mutex m_ws_writable_mutex;

    //trader实例表
    std::map<void*, trader_dll::TraderBase*> m_trader_map;
    std::set<trader_dll::TraderBase*> m_removing_trader_set;
    trader_dll::TraderBase* GetTrader(void* wsi);
    void RemoveTrader(struct lws* wsi);
};
