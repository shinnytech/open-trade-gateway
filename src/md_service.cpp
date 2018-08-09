#include "stdafx.h"
#include "md_service.h"

#include <functional>
#include "version.h"
#include "libwebsockets/libwebsockets.h"
#include "config.h"
#include "http.h"
#include "rapid_serialize.h"

const char* md_host = "mdopen.shinnytech.com";
const char* md_path = "/t/md/front/mobile";
const char* ins_file_url = "https://openmd.shinnytech.com/t/md/symbols/latest.json";


Instrument::Instrument()
{
    product_class = kProductClassFutures;
    exchange_id = kExchangeUser;
    last_price = NAN;
    pre_settlement_price = NAN;
}


class InsFileParser
    : public RapidSerialize::Serializer<InsFileParser>
{
public:
    void DefineStruct(Instrument& d)
    {
        AddItem(d.symbol, ("instrument_id"));
        AddItem(d.expired, ("expired"));
        AddItem(d.ins_id, ("ins_id"));
        AddItemEnum(d.exchange_id, ("exchange_id"), {
            { kExchangeShfe, ("SHFE") },
            { kExchangeDce, ("DCE") },
            { kExchangeCzce, ("CZCE") },
            { kExchangeCffex, ("CFFEX") },
            { kExchangeIne, ("INE") },
            { kExchangeKq, ("KQ") },
            });
        AddItemEnum(d.product_class, ("class"), {
            { kProductClassFutures, ("FUTURE") },
            { kProductClassOptions, ("OPTION") },
            { kProductClassCombination, ("FUTURE_COMBINE") },
            { kProductClassFOption, ("FUTURE_OPTION") },
            { kProductClassFutureIndex, ("FUTURE_INDEX") },
            { kProductClassFutureContinuous, ("FUTURE_CONT") },
            { kProductClassStock, ("STOCK") },
            });
        AddItem(d.volume_multiple, ("volume_multiple"));
    }
};


class MdParser
    : public RapidSerialize::Serializer<MdParser>
{
public:
    using RapidSerialize::Serializer<MdParser>::Serializer;

    void DefineStruct(Instrument& data)
    {
        AddItem(data.last_price, ("last_price"));
        AddItem(data.pre_settlement_price, ("pre_settlement_price"));
    }

    void DefineStruct(MdData& d)
    {
        AddItem(d.ins_list, ("ins_list"));
        AddItem(d.quotes, ("quotes"));
        AddItem(d.mdhis_more_data, ("mdhis_more_data"));
    }
};


static struct lws_extension exts[] = {
    {
        "permessage-deflate",
        lws_extension_callback_pm_deflate,
        "permessage-deflate; client_max_window_bits"
    },
    { NULL, NULL, NULL /* terminator */ }
};

static int OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    MdService* ws = static_cast<MdService*>(user);
    if (ws)
        return ws->OnWsMessage(wsi, reason, user, in, len);
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "md",
        OnWsMessage,
        0,
        40000,
    },
    { NULL, NULL, 0, 0 } /* end */
};

MdService::MdService()
{
    m_need_subscribe_quote = false;
    m_need_peek_message = false;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.extensions = exts;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    m_ca_path = "default\\ca.cert";
    info.ssl_ca_filepath = m_ca_path.c_str();
    m_ws_context = lws_create_context(&info);
    m_running_flag = false;
    m_send_buf = new char[LWS_PRE + 524228];
    m_recv_buf = new char[8 * 1024 * 1024];
    m_recv_len = 0;
    m_rate_limit_connect = 0;
    m_ws_md = NULL;
}

MdService::~MdService()
{
    if (m_send_buf)
        delete[] m_send_buf;
    if (m_recv_buf)
        delete[] m_recv_buf;
}

void MdService::CleanUp()
{
    m_running_flag = false;
    m_worker_thread.join();
}

int MdService::Ratelimit(unsigned int* last, unsigned int millsecs)
{
    DWORD d = GetTickCount();
    if (d - (*last) < millsecs)
        return 0;
    *last = d;
    return 1;
}

