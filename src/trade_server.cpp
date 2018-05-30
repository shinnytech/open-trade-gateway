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

void TraderServer::SendJson(void* wsi, const std::string& utf8_msg)
{
}

int TraderServer::OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    std::string** pss = (std::string**)(user);
    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        *pss = new std::string;
        m_send_queue[wsi] = std::list<std::string>();
        break;
    case LWS_CALLBACK_CLOSED:
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        RemoveTrader(wsi);
        delete (*pss);
        break;
    case LWS_CALLBACK_SERVER_WRITEABLE: {
        std::lock_guard<std::mutex> lck(m_mtx);
        if (!m_send_queue[wsi].empty()) {
            std::string& p = m_send_queue[wsi].front();
            int length = p.size();
            char* buf = new char[length + LWS_PRE];
            memcpy(buf + LWS_PRE, p.data(), p.size());
            int c = lws_write(wsi, (unsigned char*)(&buf[LWS_PRE]), length, LWS_WRITE_TEXT);
            delete[] buf;
            m_send_queue[wsi].pop_front();
            if (!m_send_queue[wsi].empty()) {
                lws_callback_on_writable(wsi);
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
            OnNetworkInput(wsi, **pss);
            (*pss)->clear();
        }
        DASSERT((*pss)->length() < 8 * 1024 * 1024);
        break;
    }
    return 0;
}

class SerializerLogin
    : public RapidSerialize::Serializer<SerializerLogin>
{
public:
    using RapidSerialize::Serializer<SerializerLogin>::Serializer;

    void DefineStruct(ReqLogin& d)
    {
        AddItem(d.aid, "aid");
        AddItem(d.bid, "bid");
        AddItem(d.user_name, "user_name");
        AddItem(d.password, "password");
    }
};

void TraderServer::OnNetworkInput(struct lws* wsi, const std::string& json)
{
    SerializerLogin ss;
    if (!ss.FromString(json))
        return;
    ReqLogin req;
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
            m_trader_map[wsi] = new trader_dll::TraderCtp(std::bind(&TraderServer::OnTraderInput, this, wsi, std::placeholders::_1));
            m_trader_map[wsi]->Start(req);
        }
        return;
        //if (aid == "req_login_femas_open")
        //    m_trader_map[wsi] = new trader_dll::TraderFemasOpen();
        //if (aid == "req_login_hs")
        //    m_trader_map[wsi] = new trader_dll::TraderHs();
    }
    auto orign_trader = m_trader_map.find(wsi);
    if (orign_trader != m_trader_map.end())
        orign_trader->second->Input(json);
}

void TraderServer::RemoveTrader(struct lws* wsi)
{
    std::lock_guard<std::mutex> lck(m_mtx);
    auto it_trader = m_trader_map.find(wsi);
    if (it_trader != m_trader_map.end()){
        it_trader->second->Release();
        m_trader_map.erase(it_trader);
    }
}

void TraderServer::OnTraderInput(struct lws* wsi, const std::string& utf8_msg)
{
    if (utf8_msg.empty())
        return;
    std::lock_guard<std::mutex> lck(m_mtx);
    auto it = m_send_queue.find(wsi);
    if (it == m_send_queue.end())
        return;
    m_send_queue[wsi].push_back(utf8_msg);
    lws_callback_on_writable(wsi);
}

void TraderServer::Run()
{
    while (true) {
        lws_service(ws_context, 10);
        //lws_callback_on_writable_all_protocol(ws_context, &protocols[0]);
    }
    lws_context_destroy(ws_context);
}
