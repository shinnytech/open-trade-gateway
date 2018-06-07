/////////////////////////////////////////////////////////////////////////
///@file trade_base.cpp
///@brief	交易后台接口基类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trader_base.h"


namespace trader_dll
{

TraderBase::TraderBase(std::function<void(const std::string&)> callback)
    : m_callback(callback)
{
}

TraderBase::~TraderBase()
{
}

void TraderBase::Input(const std::string& json)
{
    {
        std::lock_guard<std::mutex> lck(m_mtx);
        m_in_queue.push(json);
    }
    m_cv.notify_all();
}

void TraderBase::Output(const std::string& json)
{
    //static FILE* f = NULL;
    //if (!f)
    //	f = fopen("c:\\tmp\\out.json", "wt");
    //fprintf(f, "%s\n", json.c_str());
    //fflush(f);
    if(m_running)
        m_callback(json);
}

#include <chrono>
using namespace std::chrono_literals;
void TraderBase::Run()
{
    OnInit();
    while (m_running) {
        std::unique_lock<std::mutex> lck(m_mtx);
        m_cv.wait_for(lck, 100ms, [=] {return (!m_in_queue.empty()); });
        m_processing_in_queue.swap(m_in_queue);
        lck.unlock();
        while (!m_processing_in_queue.empty()) {
            auto& f = m_processing_in_queue.front();
            ProcessInput(f);
            m_processing_in_queue.pop();
        }
        OnIdle();
    }
}

void TraderBase::Start(const ReqLogin& req_login)
{
    m_running = true;
    m_req_login = req_login;
    m_worker_thread = std::thread(std::bind(&TraderBase::Run, this));
}

void TraderBase::Release()
{
    m_running = false;
    m_worker_thread.join();
}

void TraderBase::OutputNotify(long notify_code, const std::string& notify_msg, const char* level, const char* type)
{
    char* notify_template = "{"\
                                  "\"aid\": \"rtn_data\","\
                                  "\"data\" : ["\
                                  "{"\
                                  "\"notify\":{"\
                                  "\"%d\": {"\
                                  "\"type\": \"%s\","\
                                  "\"level\": \"%s\","\
                                  "\"code\": %d,"\
                                  "\"content\" : \"%s\""\
                                  "}}}]}";
    char buf[1024];
    static int notify_req = 0;
    sprintf(buf, notify_template, notify_req++, level, type, notify_code, notify_msg.c_str());
    Output(buf);
}

}