int MdService::OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    if (wsi != m_ws_md)
        return 0;
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        LOG_INFO("md server connected");
        m_need_subscribe_quote = true;
        break;
    case LWS_CALLBACK_CLOSED:
        LOG_ERROR("md server connect loss");
        m_ws_md = NULL;
        break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        LOG_ERROR("md server connect error");
        m_ws_md = NULL;
        break;
    case LWS_CALLBACK_WSI_DESTROY:
        LOG_ERROR("md server connect destroy");
        m_ws_md = NULL;
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE: {
        memcpy(m_recv_buf + m_recv_len, in, len);
        m_recv_len += len;
        int final = lws_is_final_fragment(wsi);
        if (final) {
            *(m_recv_buf + m_recv_len) = '\0';
            DASSERT(m_recv_len > 0);
            m_recv_len = 0;
            OnWsMdData(m_recv_buf);
            m_need_peek_message = true;
        }
        DASSERT(m_recv_len < 8 * 1024 * 1024);
        lws_callback_on_writable(m_ws_md);
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE_PONG: {
        lws_callback_on_writable(m_ws_md);
        break;
    }
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        char** p = (char**)in;
        if (len < 100)
            return 1;
        *p += sprintf(*p, "Accept: application/v1+json\r\nUser-Agent: OTG-%s\r\n"
            , VERSION_STR
        );
        break;
    }
    case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
        break;
    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        std::string *p = nullptr;
        if (m_need_subscribe_quote) {
            p = &m_req_subscribe_quote;
            m_need_subscribe_quote = false;
        } else if (m_need_peek_message) {
            p = &m_req_peek_message;
            m_need_peek_message = false;
        }
        if (p) {
            int length = p->size();
            if (length > 0 && length < 32768) {
                memcpy(m_send_buf + LWS_PRE, p->c_str(), p->size());
                int c = lws_write(m_ws_md, (unsigned char*)(&m_send_buf[LWS_PRE]), p->size(), LWS_WRITE_TEXT);
            } else {
                LOG_ERROR("行情接口发送数据包过大, %d", length);
            }
            lws_callback_on_writable(wsi);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

void MdService::Run()
{
    while (m_running_flag)
        RunOnce();
}

void MdService::RunOnce()
{
    if (!m_ws_md && Ratelimit(&m_rate_limit_connect, 10000u)) {
        struct lws_client_connect_info conn_info;
        memset(&conn_info, 0, sizeof(conn_info));
        conn_info.userdata = this;
        conn_info.address = md_host;
        conn_info.path = md_path;
        conn_info.context = m_ws_context;
        conn_info.port = 443;
        conn_info.ssl_connection = LCCSCF_USE_SSL;
        conn_info.host = "mdopen.shinnytech.com";
        conn_info.origin = "mdopen.shinnytech.com";
        conn_info.ietf_version_or_minus_one = -1;
        conn_info.client_exts = exts;
        conn_info.protocol = "md";
        m_ws_md = lws_client_connect_via_info(&conn_info);
    }
    if (m_ws_md)
        lws_callback_on_writable(m_ws_md);
    lws_service(m_ws_context, 10);
}

void MdService::OnWsMdData(const char* in)
{
    MdParser ss;
    ss.FromString(in);
    rapidjson::Value* dt = rapidjson::Pointer("/data").Get(*(ss.m_doc));
    if (dt && dt->IsArray()) {
        for (auto& v : dt->GetArray())
            ss.ToVar(m_data, &v);
    }
}

bool WriteWholeFile(const char* filename, const char* content, long length)
{
    FILE* pf = fopen(filename, "wb");
    if (!pf)
        return false;
    fwrite(content, length, 1, pf);
    fclose(pf);
    return true;
}

bool MdService::Init()
{
    bool download_success = true;
    LOG_DEBUG("download ins file start");
    std::string content;
    bool load_success = false;
    InsFileParser ss;
    if (0 == HttpGet(ins_file_url, &content)) {
        LOG_INFO("download ins file fail");
        if (ss.FromString(content.c_str())) {
            LOG_DEBUG("parse ins file success");
            ss.ToVar(m_data.quotes);
            load_success = true;
            if (WriteWholeFile(g_config.ins_file_path.c_str(), content.c_str(), content.size())) {
                LOG_DEBUG("save ins file success");
            } else {
                LOG_WARNING("save ins file fail");
            }
        } else {
            LOG_WARNING("parse ins file fail");
        }
    } else {
        LOG_WARNING("download ins file fail");
    }
    if (!load_success) {
        if (!ss.FromFile(g_config.ins_file_path.c_str())) {
            ss.ToVar(m_data.quotes);
            LOG_WARNING("load local ins file fail");
            return false;
        }
    }
    std::string ins_list;
    for (auto it = m_data.quotes.begin(); it != m_data.quotes.end(); ++it) {
        auto& symbol = it->first;
        ins_list += symbol;
        ins_list += ",";
    }
    m_req_subscribe_quote = "{\"aid\": \"subscribe_quote\", \"ins_list\": \"" + ins_list + "\"}";
    m_need_subscribe_quote = true;
    m_running_flag = true;
    m_worker_thread = std::thread(std::bind(&MdService::Run, this));
    return true;
}
