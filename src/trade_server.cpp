/////////////////////////////////////////////////////////////////////////
///@file trade_server.cpp
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trade_server.h"

#include "config.h"
#include "rapid_serialize.h"
#include "libwebsockets/libwebsockets.h"
#include "md_service.h"
#include "trader_base.h"
#include "ctp/trader_ctp.h"


static struct lws_extension exts[] = {
    { NULL, NULL, NULL /* terminator */ }
};

static int OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    void* context_user = lws_context_user(lws_get_context(wsi));
    TraderServer* ws = static_cast<TraderServer*>(context_user);
    if (ws)
        return ws->OnWsMessage(wsi, reason, user, in, len);
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "api",
        ::OnWsMessage,
        sizeof(std::string*),
        40000,
    },
    { NULL, NULL, 0, 0 } /* end */
};

TraderServer::TraderServer()
{
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = g_config.port;
    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;
    info.max_http_header_pool = 16;
    info.gid = -1;
    info.uid = -1;
    info.options |= LWS_SERVER_OPTION_VALIDATE_UTF8;
    info.timeout_secs = 5;
    info.protocols = protocols;
    info.extensions = exts;
    info.user = this;
    ws_context = lws_create_context(&info);
}

TraderServer::~TraderServer()
{
}

void TraderServer::SendJson(struct lws* wsi, const std::string& p)
{
    if (!p.empty()) {
        int length = p.size();
        char* buf = new char[length + LWS_PRE];
        memcpy(buf + LWS_PRE, p.data(), p.size());
        int c = lws_write(wsi, (unsigned char*)(&buf[LWS_PRE]), length, LWS_WRITE_TEXT);
        delete[] buf;
    }
}

int TraderServer::OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    std::string** pss = (std::string**)(user);
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        *pss = new std::string;
        OnNetworkConnected(wsi);
        break;
    case LWS_CALLBACK_CLOSED:
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        RemoveTrader(wsi);
        if(pss && *pss){
            delete (*pss);
            *pss = NULL;
        }
        break;
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        auto trader = GetTrader(wsi);
        if (!trader){
            SendJson(wsi, m_broker_list_str);
        } else {
            std::string p;
            if (trader->m_out_queue.try_pop_front(&p)) {
                SendJson(wsi, p);
                if (!trader->m_out_queue.empty()) {
                    lws_callback_on_writable(wsi);
                }
            }
        }
        break;
    }
    case LWS_CALLBACK_RECEIVE:
        int orign_len = (*pss)->size();
        (*pss)->resize(orign_len + len);
        memcpy((void*)((*pss)->data() + orign_len), in, len);
        int final = lws_is_final_fragment(wsi);
        if (final) {
            OnNetworkInput(wsi, (*pss)->c_str());
            (*pss)->clear();
        }
        assert((*pss)->length() < 8 * 1024 * 1024);
        break;
    }
    return 0;
}

void TraderServer::OnNetworkConnected(struct lws* wsi)
{
    lws_callback_on_writable(wsi);
}

void TraderServer::OnNetworkInput(struct lws* wsi, const char* json_str)
{
    trader_dll::SerializerTradeBase ss;
    if (!ss.FromString(json_str))
        return;
    trader_dll::ReqLogin req;
    ss.ToVar(req);
    if (req.aid == "req_login") {
        if (m_trader_map.find(wsi) != m_trader_map.end()) {
            RemoveTrader(wsi);
        }
        auto broker = g_config.brokers.find(req.bid);
        if (broker == g_config.brokers.end())
            return;
        req.broker = broker->second;
        if (broker->second.broker_type == "ctp") {
            m_trader_map[wsi] = new trader_dll::TraderCtp(std::bind(lws_callback_on_writable, wsi));
            m_trader_map[wsi]->Start(req);
        }
        return;
    }
    auto trader = GetTrader(wsi);
    if (trader)
        trader->m_in_queue.push_back(json_str);
}

void TraderServer::RemoveTrader(struct lws* wsi)
{
    auto trader = GetTrader(wsi);
    if(trader){
        m_trader_map.erase(wsi);
        m_removing_trader_set.insert(trader);
        trader->Stop();
    }
    for (auto it = m_removing_trader_set.begin(); it != m_removing_trader_set.end(); ) {
        if ((*it)->m_finished){
            (*it)->m_worker_thread.join();
            delete(*it);
            it = m_removing_trader_set.erase(it);
        }else{
            ++it;
        }
    }
}

trader_dll::TraderBase* TraderServer::GetTrader(void* wsi)
{
    auto it = m_trader_map.find(wsi);
    if (it != m_trader_map.end())
        return it->second;
    return NULL;
}

void TraderServer::InitBrokerList()
{
    trader_dll::SerializerTradeBase ss;
    rapidjson::Pointer("/aid").Set(*ss.m_doc, "rtn_brokers");
    long long n = 0LL;
    for (auto it = g_config.brokers.begin(); it != g_config.brokers.end(); ++it) {
        std::string bid = it->first;
        rapidjson::Pointer("/brokers/" + std::to_string(n)).Set(*ss.m_doc, bid);
        n++;
    }
    ss.ToString(&m_broker_list_str);
}

void TraderServer::Run()
{
    InitBrokerList();
    //加载合约文件
    if (!md_service::Init())
        return;
    //提供交易服务
    while (true) {
        lws_service(ws_context, 10);
    }
    //服务结束
    md_service::CleanUp();
    lws_context_destroy(ws_context);
}
