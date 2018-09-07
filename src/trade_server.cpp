/////////////////////////////////////////////////////////////////////////
///@file trade_server.cpp
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trade_server.h"

#include <iostream>
#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "log.h"
#include "config.h"
#include "rapid_serialize.h"
#include "md_service.h"
#include "trader_base.h"
#include "ctp/trader_ctp.h"
#include "sim/trader_sim.h"

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
typedef server::message_ptr message_ptr;

namespace trade_server
{

struct TradeSession
{
    TradeSession(){
        m_trader_instance = NULL;
    }
    trader_dll::TraderBase* m_trader_instance;
};

struct TradeServerContext
{
    server m_trade_server;
    //期货公司列表
    std::string m_broker_list_str;
    //trader实例表
    std::map<websocketpp::connection_hdl, TradeSession, std::owner_less<websocketpp::connection_hdl>> m_trader_map;
    int m_next_sessionid;
    server m_server;
    std::set<trader_dll::TraderBase*> m_removing_trader_set;
} trade_server_context;


void DeleteTraderInstance(trader_dll::TraderBase* trader)
{
    trade_server_context.m_removing_trader_set.insert(trader);
    trader->Stop();
    for (auto it = trade_server_context.m_removing_trader_set.begin(); it != trade_server_context.m_removing_trader_set.end(); ) {
        if ((*it)->m_finished){
            (*it)->m_worker_thread.join();
            delete(*it);
            it = trade_server_context.m_removing_trader_set.erase(it);
        }else{
            ++it;
        }
    }
}

void InitBrokerList()
{
    trader_dll::SerializerTradeBase ss;
    rapidjson::Pointer("/aid").Set(*ss.m_doc, "rtn_brokers");
    long long n = 0LL;
    for (auto it = g_config.brokers.begin(); it != g_config.brokers.end(); ++it) {
        std::string bid = it->first;
        rapidjson::Pointer("/brokers/" + std::to_string(n)).Set(*ss.m_doc, bid);
        n++;
    }
    ss.ToString(&trade_server_context.m_broker_list_str);
}


void SendTextMsg(websocketpp::connection_hdl hdl, const std::string& msg){
    websocketpp::lib::error_code ec;
    trade_server_context.m_trade_server.send(hdl, msg, websocketpp::frame::opcode::value::TEXT, ec);
    if (ec) {
        Log(LOG_ERROR, NULL, "md service send message fail, ec=%s, message=%s", ec.message().c_str(), msg.c_str());
    }else{
        Log(LOG_INFO, NULL, "md service send message success, len=%d", msg.size());
    }
}

void OnOpenConnection(websocketpp::connection_hdl hdl)
{
    trade_server_context.m_trader_map[hdl] = TradeSession();
    SendTextMsg(hdl, trade_server_context.m_broker_list_str);
    Log(LOG_INFO, NULL, "ws server got connection, session=%p", hdl);
}

void OnCloseConnection(websocketpp::connection_hdl hdl)
{
    Log(LOG_INFO, NULL, "ws server loss connection, session=%p", hdl);
    TradeSession& session = trade_server_context.m_trader_map[hdl];
    if (session.m_trader_instance){
        DeleteTraderInstance(session.m_trader_instance);
        session.m_trader_instance = NULL;
    }
}

void OnMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
    if (msg->get_opcode() != websocketpp::frame::opcode::TEXT){
        Log(LOG_ERROR, NULL, "TraderServer OnMessage received wrong opcode, session=%p", hdl);
    }
    auto& json_str = msg->get_payload();
    trader_dll::SerializerTradeBase ss;
    if (!ss.FromString(json_str.c_str())){
        Log(LOG_WARNING, NULL, "ws server parse json(%s) fail, session=%p", json_str.c_str(), hdl);
        return;
    }
    Log(LOG_INFO, json_str.c_str(), "ws server received package, session=%p", hdl);
    trader_dll::ReqLogin req;
    ss.ToVar(req);
    TradeSession& session = trade_server_context.m_trader_map[hdl];
    if (req.aid == "req_login") {
        if (session.m_trader_instance){
            DeleteTraderInstance(session.m_trader_instance);
            session.m_trader_instance = NULL;
        }
        auto broker = g_config.brokers.find(req.bid);
        if (broker == g_config.brokers.end()){
            Log(LOG_WARNING, json_str.c_str(), "ws server req_login invalid bid=%s", req.bid.c_str());
            return;
        }
        req.broker = broker->second;
        if (broker->second.broker_type == "ctp") {
            trade_server_context.m_trader_map[hdl].m_trader_instance = new trader_dll::TraderCtp(std::bind(SendTextMsg, hdl, std::placeholders::_1));
            trade_server_context.m_trader_map[hdl].m_trader_instance->Start(req);
        } else if (broker->second.broker_type == "sim") {
            trade_server_context.m_trader_map[hdl].m_trader_instance = new trader_dll::TraderSim(std::bind(SendTextMsg, hdl, std::placeholders::_1));
            trade_server_context.m_trader_map[hdl].m_trader_instance->Start(req);
        } else {
            Log(LOG_ERROR, json_str.c_str(), "ws server req_login invalid broker_type=%s", broker->second.broker_type.c_str());
        }
        return;
    }
    if (session.m_trader_instance){
        session.m_trader_instance->m_in_queue.push_back(json_str);
    }
}

