#pragma once
#include <thread>

struct lws;
struct lws_context;

const int kOptionClassCall = 1;
const int kOptionClassPut = 2;

const int kProductClassAll = 0xFFFFFFFF;
const int kProductClassFutures = 0x00000001;
const int kProductClassOptions = 0x00000002;
const int kProductClassCombination = 0x00000004;
const int kProductClassFOption = 0x00000008;
const int kProductClassFutureIndex = 0x00000010;
const int kProductClassFutureContinuous = 0x00000020;
const int kProductClassStock = 0x00000040;
const int kProductClassFuturePack = kProductClassFutures | kProductClassFutureIndex | kProductClassFutureContinuous;

const int kExchangeShfe = 0x00000001;
const int kExchangeCffex = 0x00000002;
const int kExchangeCzce = 0x00000004;
const int kExchangeDce = 0x00000008;
const int kExchangeIne = 0x00000010;
const int kExchangeKq = 0x00000020;
const int kExchangeUser = 0x10000000;
const int kExchangeAll = 0xFFFFFFFF;


struct Instrument {
    Instrument();
    std::string symbol;
    bool expired;
    std::string ins_id;
    long exchange_id;
    long product_class;
    long volume_multiple;
    double last_price;
    double pre_settlement_price;
};

struct MdData {
    MdData() { mdhis_more_data = false; }
    std::string ins_list;
    std::map<std::string, Instrument> quotes;
    bool mdhis_more_data;
};


/*
MdService 实例从 Diff (http://www.shinnytech.com/diff) 的服务器获取合约及行情信息
Example:
    创建并运行 MdService 实例:
        MdService md_service;
        md_service.Init();  //初始化 MdService 服务
        md_service.Cleanup();  //要求 md_service 停止运行

    md_service 初始化完成后, 可以提供合约及行情信息:
        Instrument* ins = md_service.GetInstrument("SHFE.cu1801");
        ins->volume_multiple; //合约乘数
        ins->last_price; //合约的最新价
*/
class MdService
{
public:
    MdService();
    ~MdService();

    //初始化 MdService 实例, 初始化失败则返回 false
    bool Init();
    //要求 MdService 实例停止运行
    void CleanUp();

    int OnWsMessage(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);

private:
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
    void Run();
    void RunOnce();
    void OnWsMdData(const char* in);

    //websocket client
    struct lws* m_ws_md;
    struct lws_context* m_ws_context;
    std::string m_ca_path;
    char* m_send_buf;
    char* m_recv_buf;
    int m_recv_len;
    int Ratelimit(unsigned int* last, unsigned int millsecs);
    unsigned int m_rate_limit_connect;
};
