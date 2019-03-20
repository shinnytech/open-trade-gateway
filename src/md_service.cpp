/////////////////////////////////////////////////////////////////////////
///@file md_service.cpp
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "md_service.h"

#include <iostream>
#include <functional>
#include <utility>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
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
        AddItem(d.expired, ("expired"));
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
};

typedef std::array<char, 64> InsMapKeyType;
typedef Instrument InsMapMappedType;
typedef std::pair<const InsMapKeyType, InsMapMappedType> InsMapValueType;
typedef boost::interprocess::allocator<InsMapValueType, boost::interprocess::managed_shared_memory::segment_manager> ShmemAllocator;
struct CharArrayComparer {
    bool operator()(const std::array<char, 64>& a, const std::array<char, 64>& b) const {
        return strncmp(a.data(), b.data(), 64) < 0;
    }
};
typedef boost::interprocess::map<InsMapKeyType, InsMapMappedType, CharArrayComparer, ShmemAllocator> InsMapType;

static struct MdServiceContext
{
    //合约及行情数据
    InsMapType* m_ins_map;

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
    std::array<char, 64> key = {};
    std::copy(symbol.begin(), symbol.end(), key.data());
    auto it = md_context.m_ins_map->find(key);
    if (it != md_context.m_ins_map->end())
        return &(it->second);
    return NULL;
}

using namespace std::chrono;

const char* md_host = "openmd.shinnytech.com";
const char* md_port = "80";
const char* md_path = "/t/md/front/mobile";


void OnResolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
void OnConnect(boost::system::error_code ec);
void OnHandshake(boost::system::error_code ec);
void DoRead();
void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);
void DoWrite();
void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);
void OnMessage(const std::string &json_str);
void SendTextMsg(const std::string &msg);


void DoResolve()
{
    md_context.m_resolver->async_resolve(
        md_host,
        md_port,
        std::bind(
            OnResolve,
            std::placeholders::_1,
            std::placeholders::_2));
}

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
    md_context.m_ws_socket->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::request_type& m)
        {
            m.insert(boost::beast::http::field::accept, "application/v1+json");
            m.insert(boost::beast::http::field::user_agent, "OTG-" VERSION_STR);
        }));
    md_context.m_ws_socket->async_handshake(md_host, md_path,
        boost::beast::bind_front_handler(OnHandshake)
    );
}

void OnHandshake(boost::system::error_code ec)
{
    if (ec){
        Log(LOG_WARNING, NULL, "md session handshake fail, ec=%s", ec.message().c_str());
        return;
    }
    Log(LOG_INFO, NULL, "md service got connection");
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
        Log(LOG_INFO, NULL, "md session loss connection");
        DoResolve();
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
    rapidjson::Value* dt = rapidjson::Pointer("/data/0/quotes").Get(*(ss.m_doc));
    if (!dt)
        return;
    for(auto& m: dt->GetObject()){
        std::array<char, 64> key = {};
        strncpy(key.data(), m.name.GetString(), 64);
        auto it = md_context.m_ins_map->find(key);
        if (it == md_context.m_ins_map->end())
            continue;
        ss.ToVar(it->second, &m.value);
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
    if (ec){
        Log(LOG_WARNING, NULL, "md session send message fail");
    }
    md_context.m_output_buffer.consume(bytes_transferred);
    if(md_context.m_output_buffer.size() > 0)
        DoWrite();
}

bool LoadInsList()
{
   boost::interprocess::shared_memory_object::remove("InsMapSharedMemory");
   boost::interprocess::managed_shared_memory* segment = new boost::interprocess::managed_shared_memory
      (boost::interprocess::create_only
      ,"InsMapSharedMemory" //segment name
      ,32 * 1024 * 1024);          //segment size in bytes

   //Initialize the shared memory STL-compatible allocator
//    ShmemAllocator alloc_inst (segment.get_segment_manager());
   ShmemAllocator* alloc_inst = new ShmemAllocator(segment->get_segment_manager());

   //Construct a shared memory map.
   //Note that the first parameter is the comparison function,
   //and the second one the allocator.
   //This the same signature as std::map's constructor taking an allocator
   md_context.m_ins_map = 
      segment->construct<InsMapType>("InsMap")      //object name
                                 (CharArrayComparer() //first  ctor parameter
                                 ,*alloc_inst);     //second ctor parameter

    //下载和加载合约表文件
    std::string content;
    if (HttpGet(ins_file_url, &content) != 0) {
        Log(LOG_FATAL, NULL, "md service download ins file fail");
        exit(-1);
    }
    InsFileParser ss;
    if (!ss.FromString(content.c_str())) {
        Log(LOG_FATAL, NULL, "md service parse downloaded ins file fail");
        exit(-1);
    }
    for(auto& m: ss.m_doc->GetObject()){
        std::array<char, 64> key = {};
        strncpy(key.data(), m.name.GetString(), 64);
        ss.ToVar((*md_context.m_ins_map)[key], &m.value);
    }
    return true;
}

bool Init(boost::asio::io_context& ioc)
{
    // Inform the io_context that we are about to fork. The io_context
    // cleans up any internal resources, such as threads, that may
    // interfere with forking.
    ioc.notify_fork(boost::asio::io_context::fork_prepare);

    if (fork() == 0)
    {
        // In child process
        std::string ins_list;
        for(auto it = md_context.m_ins_map->begin(); it != md_context.m_ins_map->end(); ++it){
            auto& key = it->first;
            Instrument& ins = it->second;
            if(!ins.expired && (ins.product_class == kProductClassFutures
                || ins.product_class == kProductClassOptions
                || ins.product_class == kProductClassFOption
                )){
                ins_list += std::string(key.data());
                ins_list += ",";
            }
        }
        md_context.m_req_peek_message = "{\"aid\":\"peek_message\"}";
        md_context.m_req_subscribe_quote = "{\"aid\": \"subscribe_quote\", \"ins_list\": \"" + ins_list + "\"}";

        // Inform the io_context that the fork is finished and that this
        // is the child process. The io_context uses this opportunity to
        // create any internal file descriptors that must be private to
        // the new process.
        ioc.notify_fork(boost::asio::io_context::fork_child);

        md_context.m_ioc = &ioc;
        md_context.m_ws_socket = new boost::beast::websocket::stream<boost::asio::ip::tcp::socket>(ioc);
        md_context.m_resolver = new boost::asio::ip::tcp::resolver(ioc);

        // Look up the domain name
        DoResolve();
        return true;
    }
    else
    {
        // In parent process
        // Inform the io_context that the fork is finished (or failed)
        // and that this is the parent process. The io_context uses this
        // opportunity to recreate any internal resources that were
        // cleaned up during preparation for the fork.
        ioc.notify_fork(boost::asio::io_context::fork_parent);
        return false;
    }
}

void Stop()
{
}
}