bool Init()
{
    InitBrokerList();
    return true;
}

void Run()
{
    try {
        // // Set logging settings
        // trade_server_context.m_trade_server.set_access_channels(websocketpp::log::alevel::all);
        // trade_server_context.m_trade_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize Asio
        trade_server_context.m_trade_server.init_asio();

        // Register our message handler
        trade_server_context.m_trade_server.set_message_handler(bind(OnMessage, ::_1, ::_2));
        trade_server_context.m_trade_server.set_open_handler(bind(&OnOpenConnection, ::_1));
        trade_server_context.m_trade_server.set_close_handler(bind(&OnCloseConnection, ::_1));
        trade_server_context.m_trade_server.set_max_message_size(4 * 1024 * 1024);

        // Listen on port 9002
        trade_server_context.m_trade_server.listen(g_config.port);

        // Start the server accept loop
        trade_server_context.m_trade_server.start_accept();

        // Start the ASIO io_service run loop
        trade_server_context.m_trade_server.run();
    } catch (websocketpp::exception const & e) {
        Log(LOG_ERROR, NULL, "trade server websocketpp exception, what=%s", e.what());
    } catch (...) {
        Log(LOG_ERROR, NULL, "trade server other exception");
    }    
}

void Stop()
{
    trade_server_context.m_trade_server.stop_listening();
    for(auto it = trade_server_context.m_trader_map.begin(); it != trade_server_context.m_trader_map.end(); ++it){
        auto hdl = it->first;
        websocketpp::lib::error_code ec;
        trade_server_context.m_trade_server.close(hdl, websocketpp::close::status::going_away, "", ec);
        if (ec) {
            Log(LOG_ERROR, NULL, "trade server websocketpp close exception, what=%s", ec.message().c_str());
        }        
    }
}

void CleanUp()
{
    for (auto it = trade_server_context.m_removing_trader_set.begin(); it != trade_server_context.m_removing_trader_set.end(); ) {
        (*it)->m_worker_thread.join();
    }
    // for (auto it = m_trader_map.begin(); it != m_trader_map.end(); ++it){
    //     auto trader = it->second;
    //     trader->Stop();
    // }
    // for (auto it = m_trader_map.begin(); it != m_trader_map.end(); ++it){
    //     auto trader = it->second;
    //     trader->m_worker_thread.join();
    //     assert(trader->m_finished);
    // }
}

}