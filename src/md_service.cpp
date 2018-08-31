/////////////////////////////////////////////////////////////////////////
///@file md_service.cpp
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "md_service.h"

#include <libwebsockets.h>
#include "log.h"
#include "config.h"
#include "rapid_serialize.h"
#include "http.h"
#include "version.h"

namespace md_service{

const char* md_host = "openmd.shinnytech.com";
const char* md_path = "/t/md/front/mobile";
const char* ins_file_url = "https://openmd.shinnytech.com/t/md/symbols/latest.json";

Instrument::Instrument()
{
    product_class = kProductClassFutures;
    price_tick = NAN;
    last_price = NAN;
    pre_settlement = NAN;
    margin = 0.0;
    commission = 0.0;
    upper_limit = NAN;
    lower_limit = NAN;
    ask_price1 = NAN;
    bid_price1 = NAN;
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
        AddItem(d.exchange_id, ("exchange_id"));
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
        AddItem(d.price_tick, ("price_tick"));
        AddItem(d.margin, ("margin"));
        AddItem(d.commission, ("commission"));
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
        AddItem(data.pre_settlement, ("pre_settlement"));
        AddItem(data.upper_limit, ("upper_limit"));
        AddItem(data.lower_limit, ("lower_limit"));
        AddItem(data.ask_price1, ("ask_price1"));
        AddItem(data.bid_price1, ("bid_price1"));
    }

    void DefineStruct(MdData& d)
    {
        AddItem(d.ins_list, ("ins_list"));
        AddItem(d.quotes, ("quotes"));
        AddItem(d.mdhis_more_data, ("mdhis_more_data"));
    }
};

static struct MdServiceContext
{
    //合约及行情数据
    MdData m_data;

    //发送包管理
    std::string m_req_subscribe_quote;
    std::string m_req_peek_message;
    bool m_need_subscribe_quote;
    bool m_need_peek_message;

    //工作线程
    std::thread m_worker_thread;
    bool m_running_flag;

    //websocket client
    struct lws* m_ws_md;
    struct lws_context* m_ws_context;
    char* m_send_buf;
    char* m_recv_buf;
    int m_recv_len;
    long long m_rate_limit_connect;
} md_context;

static struct lws_extension exts[] = {
    {
        "permessage-deflate",
        lws_extension_callback_pm_deflate,
        "permessage-deflate; client_max_window_bits"
    },
    { NULL, NULL, NULL /* terminator */ }
};

void CleanUp()
{
    md_context.m_running_flag = false;
    md_context.m_worker_thread.join();
    if (md_context.m_send_buf)
        delete[] md_context.m_send_buf;
    if (md_context.m_recv_buf)
        delete[] md_context.m_recv_buf;
}

Instrument* GetInstrument(const std::string& symbol)
{
    auto it = md_context.m_data.quotes.find(symbol);
    if (it != md_context.m_data.quotes.end())
        return &(it->second);
    return NULL;
}

using namespace std::chrono;

int Ratelimit(long long* last, long long millsecs)
{
    long long d = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (d - (*last) < millsecs)
        return 0;
    *last = d;
    return 1;
}

void OnWsMdData(const char* in)
{
    MdParser ss;
    ss.FromString(in);
    rapidjson::Value* dt = rapidjson::Pointer("/data").Get(*(ss.m_doc));
    if (dt && dt->IsArray()) {
        for (auto& v : dt->GetArray())
            ss.ToVar(md_context.m_data, &v);
    }
}

