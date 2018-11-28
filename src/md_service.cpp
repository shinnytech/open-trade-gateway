/////////////////////////////////////////////////////////////////////////
///@file md_service.cpp
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "md_service.h"

#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include "log.h"
#include "config.h"
#include "rapid_serialize.h"
#include "http.h"
#include "version.h"


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
    boost::asio::io_context* m_ioc;
    boost::asio::ip::tcp::resolver* m_resolver;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket>* m_ws_socket;
    boost::beast::multi_buffer m_input_buffer;
    boost::beast::multi_buffer m_output_buffer;
} md_context;

Instrument* GetInstrument(const std::string& symbol)
{
    auto it = md_context.m_data.quotes.find(symbol);
    if (it != md_context.m_data.quotes.end())
        return &(it->second);
    return NULL;
}

using namespace std::chrono;

const char* md_host = "openmd.shinnytech.com";
const char* md_port = "80";
const char* md_path = "/t/md/front/mobile";

void OnConnect(boost::system::error_code ec);
void OnHandshake(boost::system::error_code ec);
void DoRead();
void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);
void DoWrite();
void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);
void OnCloseConnection();
void OnMessage(const std::string &json_str);
void SendTextMsg(const std::string &msg);


void OnResolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) 
{
    if (ec){
        Log(LOG_WARNING, NULL, "md service resolve fail, ec=%s", ec.message().c_str());
        return;
    }
    // Make the connection on the IP address we get from a lookup
    boost::asio::async_connect(
        md_context.m_ws_socket->next_layer(),
        results.begin(),
        results.end(),
        std::bind(
            OnConnect,
            std::placeholders::_1));
}

void OnConnect(boost::system::error_code ec) 
{
    if (ec){
        Log(LOG_WARNING, NULL, "md session connect fail, ec=%s", ec.message().c_str());
        return;
    }
    // Perform the websocket handshake
    md_context.m_ws_socket->async_handshake(md_host, md_path,
        std::bind(
            OnHandshake,
            std::placeholders::_1));
}

void OnHandshake(boost::system::error_code ec)
{
    if (ec){
        Log(LOG_WARNING, NULL, "md session handshake fail, ec=%s", ec.message().c_str());
        return;
    }
    Log(LOG_INFO, NULL, "md service got connection");
    // con->append_header("Accept", "application/v1+json");
    // con->append_header("User-Agent", "OTG-" VERSION_STR);
    SendTextMsg(md_context.m_req_subscribe_quote);
    SendTextMsg(md_context.m_req_peek_message);
    DoRead();
}

void DoRead()
{
    md_context.m_ws_socket->async_read(
        md_context.m_input_buffer,
        std::bind(
            OnRead,
            std::placeholders::_1,
            std::placeholders::_2));
}

void OnRead(boost::system::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if (ec) {
        if (ec != boost::beast::websocket::error::closed){
            Log(LOG_WARNING, NULL, "md service read fail, ec=%s", ec.message().c_str());
        }
        OnCloseConnection();
        return;
    }
    OnMessage(boost::beast::buffers_to_string(md_context.m_input_buffer.data()));
    md_context.m_input_buffer.consume(md_context.m_input_buffer.size());
    // Do another read
    DoRead();
}

void OnMessage(const std::string &json_str)
{
    Log(LOG_INFO, NULL, "md service received message, len=%d", json_str.size());
    SendTextMsg(md_context.m_req_peek_message);
    MdParser ss;
    ss.FromString(json_str.c_str());
    rapidjson::Value* dt = rapidjson::Pointer("/data").Get(*(ss.m_doc));
    if (dt && dt->IsArray()) {
        for (auto& v : dt->GetArray())
            ss.ToVar(md_context.m_data, &v);
    }
}

void SendTextMsg(const std::string &msg)
{
    if(md_context.m_output_buffer.size() > 0){
        size_t n = boost::asio::buffer_copy(md_context.m_output_buffer.prepare(msg.size()), boost::asio::buffer(msg));
        md_context.m_output_buffer.commit(n);
    } else {
        size_t n = boost::asio::buffer_copy(md_context.m_output_buffer.prepare(msg.size()), boost::asio::buffer(msg));
        md_context.m_output_buffer.commit(n);
        DoWrite();
    }
}

void DoWrite()
{
    md_context.m_ws_socket->text(true);
    md_context.m_ws_socket->async_write(
        md_context.m_output_buffer.data(),
        std::bind(
            OnWrite,
            std::placeholders::_1,
            std::placeholders::_2));
}

void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if (ec){
        Log(LOG_WARNING, NULL, "md session send message fail");
    }
    md_context.m_output_buffer.consume(bytes_transferred);
    if(md_context.m_output_buffer.size() > 0)
        DoWrite();
}

void OnCloseConnection()
{
    Log(LOG_INFO, NULL, "md session loss connection");
}

bool LoadInsList()
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
    return true;
}

void Init(boost::asio::io_context& ioc)
{
    // Inform the io_context that we are about to fork. The io_context
    // cleans up any internal resources, such as threads, that may
    // interfere with forking.
    ioc.notify_fork(boost::asio::io_context::fork_prepare);

    if (fork() == 0)
    {
        // In child process

        // Inform the io_context that the fork is finished and that this
        // is the child process. The io_context uses this opportunity to
        // create any internal file descriptors that must be private to
        // the new process.
        ioc.notify_fork(boost::asio::io_context::fork_child);

        md_context.m_ioc = &ioc;
        md_context.m_ws_socket = new boost::beast::websocket::stream<boost::asio::ip::tcp::socket>(ioc);
        md_context.m_resolver = new boost::asio::ip::tcp::resolver(ioc);

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

        // Look up the domain name
        md_context.m_resolver->async_resolve(
            md_host,
            md_port,
            std::bind(
                OnResolve,
                std::placeholders::_1,
                std::placeholders::_2));
    }
    else
    {
        // In parent process
        // Inform the io_context that the fork is finished (or failed)
        // and that this is the parent process. The io_context uses this
        // opportunity to recreate any internal resources that were
        // cleaned up during preparation for the fork.
        ioc.notify_fork(boost::asio::io_context::fork_parent);
    }
}

void Stop()
{
}
}
