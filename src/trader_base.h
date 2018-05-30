/////////////////////////////////////////////////////////////////////////
///@file trade_base.h
///@brief	交易后台接口基类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once
#include <concurrent_queue.h>
#include <string>
#include <queue>
#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "config.h"

struct ReqLogin
{
    std::string aid;
    std::string bid;        //对应服务器brokers配置文件中的id号
    std::string user_name;  //用户输入的用户名
    std::string password;   //用户输入的密码
    BrokerConfig broker;    //用户可以强制输入broker信息
};

namespace trader_dll
{

class TraderBase
{
public:
    TraderBase(std::function<void(const std::string&)> callback);
    virtual ~TraderBase();
    void Start(const ReqLogin& req_login);
    void Release();
    void Input(const std::string& json);
    void OutputNotify(long notify_class_id, const std::string& ret_msg, const char* type = "MESSAGE");

protected:
    virtual void OnInit() {};
    virtual void OnIdle() {};
    virtual void ProcessInput(const std::string& msg) = 0;
    void Output(const std::string& json);
    void Run();

    std::queue<std::string> m_in_queue;
    std::queue<std::string> m_processing_in_queue;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::thread m_worker_thread;
    std::function<void(const std::string&)> m_callback;
    bool m_running;
    ReqLogin m_req_login;
};
}