static int OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason,
    void* user, void* in, size_t len)
{
    if (wsi != md_context.m_ws_md)
        return 0;
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:{
        Log(LOG_INFO, NULL, "md service got connection, session=%p", wsi);
        md_context.m_need_subscribe_quote = true;
        break;
    }
    case LWS_CALLBACK_CLOSED:{
        Log(LOG_ERROR, NULL, "md service connection closed, session=%p", wsi);
        md_context.m_ws_md = NULL;
        break;
    }
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:{
        Log(LOG_ERROR, NULL, "md service connection error, session=%p", wsi);
        md_context.m_ws_md = NULL;
        break;
    }
    case LWS_CALLBACK_WSI_DESTROY:{
        Log(LOG_ERROR, NULL, "md service connection destroy, session=%p", wsi);
        md_context.m_ws_md = NULL;
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE: {
        memcpy(md_context.m_recv_buf + md_context.m_recv_len, in, len);
        md_context.m_recv_len += len;
        int final = lws_is_final_fragment(wsi);
        if (final) {
            Log(LOG_INFO, NULL, "md service received package, length=%d", md_context.m_recv_len);
            *(md_context.m_recv_buf + md_context.m_recv_len) = '\0';
            assert(md_context.m_recv_len > 0);
            md_context.m_recv_len = 0;
            OnWsMdData(md_context.m_recv_buf);
            md_context.m_need_peek_message = true;
        }
        assert(md_context.m_recv_len < 8 * 1024 * 1024);
        lws_callback_on_writable(md_context.m_ws_md);
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE_PONG: {
        lws_callback_on_writable(md_context.m_ws_md);
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
        std::string* p = NULL;
        if (md_context.m_need_subscribe_quote) {
            p = &md_context.m_req_subscribe_quote;
            md_context.m_need_subscribe_quote = false;
        } else if (md_context.m_need_peek_message) {
            p = &md_context.m_req_peek_message;
            md_context.m_need_peek_message = false;
        }
        if (p) {
            int length = p->size();
            if (length > 0 && length < 524228) {
                Log(LOG_INFO, NULL, "md service send package, length=%d", p->size());
                memcpy(md_context.m_send_buf + LWS_PRE, p->c_str(), p->size());
                int c = lws_write(md_context.m_ws_md, (unsigned char*)(&md_context.m_send_buf[LWS_PRE]), p->size(), LWS_WRITE_TEXT);
            } else {
                Log(LOG_FATAL, NULL, "md service send pack size(%d) should less than 524228", length);
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

static struct lws_protocols protocols[] = {
    {
        "md",
        OnWsMessage,
        0,
        40000,
    },
    { NULL, NULL, 0, 0 } /* end */
};

void RunOnce()
{
    if (!md_context.m_ws_md && Ratelimit(&md_context.m_rate_limit_connect, 10000u)) {
        struct lws_client_connect_info conn_info;
        memset(&conn_info, 0, sizeof(conn_info));
        conn_info.address = md_host;
        conn_info.path = md_path;
        conn_info.context = md_context.m_ws_context;
        conn_info.port = 80;
        conn_info.host = "openmd.shinnytech.com";
        conn_info.origin = "openmd.shinnytech.com";
        conn_info.ietf_version_or_minus_one = -1;
        conn_info.client_exts = exts;
        conn_info.protocol = "md";
        md_context.m_ws_md = lws_client_connect_via_info(&conn_info);
    }
    lws_service(md_context.m_ws_context, 10);
}

void Run()
{
    while (md_context.m_running_flag)
        RunOnce();
}

bool WriteWholeFile(const char* filename, const char* content, long length)
{
    FILE* pf = fopen(filename, "wb");
    if (!pf){
        Log(LOG_ERROR, NULL, "open file (%s) for write fail", filename);
        return false;
    }
    if (fwrite(content, length, 1, pf) != length){
        Log(LOG_ERROR, NULL, "write (%d) bytes to file (%s) fail", length, filename);
        return false;
    }
    fclose(pf);
    return true;
}

bool Init()
{
    //初始化context
    md_context.m_need_subscribe_quote = false;
    md_context.m_need_peek_message = false;
    //下载和加载合约表文件
    std::string content;
    InsFileParser ss;
    if (HttpGet(ins_file_url, &content) != 0) {
        Log(LOG_FATAL, NULL, "md service download ins file fail");
        exit(-1);
    }
    if (!ss.FromString(content.c_str())) {
        Log(LOG_FATAL, NULL, "md service parse downloaded ins file fail");
        exit(-1);
    }
    ss.ToVar(md_context.m_data.quotes);
    //初始化websocket
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.extensions = exts;
    info.ka_time = 15;
    info.ka_interval = 3;
    info.ka_probes = 5;
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    md_context.m_ws_context = lws_create_context(&info);
    md_context.m_running_flag = false;
    md_context.m_send_buf = new char[LWS_PRE + 524228];
    md_context.m_recv_buf = new char[8 * 1024 * 1024];
    md_context.m_recv_len = 0;
    md_context.m_rate_limit_connect = 0;
    md_context.m_ws_md = NULL;
    //订阅全行情
    std::string ins_list;
    for (auto it = md_context.m_data.quotes.begin(); it != md_context.m_data.quotes.end(); ++it) {
        auto& symbol = it->first;
        auto& ins = it->second;
        if(!ins.expired && (ins.product_class == kProductClassFutures
            || ins.product_class == kProductClassOptions
            || ins.product_class == kProductClassFOption
            )){
            ins_list += symbol;
            ins_list += ",";
        }
    }
    md_context.m_req_peek_message = "{\"aid\":\"peek_message\"}";
    md_context.m_req_subscribe_quote = "{\"aid\": \"subscribe_quote\", \"ins_list\": \"" + ins_list + "\"}";
    md_context.m_need_subscribe_quote = true;
    md_context.m_running_flag = true;
    md_context.m_worker_thread = std::thread(Run);
    return true;
}
}
