/////////////////////////////////////////////////////////////////////////
///@file md_service.cpp
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "md_service.h"

#include <libwebsockets.h>
#include <iostream>

#define ASIO_STANDALONE
#include "websocketpp/config/asio_no_tls_client.hpp"
#include "websocketpp/client.hpp"

#include "log.h"
#include "config.h"
#include "rapid_serialize.h"
#include "http.h"
#include "version.h"

typedef websocketpp::client<websocketpp::config::asio_client> client;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

namespace md_service{

const char* ins_file_url = "http://openmd.shinnytech.com/t/md/symbols/latest.json";

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

    //工作线程
    client m_ws_client;
    std::thread m_worker_thread;
} md_context;

Instrument* GetInstrument(const std::string& symbol)
{
    auto it = md_context.m_data.quotes.find(symbol);
    if (it != md_context.m_data.quotes.end())
        return &(it->second);
    return NULL;
}

using namespace std::chrono;

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

void SendTextMsg(websocketpp::connection_hdl hdl, const std::string& msg){
    websocketpp::lib::error_code ec;
    md_context.m_ws_client.send(hdl, msg, websocketpp::frame::opcode::value::TEXT, ec);
    if (ec) {
        Log(LOG_ERROR, NULL, "md service send message fail, ec=%s, message=%s", ec.message().c_str(), msg.c_str());
    }else{
        Log(LOG_INFO, NULL, "md service send message success, len=%d", msg.size());
    }
}

void OnMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
    Log(LOG_INFO, NULL, "md service received message, len=%d", msg->get_payload().size());
    SendTextMsg(hdl, md_context.m_req_peek_message);
    OnWsMdData(msg->get_payload().c_str());
}

void OnOpenConnection(websocketpp::connection_hdl hdl)
{
    Log(LOG_INFO, NULL, "md service got connection, session=%p", hdl);
    SendTextMsg(hdl, md_context.m_req_subscribe_quote);
    SendTextMsg(hdl, md_context.m_req_peek_message);
}

void OnFailConnection(websocketpp::connection_hdl hdl)
{
    Log(LOG_ERROR, NULL, "md service connection error, session=%p", hdl);
}

void OnCloseConnection(websocketpp::connection_hdl hdl)
{
    Log(LOG_ERROR, NULL, "md service connection closed, session=%p", hdl);
}


void Run()
{
    std::string uri = "ws://openmd.shinnytech.com/t/md/front/mobile";
    try {
        // Set logging to be pretty verbose (everything except message payloads)
        md_context.m_ws_client.clear_access_channels(websocketpp::log::alevel::all);
        md_context.m_ws_client.clear_access_channels(websocketpp::log::alevel::all);
        // Initialize ASIO
        md_context.m_ws_client.init_asio();
        // Register our message handler
        md_context.m_ws_client.set_open_handler(bind(&OnOpenConnection, ::_1));
        md_context.m_ws_client.set_fail_handler(bind(&OnFailConnection, ::_1));
        md_context.m_ws_client.set_close_handler(bind(&OnCloseConnection, ::_1));
        md_context.m_ws_client.set_message_handler(bind(&OnMessage, ::_1, ::_2));
    } catch (websocketpp::exception const & e) {
        Log(LOG_ERROR, NULL, "md service websocket exception, what=%s", e.what());
    }
    while(true){
        try {
            websocketpp::lib::error_code ec;
            client::connection_ptr con = md_context.m_ws_client.get_connection(uri, ec);
            con->append_header("Accept", "application/v1+json");
            con->append_header("User-Agent", "OTG-" VERSION_STR);
            if (ec) {
                Log(LOG_ERROR, NULL, "md service create connection fail, reason=%s", ec.message().c_str());
                sleep(10000);
                continue;
            }
            // Note that connect here only requests a connection. No network messages are
            // exchanged until the event loop starts running in the next line.
            md_context.m_ws_client.connect(con);

            // Start the ASIO io_service run loop
            // this will cause a single connection to be made to the server. c.run()
            // will exit when this connection is closed.
            md_context.m_ws_client.run();
        } catch (websocketpp::exception const & e) {
            Log(LOG_ERROR, NULL, "md service websocket exception, what=%s", e.what());
        }
    }
}

bool Init()
{
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
    md_context.m_worker_thread = std::thread(Run);
    return true;
}

void Stop()
{
}

void CleanUp()
{
}

}
