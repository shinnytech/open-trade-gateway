#include "stdafx.h"
#include "trader_hs.h"

#include "utility.h"
#include "log.h"
#include <windows.h>
#include <process.h>
#include <direct.h>
#include "tiny_serialize.h"
#include "FutuTrade.h"
#include "hs_callback.h"


namespace trader_dll
{

struct ReqLoginHs {
    //req_login
    std::string servers;
    std::string server_id; //指定要登录的服务器
    std::string user_id;
    std::string password;
    std::string dynamiccode;
    std::string productinfo;
    std::string authcode;
    double init_balance;
};

struct ActionInsertOrderHs {
    std::string local_key;
    std::string futu_exch_type;
    std::string futu_code;
    char entrust_bs;
    char futures_direction;
    int volume;
    std::string entrust_prop;
    float limit_price;
};

struct ActionCancelOrderHs {
    std::string local_key;
};

struct RtnDataOrder {
    RtnDataOrder()
    {
    }
    std::string local_key;
    std::string confirm_id;
    std::string entrust_refrence;
    std::string session_no;
    std::string futu_exch_type;
    std::string futu_code;
    char entrust_bs;
    char futures_direction;
    char hedge_type;
    std::string entrust_prop;
    double futu_entrust_price;
    int entrust_amount;	//报单手数
    int total_business_amount; //已成交手数
    int withdraw_amount; //撤单数量
    double curr_entrust_margin;	//
    char entrust_status;
    char time_condition;
    char volume_condition;
    std::string entrust_time;
    std::string error_message;
    char forcedrop_reason;
};

struct RtnDataTrade {
    RtnDataTrade()
    {
    }
    std::string local_key;
    std::string futu_exch_type;
    std::string futu_code;
    std::string entrust_no;
    std::string entrust_refrence;
    std::string session_no;
    std::string business_id;
    char entrust_bs;
    char futures_direction;
    long business_amount;
    double futu_business_price;
    std::string business_time;
    double business_fare;
};

struct RtnDataPosition {
    RtnDataPosition()
    {
    }
    std::string fund_account;
    std::string futu_exch_type;
    std::string futu_code;
    std::vector<std::string> fields_long;
    std::vector<std::string> fields_short;
};

struct RtnDataAccount {
    RtnDataAccount()
    {
    }
    std::string fund_account;
    char money_type;

    double balance;
    double available;
    double pre_balance;
    double static_balance;
    double float_profit;
    double risk_ratio;
    double using_money;
    double margin;
    double frozen_margin;
    double commission;
    double deposit;
    double withdraw;
    double preminum;
    double position_profit;
    double close_profit;
    double frozen_commission;
    double frozen_premium;
};

struct HsRtnDataUser {
    std::string user_id;
    std::map<std::string, RtnDataAccount> accounts;
    std::map<std::string, RtnDataPosition> positions;
    std::map<std::string, RtnDataOrder> orders;
    std::map<std::string, RtnDataTrade> trades;
};

struct HsRtnDataItem {
    std::map<std::string, HsRtnDataUser> trade;
};

struct HsRtnDataPack {
    HsRtnDataPack()
    {
        aid = "rtn_data";
        data.resize(1);
    }
    std::string aid;
    std::vector<HsRtnDataItem> data;
};
}

static void DefineStruct(trader_dll::ReqLoginHs& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.servers, _T("servers"));
    tl.AddItem(d.user_id, _T("user_id"));
    tl.AddItem(d.password, _T("password"));
}

const static int POSITION_FIELD_ID_fund_account = 0;//	C18	资产账户
const static int POSITION_FIELD_ID_futu_exch_type = 1;//	C4	交易类别
const static int POSITION_FIELD_ID_futu_code = 2;//	C30	合约代码
const static int POSITION_FIELD_ID_entrust_bs = 3;//	C1	买卖方向
const static int POSITION_FIELD_ID_hedge_type = 4;//	C1	投机 / 套保类型
const static int POSITION_FIELD_ID_begin_amount = 5;//	N10	期初数量
const static int POSITION_FIELD_ID_real_amount = 6;//	N10	持仓数量
const static int POSITION_FIELD_ID_real_current_amount = 7;//	N16.2	今总持仓
const static int POSITION_FIELD_ID_old_current_amount = 8;//	N10	老仓持仓数量
const static int POSITION_FIELD_ID_enable_amount = 9;//	N10	可用数量
const static int POSITION_FIELD_ID_real_enable_amount = 10;//	N10	当日开仓可用数量
const static int POSITION_FIELD_ID_hold_margin = 11;//	N16.2	持仓保证金
const static int POSITION_FIELD_ID_old_open_balance = 12;//	N16.2	老仓持仓成交额
const static int POSITION_FIELD_ID_real_open_balance = 13;//	N16.2	回报开仓金额
const static int POSITION_FIELD_ID_hold_income = 14;//	N16.2	期货盯市盈亏
const static int POSITION_FIELD_ID_average_hold_price = 15;//	N12.6	持仓均价
const static int POSITION_FIELD_ID_hold_income_float = 16;//	N16.2	持仓浮动盈亏
const static int POSITION_FIELD_ID_average_price = 17;//	N12.6	平均价

static void DefineStruct(trader_dll::ActionInsertOrderHs& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.local_key, _T("order_id"));
    tl.AddItemEnum(d.futu_exch_type, _T("exchange_id"), {
        { "F1", _T("CZCE") },
        { "F2", _T("DCE") },
        { "F3", _T("SHFE") },
        { "F4", _T("CFFEX") },
        { "F5", _T("INE") },
    });
    tl.AddItem(d.futu_code, _T("instrument_id"));
    tl.AddItemEnum(d.entrust_bs, _T("direction"), {
        { '1', _T("BUY") },
        { '2', _T("SELL") },
    });
    tl.AddItemEnum(d.futures_direction, _T("offset"), {
        { '1', _T("OPEN") },
        { '2', _T("CLOSE") },
        { '4', _T("CLOSETODAY") },
    });
    tl.AddItem(d.volume, _T("volume"));
    tl.AddItemEnum(d.entrust_prop, _T("price_type"), {
        //@todo:
        { "F0", _T("LIMIT") },
        { "F1", _T("MARKET") },
        //F0	限价单
        //F1	市价单
        //F2	止损定单
        //F3	止盈定单
        //F4	限价止损定单
        //F5	限价止盈定单
        //F6	止损
        //F7	组合定单
        //FA	跨期套利确认
        //FB	持仓套保确认
        //FC	请求询价
        //FD	期权权力行使
        //FE	期权权力放弃
        //FF	双边报价
        //FG	五档市价
        //FH	最优价
    });
    tl.AddItem(d.limit_price, _T("limit_price"));
}

static void DefineStruct(trader_dll::ActionCancelOrderHs& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.local_key, _T("order_id"));
}

static void DefineStruct(trader_dll::RtnDataOrder& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.local_key, _T("order_id"));
    tl.AddItemEnum(d.futu_exch_type, _T("exchange_id"), {
        { "F1", _T("CZCE") },
        { "F2", _T("DCE") },
        { "F3", _T("SHFE") },
        { "F4", _T("CFFEX") },
        { "F5", _T("INE") },
    });
    tl.AddItem(d.futu_code, _T("instrument_id"));
    tl.AddItem(d.confirm_id, _T("exchange_order_id"));
    tl.AddItemEnum(d.entrust_bs, _T("direction"), {
        { '1', _T("BUY") },
        { '2', _T("SELL") },
    });
    tl.AddItemEnum(d.futures_direction, _T("offset"), {
        { '1', _T("OPEN") },
        { '2', _T("CLOSE") },
        { '4', _T("CLOSETODAY") },
    });
    tl.AddItemEnum(d.hedge_type, _T("hedge_flag"), {
        { '0', _T("SPECULATION") },
        { '1', _T("HEDGE") },
        { '2', _T("ARBITRAGE") },
        { '3', _T("MARKETMAKER") },
        //250046	套保标识
        //0	投机
        //1	套保
        //2	套利
        //3	做市商
        //4	备兑
    });
    tl.AddItem(d.entrust_amount, _T("volume_orign"));
    int volume_left = d.entrust_amount - d.total_business_amount;
    tl.AddItem(volume_left, _T("volume_left"));
    //价格及其它属性
    std::string order_type = "TRADE";
    std::string order_price_type = "LIMIT";
    std::string trade_type = "TAKEPROFIT";
    if (!tl.is_save) {
        if (d.entrust_prop == "F0") {
        } else if (d.entrust_prop == "F1") {
            order_price_type = "ANY";
        } else if (d.entrust_prop == "F2") {
            order_price_type = "ANY";
        } else if (d.entrust_prop == "F3") {
            trade_type = "STOPLOSS";
            order_price_type = "ANY";
        } else if (d.entrust_prop == "F4") {
            trade_type = "STOPLOSS";
        } else if (d.entrust_prop == "FG") {
            order_price_type = "FIVELEVEL";
        } else if (d.entrust_prop == "FH") {
            order_price_type = "BEST";
        }
    }
    tl.AddItem(order_type, _T("order_type"));
    tl.AddItem(order_price_type, _T("price_type"));
    tl.AddItem(trade_type, _T("trade_type"));
    tl.AddItem(d.futu_entrust_price, _T("limit_price"));
    tl.AddItemEnum(d.entrust_status, _T("status"), {
        { '0', _T("ALIVE") },
        { '1', _T("ALIVE") },
        { '2', _T("ALIVE") },
        { '3', _T("ALIVE") },
        { '4', _T("ALIVE") },
        { '5', _T("FINISHED") },
        { '6', _T("FINISHED") },
        { '7', _T("ALIVE") },
        { '8', _T("FINISHED") },
        { '9', _T("FINISHED") },
        { 'D', _T("ALIVE") },
        //1203	委托状态
        //0	未报
        //1	待报
        //2	已报
        //3	已报待撤
        //4	部成待撤
        //5	部撤
        //6	已撤
        //7	部成
        //8	已成
        //9	废单
        //A	已报待改(港股)
        //B	预埋单撤单(港股)
        //C	正报
        //D	撤废
    });
    tl.AddItemEnum(d.time_condition, _T("time_condition"), {
        { '1', _T("IOC") },
        { '3', _T("GFD") },
        { '4', _T("GTC") },
        { '5', _T("GTD") },
        { '6', _T("GFA") },
        { '7', _T("GFS") },
        //250022	委托有效类型
        //1	即时生效
        //3	当日有效
        //4	取消前有效
        //5	指定日期前有效
        //6	集合竞价有效
        //7	本节有效
    });
    tl.AddItemEnum(d.volume_condition, _T("volume_condition"), {
        { '1', _T("ALL") },
        { '3', _T("ANY") },
        { '4', _T("MIN") },
        //250023	期货交易类型
        //1	全部成交
        //3	任何数量
        //4	最小数量
    });
    tl.AddItem(d.entrust_time, _T("insert_time"));
    tl.AddItem(d.error_message, _T("status_msg"));
    tl.AddItem(d.curr_entrust_margin, _T("frozen_margin"));
    tl.AddItemEnum(d.forcedrop_reason, _T("force_close"), {
        { '0', _T("NOT") },
        { '1', _T("LACK_DEPOSIT") },
        { '2', _T("CLIENT_POSITION_LIMIT") },
        { '3', _T("MEMBER_POSITION_LIMIT") },
        { '4', _T("POSITION_MULTIPLE") },
        { '5', _T("VIOLATION") },
        { '6', _T("OTHER") },
        //250066	强平原因
        //0	非强平
        //1	资金不足
        //2	客户超仓
        //3	会员超仓
        //4	持仓非整数
        //5	违规
        //6	其它
    });
}

static void DefineStruct(trader_dll::RtnDataTrade& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.local_key, _T("order_id"));
    tl.AddItem(d.futu_code, _T("instrument_id"));
    tl.AddItem(d.business_id, _T("exchange_trade_id"));
    tl.AddItemEnum(d.futu_exch_type, _T("exchange_id"), {
        { "F1", _T("CZCE") },
        { "F2", _T("DCE") },
        { "F3", _T("SHFE") },
        { "F4", _T("CFFEX") },
        { "F5", _T("INE") },
    });
    tl.AddItem(d.futu_code, _T("instrument_id"));
    tl.AddItemEnum(d.entrust_bs, _T("direction"), {
        { '1', _T("BUY") },
        { '2', _T("SELL") },
    });
    tl.AddItemEnum(d.futures_direction, _T("offset"), {
        { '1', _T("OPEN") },
        { '2', _T("CLOSE") },
        { '4', _T("CLOSETODAY") },
    });
    tl.AddItem(d.business_amount, _T("volume"));
    tl.AddItem(d.futu_business_price, _T("price"));
    tl.AddItem(d.business_time, _T("trade_date_time"));
    tl.AddItem(d.business_fare, _T("commission"));
}

static void DefineStruct(trader_dll::RtnDataPosition& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.fund_account, _T("user_id"));
    tl.AddItemEnum(d.futu_exch_type, _T("exchange_id"), {
        { "F1", _T("CZCE") },
        { "F2", _T("DCE") },
        { "F3", _T("SHFE") },
        { "F4", _T("CFFEX") },
        { "F5", _T("INE") },
    });
    tl.AddItem(d.futu_code, _T("instrument_id"));
    if (!d.fields_long.empty()) {
        int volume = atoi(d.fields_long[POSITION_FIELD_ID_real_amount].c_str());
        int volume_today = atoi(d.fields_long[POSITION_FIELD_ID_real_current_amount].c_str());
        int volume_his = atoi(d.fields_long[POSITION_FIELD_ID_old_current_amount].c_str());
        double open_price = atof(d.fields_long[POSITION_FIELD_ID_average_hold_price].c_str());
        double float_profit = atof(d.fields_long[POSITION_FIELD_ID_hold_income_float].c_str());
        double margin = atof(d.fields_long[POSITION_FIELD_ID_hold_margin].c_str());
        tl.AddItem(open_price, _T("open_price_long"));
        tl.AddItem(float_profit, _T("float_profit_long"));
        tl.AddItem(volume_today, _T("volume_long_today"));
        tl.AddItem(volume_his, _T("volume_long_his"));
        tl.AddItem(margin, _T("margin_long"));
        //tl.AddItem(frozen, _T("volume_long_frozen"));
        //tl.AddItem(frozen_today, _T("volume_long_frozen_today"));
        //tl.AddItem(d.f->PositionCost, _T("open_cost_long"));
    }
    if (!d.fields_short.empty()) {
        int volume = atoi(d.fields_short[POSITION_FIELD_ID_real_amount].c_str());
        int volume_today = atoi(d.fields_short[POSITION_FIELD_ID_real_current_amount].c_str());
        int volume_his = atoi(d.fields_short[POSITION_FIELD_ID_old_current_amount].c_str());
        double open_price = atof(d.fields_short[POSITION_FIELD_ID_average_hold_price].c_str());
        double float_profit = atof(d.fields_short[POSITION_FIELD_ID_hold_income_float].c_str());
        double margin = atof(d.fields_short[POSITION_FIELD_ID_hold_margin].c_str());
        tl.AddItem(open_price, _T("open_price_short"));
        tl.AddItem(float_profit, _T("float_profit_short"));
        tl.AddItem(volume_today, _T("volume_short_today"));
        tl.AddItem(volume_his, _T("volume_short_his"));
        tl.AddItem(margin, _T("margin_short"));
    }
}

static void DefineStruct(trader_dll::RtnDataAccount& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.fund_account, _T("user_id"));
    tl.AddItem(d.fund_account, _T("account_id"));
    tl.AddItemEnum(d.money_type, _T("currency"), {
        { '0', _T("CNY") },
        { '1', _T("USD") },
        { '2', _T("HKD") },
    });
    tl.AddItem(d.balance, _T("balance"));
    tl.AddItem(d.available, _T("available"));
    tl.AddItem(d.static_balance, _T("static_balance"));
    tl.AddItem(d.pre_balance, _T("pre_balance"));
    tl.AddItem(d.deposit, _T("deposit"));
    tl.AddItem(d.withdraw, _T("withdraw"));
    tl.AddItem(d.commission, _T("commission"));
    tl.AddItem(d.preminum, _T("preminum"));
    tl.AddItem(d.float_profit, _T("float_profit"));
    tl.AddItem(d.position_profit, _T("position_profit"));
    tl.AddItem(d.close_profit, _T("close_profit"));
    tl.AddItem(d.risk_ratio, _T("risk_ratio"));
    tl.AddItem(d.margin, _T("margin"));
    tl.AddItem(d.frozen_margin, _T("frozen_margin"));
    tl.AddItem(d.frozen_commission, _T("frozen_commission"));
    tl.AddItem(d.frozen_premium, _T("frozen_premium"));
}

static void DefineStruct(trader_dll::HsRtnDataUser& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.user_id, _T("user_id"));
    if (!d.accounts.empty())
        tl.AddItem(d.accounts, _T("accounts"));
    if (!d.positions.empty())
        tl.AddItem(d.positions, _T("positions"));
    if (!d.orders.empty())
        tl.AddItem(d.orders, _T("orders"));
    if (!d.trades.empty())
        tl.AddItem(d.trades, _T("trades"));
}

static void DefineStruct(trader_dll::HsRtnDataItem& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.trade, _T("trade"));
}

static void DefineStruct(trader_dll::HsRtnDataPack& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.data, _T("data"));
}

namespace trader_dll
{

class HsScopePacker
{
public:
    HsScopePacker()
    {
        p = NewPacker(2);
        p->AddRef();
        p->BeginPack();
    }
    void Finish()
    {
        p->EndPack();
    }
    ~HsScopePacker()
    {
        p->FreeMem(p->GetPackBuf());
        p->Release();
    }
    IF2Packer* p;
};

class HsScopeUnpacker
{
public:
    HsScopeUnpacker(const std::string& binary_str)
    {
        lpMsg = NewBizMessage();
        lpMsg->AddRef();
        lpMsg->SetBuff(binary_str.data(), binary_str.size());
        int iLen = 0;
        const void* lpBuffer = lpMsg->GetContent(iLen);
        p = NewUnPacker((void*)lpBuffer, iLen);
    }
    ~HsScopeUnpacker()
    {
        p->AddRef();
        p->Release();
    }
    template<class T>
    void Get(const char* field_name, T& field);
    template<>
    void Get(const char* field_name, std::string& field)
    {
        const char* v = p->GetStr(field_name);
        if (v)
            field = v;
    }
    template<>
    void Get(const char* field_name, int& field)
    {
        field = p->GetInt(field_name);
    }
    template<>
    void Get(const char* field_name, double& field)
    {
        field = p->GetDouble(field_name);
    }
    template<>
    void Get(const char* field_name, char& field)
    {
        field = p->GetChar(field_name);
    }
    IBizMessage* lpMsg;
    IF2UnPacker* p;
};

TraderHs::TraderHs()
{
    m_api = NULL;
    m_order_ref = 0;
    m_callback = new HsCallback;
    m_callback->m_trader = this;
    m_config = NewConfig();
    m_config->AddRef();
    RegisterProcessor("req_login_hs", std::bind(&TraderHs::OnClientReqLogin, this, std::placeholders::_1));
    RegisterProcessor("insert_order", std::bind(&TraderHs::OnClientReqInsertOrder, this, std::placeholders::_1));
    RegisterProcessor("cancel_order", std::bind(&TraderHs::OnClientReqCancelOrder, this, std::placeholders::_1));
    RegisterProcessor("on_connect", std::bind(&TraderHs::OnConnect, this, std::placeholders::_1));
    RegisterProcessor("331100", std::bind(&TraderHs::OnHsRspLogin, this, std::placeholders::_1));
    RegisterProcessor("338300", std::bind(&TraderHs::OnHsRspQryAccount, this, std::placeholders::_1));
    RegisterProcessor("338303", std::bind(&TraderHs::OnHsRspQryPosition, this, std::placeholders::_1));
    RegisterProcessor("338202", std::bind(&TraderHs::OnHsRspInsertOrder, this, std::placeholders::_1));
    RegisterProcessor("338217", std::bind(&TraderHs::OnHsRspCancelOrder, this, std::placeholders::_1));
    RegisterProcessor("620000", std::bind(&TraderHs::OnHsHeartbeat, this, std::placeholders::_1));
    RegisterProcessor("620001", std::bind(&TraderHs::OnHsRspSubscribe, this, std::placeholders::_1));
    RegisterProcessor("620003", std::bind(&TraderHs::OnHsPush, this, std::placeholders::_1));
}

TraderHs::~TraderHs(void)
{
}

void TraderHs::OnClientReqLogin(const std::string& json_str)
{
    ReqLoginHs fs;
    if (!TinySerialize::JsonStringToVar(fs, json_str))
        return;
    int r = m_config->Load("t2sdk.ini");
    if (r != 0){
        OutputNotify(1, _T("加载t2sdk.ini失败"));
        return;
    }
    int iRet = 0;
    if (m_api)
        m_api->Release();
    m_api = NewConnection(m_config);
    m_api->AddRef();
    if (0 != (iRet = m_api->Create2BizMsg(m_callback))) {
        const char* err_msg = m_api->GetErrorMsg(iRet);
        std::wstring err_unicode = Utf8ToWide(GBKToUTF8(err_msg).c_str());
        OutputNotify(iRet, _T("注册服务器失败,") + err_unicode);
        return;
    }
    if (0 != (iRet = m_api->Connect(5000))) {
        const char* err_msg = m_api->GetErrorMsg(iRet);
        std::wstring err_unicode = Utf8ToWide(GBKToUTF8(err_msg).c_str());
        OutputNotify(iRet, _T("连接服务器失败, ") + err_unicode);
        return;
    }
    m_user_id = fs.user_id;
    m_password = fs.password;
    HsScopePacker p;
    p.p->AddField("op_branch_no", 'I', 5);                 //操作分支机构
    p.p->AddField("op_entrust_way", 'C', 1);               //委托方式
    p.p->AddField("op_station", 'S', 255);                 //站点地址
    p.p->AddField("branch_no", 'I', 5);                    //分支机构
    p.p->AddField("input_content", 'C');                   //客户标志类别
    p.p->AddField("account_content", 'S', 30);             //输入内容
    p.p->AddField("content_type", 'S', 6);                 //银行号、市场类别
    p.p->AddField("password", 'S', 10);                    //密码
    p.p->AddField("password_type", 'C');                   //密码类型
    p.p->AddInt(0);
    p.p->AddChar('7');
    p.p->AddStr("IPMAC");
    p.p->AddInt(0);
    p.p->AddChar('1');
    p.p->AddStr(fs.user_id.c_str());
    p.p->AddStr("0");
    p.p->AddStr(fs.password.c_str());
    p.p->AddChar('2');
    p.Finish();
    SendPack(331100, p.p->GetPackBuf(), p.p->GetPackLen());
}

void TraderHs::ReqSubscribePush()
{
    HsScopePacker p;
    //加入字段名
    p.p->AddField("branch_no", 'I', 5);
    p.p->AddField("fund_account", 'S', 18);
    p.p->AddField("op_branch_no", 'I', 5);
    p.p->AddField("op_entrust_way", 'C', 1);
    p.p->AddField("op_station", 'S', 255);
    p.p->AddField("client_id", 'S', 18);
    p.p->AddField("password", 'S', 10);
    p.p->AddField("user_token", 'S', 40);
    p.p->AddField("issue_type", 'I', 8);
    p.p->AddField("noreport_flag", 'C', 1);
    p.p->AddField("position_str", 'S', 100);
    //加入对应的字段值
    p.p->AddInt(m_branch_no);
    p.p->AddStr(m_user_id.c_str());
    p.p->AddInt(m_op_branch_no);
    p.p->AddChar('7');
    p.p->AddStr("IPMAC");	//op_station
    p.p->AddStr(m_client_id.c_str());
    p.p->AddStr(m_password.c_str());
    p.p->AddStr(m_user_token.c_str());
    p.p->AddInt(33115);
    p.p->AddChar('1');
    p.p->AddStr("0");
    p.Finish();
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    lpBizMessage->SetFunction(620001);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    lpBizMessage->SetIssueType(33115);
    lpBizMessage->SetKeyInfo(p.p->GetPackBuf(), p.p->GetPackLen());
    m_api->SendBizMsg(lpBizMessage, 1);
    lpBizMessage->Release();
}

void TraderHs::OnClientReqInsertOrder(const std::string& json_str)
{
    ActionInsertOrderHs action_insert_order;
    if (!TinySerialize::JsonStringToVar(action_insert_order, json_str))
        return;
    HsRemoteOrderKey rkey;
    rkey.session_id = m_session_no;
    rkey.exchange_id = action_insert_order.futu_exch_type;
    rkey.instrument_id = action_insert_order.futu_code;
    OrderIdLocalToRemote(action_insert_order.local_key, &rkey);
    HsScopePacker p;
    ///加入字段名
    p.p->AddField("op_branch_no", 'I', 5);                  //操作分支机构
    p.p->AddField("op_entrust_way", 'C', 1);                //委托方式
    p.p->AddField("op_station", 'S', 255);                  //站点地址
    p.p->AddField("branch_no", 'I', 5);                     //分支机构
    p.p->AddField("client_id", 'S', 18);                    //客户编号
    p.p->AddField("fund_account", 'S', 18);                 //资产账户
    p.p->AddField("password", 'S', 10);	                    //密码
    p.p->AddField("user_token", 'S', 40);                   //user_token
    p.p->AddField("futu_exch_type", 'S', 4);                //交易类别
    p.p->AddField("futures_account", 'S', 12);              //交易编码
    p.p->AddField("futu_code", 'S', 30);                    //合约代码
    p.p->AddField("entrust_bs", 'C', 1);                    //买卖方向
    p.p->AddField("futures_direction", 'C', 1);             //开平仓方向
    p.p->AddField("hedge_type", 'C', 1);                    //投机/套保类型
    p.p->AddField("entrust_amount", 'I');                   //委托数量
    p.p->AddField("futu_entrust_price", 'F', 12, 6);        //委托价格
    p.p->AddField("entrust_prop", 'S', 3);                  //委托属性
    p.p->AddField("entrust_occasion", 'S', 32);	            //委托场景
    p.p->AddField("entrust_reference", 'S', 32);	        //委托引用
    p.p->AddInt(m_op_branch_no);
    p.p->AddChar('7');
    p.p->AddStr("IPMAC");
    p.p->AddInt(m_branch_no);
    p.p->AddStr(m_client_id.c_str());
    p.p->AddStr(m_user_id.c_str());
    p.p->AddStr(m_password.c_str());
    p.p->AddStr(m_user_token.c_str());
    p.p->AddStr(action_insert_order.futu_exch_type.c_str());
    p.p->AddStr("");
    p.p->AddStr(action_insert_order.futu_code.c_str());
    p.p->AddChar(action_insert_order.entrust_bs);
    p.p->AddChar(action_insert_order.futures_direction);
    p.p->AddChar('0');
    p.p->AddInt(action_insert_order.volume);
    p.p->AddDouble(action_insert_order.limit_price);
    p.p->AddStr(action_insert_order.entrust_prop.c_str());
    p.p->AddStr("");
    p.p->AddStr(rkey.order_ref.c_str());
    p.Finish();
    SendPack(338202, p.p->GetPackBuf(), p.p->GetPackLen());
}

void TraderHs::OnClientReqCancelOrder(const std::string& json_str)
{
    ActionCancelOrderHs action_cancel_order;
    if (!TinySerialize::JsonStringToVar(action_cancel_order, json_str))
        return;
    HsRemoteOrderKey rkey;
    OrderIdLocalToRemote(action_cancel_order.local_key, &rkey);
    HsScopePacker p;
    ///加入字段名
    p.p->AddField("op_branch_no", 'I', 5);                    //操作分支机构
    p.p->AddField("op_entrust_way", 'C', 1);                  //委托方式
    p.p->AddField("op_station", 'S', 255);                    //站点地址
    p.p->AddField("branch_no", 'I', 5);			              //分支机构
    p.p->AddField("client_id", 'S', 18);			          //客户编号
    p.p->AddField("fund_account", 'S', 18);		              //资产账户
    p.p->AddField("password", 'S', 10);						  //密码
    p.p->AddField("user_token", 'S', 40);                     //用户口令
    p.p->AddField("futu_exch_type", 'S', 4);                  //交易类别
    //p.p->AddField("entrust_no", 'I', 8);                      //委托编号
    //p.p->AddField("confirm_id", 'S', 20);                   //主场单号-非必输字段
    p.p->AddField("session_no", 'I', 8);                    //会话编号-非必输字段
    //p.p->AddField("entrust_occasion", 'S', 32);	          //委托场景-非必输字段
    p.p->AddField("entrust_reference", 'S', 32);	          //委托引用-非必输字段
    //加入对应的字段值
    p.p->AddInt(m_op_branch_no);
    p.p->AddChar('7');
    p.p->AddStr("IPMAC");
    p.p->AddInt(m_branch_no);
    p.p->AddStr(m_client_id.c_str());
    p.p->AddStr(m_user_id.c_str());
    p.p->AddStr(m_password.c_str());
    p.p->AddStr(m_user_token.c_str());
    p.p->AddStr(rkey.exchange_id.c_str());
    int session_no = atoi(rkey.session_id.c_str());
    p.p->AddInt(session_no);
    p.p->AddStr(rkey.order_ref.c_str());
    p.Finish();
    SendPack(338217, p.p->GetPackBuf(), p.p->GetPackLen());
}

void TraderHs::OnConnect(const std::string& binary_str)
{
    m_order_ref = 0;
    OutputNotify(0, _T("连接成功"));
}

void TraderHs::SendPack(int func_no, void* content, int len)
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    //设置包头
    lpBizMessage->SetFunction(func_no);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    //设置包内容
    lpBizMessage->SetContent(content, len);
    //把包发送出去
    m_api->SendBizMsg(lpBizMessage, 1);
    lpBizMessage->Release();
}

void TraderHs::OnHsRspLogin(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    p.Get("client_id", m_client_id);
    p.Get("user_token", m_user_token);
    p.Get("branch_no", m_branch_no);
    p.Get("op_branch_no", m_op_branch_no);
    p.Get("session_no", m_session_no);
    int error_no;
    std::string error_info;
    p.Get("error_no", error_no);
    p.Get("error_info", error_info);
    if (error_no == 0) {
        OutputNotify(0, _T("登录成功"));
        char json_str[1024];
        sprintf(json_str, ("{"\
                           "\"aid\": \"rtn_data\","\
                           "\"data\" : [{\"trade\":{\"%s\":{\"session\":{"\
                           "\"user_id\" : \"%s\","\
                           "\"session_id\" : \"%s\","\
                           "\"max_order_id\" : 0"
                           "}}}}]}"), m_user_id.c_str(), m_user_id.c_str(), m_session_no.c_str());
        Output(json_str);
        ReqSubscribePush();
        ReqQryPosition();
        ReqQryAccount();
    } else {
        OutputNotify(error_no, _T("登录失败, ") + GbkToWide(error_info.c_str()));
    }
}

void TraderHs::OnHsRspQryAccount(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    int error_no;
    p.Get("error_no", error_no);
    if (error_no != 0) {
        std::string error_info;
        p.Get("error_info", error_info);
        OutputNotify(error_no, _T("OnHsRspQryAccount, ") + GbkToWide(error_info.c_str()));
        return;
    }
    HsRtnDataPack pack;
    for (; !p.p->IsEOF(); p.p->Next()) {
        //branch_no	N5	分支机构
        //	fund_account	C18	资产账户
        //	money_type	C3	币种类别
        //	begin_balance	N16.2	期初余额
        //	current_balance	N16.2	当前余额
        //	begin_hold_margin	N16.2	上日持仓保证金
        //	enable_balance	N16.2	可用资金
        //	fetch_balance	N16.2	可取金额
        //	frozen_balance	N16.2	冻结资金
        //	unfrozen_balance	N16.2	解冻资金
        //	pre_entrust_balance	N16.2	当日开仓预冻结金额
        //	entrust_balance	N16.2	委托金额
        //	entrust_fare	N16.2	委托费用
        //	hold_income	N16.2	期货盯市盈亏
        //	hold_income_float	N16.2	持仓浮动盈亏
        //	begin_equity_balance	N16.2	期初客户权益
        //	equity_balance	N16.2	客户权益
        //	interest_balance	N16.2	利息发生额
        //	drop_income	N16.2	平仓盯市盈亏
        //	drop_income_float	N16.2	平仓浮动盈亏
        //	business_fare	N16.2	成交手续费金额
        //	hold_margin	N16.2	持仓保证金
        //	exch_hold_margin	N16.2	交易所持仓保证金
        //	client_risk_rate	N16.4	客户风险率
        //	exch_risk_rate	N16.4	交易所风险率
        //	out_premium	N16.2	支出权利金
        //	in_premium	N16.2	收取权利金
        //	dyna_market_value	N16.2	期权持仓动态市值
        //	market_balance	N16.2	市值权益
        //	in_impawn_balance	N16.2	质入金额
        //	pur_quota_sz	N16.2	深圳限购额度
        //	pur_quota_sh	N16.2	上海限购额度
        //	enable_bail_balance	N16.2	可用保证金
        //	in_balance	N16.2	转入金额
        //	out_balance	N16.2	转出金额
        //	futu_impawn_balance	N16.2	期货质押资金
        std::string fund_account;
        p.Get("fund_account", fund_account);
        std::string money_type;
        p.Get("money_type", money_type);
        std::string account_key = fund_account + "|" + money_type;
        RtnDataAccount& account = pack.data[0].trade[fund_account].accounts[account_key];
        account.fund_account = fund_account;
        account.money_type = money_type[0];
        p.Get("equity_balance", account.balance);
        p.Get("enable_balance", account.available);
        p.Get("hold_margin", account.margin);
        p.Get("curr_entrust_margin", account.frozen_margin);
        p.Get("business_fare", account.commission);
        p.Get("hold_income_float", account.float_profit);
        p.Get("begin_equity_balance", account.pre_balance);
        p.Get("in_balance", account.deposit);
        p.Get("out_balance", account.withdraw);
        p.Get("preminum", account.preminum);
        p.Get("hold_income", account.position_profit);
        p.Get("drop_income", account.close_profit);
        p.Get("client_risk_rate", account.risk_ratio);
        account.risk_ratio /= 100;
        p.Get("curr_entrust_fare", account.frozen_commission);
        p.Get("curr_entrust_premium", account.frozen_premium);
        p.Get("drop_income", account.close_profit);
        account.static_balance = account.balance - account.position_profit;
    }
    std::string json_str;
    TinySerialize::VarToJsonString(pack, json_str);
    Output(json_str);
}

void TraderHs::OnHsRspQryPosition(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    int error_no;
    p.Get("error_no", error_no);
    if (error_no != 0) {
        std::string error_info;
        p.Get("error_info", error_info);
        OutputNotify(error_no, _T("OnHsRspQryPosition, ") + GbkToWide(error_info.c_str()));
        return;
    }
    while (!p.p->IsEOF()) {
        char entrust_status;
        p.Get("entrust_status", entrust_status);
        std::string futu_code;
        p.Get("futu_code", futu_code);
        std::string entrust_reference;
        p.Get("entrust_reference", entrust_reference);
        int entrust_no;
        p.Get("entrust_no", entrust_no);
        p.p->Next();
    }
}

void TraderHs::OnHsRspInsertOrder(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    char entrust_status;
    p.Get("entrust_status", entrust_status);
    std::string futu_code;
    p.Get("futu_code", futu_code);
    std::string entrust_reference;
    p.Get("entrust_reference", entrust_reference);
    int entrust_no;
    p.Get("entrust_no", entrust_no);
    std::string error_info;
    p.Get("error_info", error_info);
    int error_no;
    p.Get("error_no", error_no);
    if (error_no != 0) {
        OutputNotify(error_no, _T("下单失败, ") + GbkToWide(error_info.c_str()));
    }
}

void TraderHs::OnHsRspCancelOrder(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    int error_no;
    p.Get("error_no", error_no);
    if (error_no != 0) {
        std::string error_info;
        p.Get("error_info", error_info);
        OutputNotify(error_no, _T("撤单失败, ") + GbkToWide(error_info.c_str()));
    }
}

void TraderHs::OnHsRspSubscribe(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    int error_no;
    p.Get("error_no", error_no);
    if (error_no == 0) {
        //pass
    } else {
        std::string error_info;
        p.Get("error_info", error_info);
        std::wstring ws = GbkToWide(error_info.c_str());
        OutputNotify(error_no, _T("OnHsRspSubscribe失败, ") + GbkToWide(error_info.c_str()));
    }
}

void TraderHs::OnHsHeartbeat(const std::string& binary_str)
{
    IBizMessage* lpMsg = NewBizMessage();
    lpMsg->AddRef();
    lpMsg->SetBuff(binary_str.data(), binary_str.size());
    if (lpMsg->GetPacketType() == REQUEST_PACKET) {
        //收到心跳包
        lpMsg->ChangeReq2AnsMessage();
        m_api->SendBizMsg(lpMsg, 1);
    }
}

std::vector<std::string> split_string(const std::string& str, char splitter)
{
    std::vector<std::string> strings;
    std::istringstream f(str);
    std::string s;
    while (std::getline(f, s, splitter)) {
        strings.push_back(s);
    }
    return strings;
}

HsRtnDataUser& GetUser(HsRtnDataPack& pack, std::string fund_account)
{
    HsRtnDataUser& user = pack.data[0].trade[fund_account];
    user.user_id = fund_account;
    return user;
}

RtnDataPosition& GetPosition(HsRtnDataPack& pack, std::string fund_account, std::string futu_exch_type, std::string futu_code)
{
    std::string symbol;
    if (futu_exch_type == "F1")
        symbol = "CZCE." + futu_code;
    if (futu_exch_type == "F2")
        symbol = "DCE." + futu_code;
    if (futu_exch_type == "F3")
        symbol = "SHFE." + futu_code;
    if (futu_exch_type == "F4")
        symbol = "CFFEX." + futu_code;
    if (futu_exch_type == "F5")
        symbol = "INE." + futu_code;
    RtnDataPosition& position = GetUser(pack, fund_account).positions[symbol];
    position.futu_exch_type = futu_exch_type;
    position.fund_account = fund_account;
    position.futu_code = futu_code;
    return position;
}

void TraderHs::OnHsPush(const std::string& binary_str)
{
    HsScopeUnpacker p(binary_str);
    int issue_type = p.lpMsg->GetIssueType();
    if (issue_type != 33115)
        return;
    HsRtnDataPack pack;
    /*
    主推内容包括：
    	common_info_return(公共信息回报)
    	entrust_return(委托回报)
    	trade_return(成交回报)
    	trading_account_return(资金回报)
    	position_return(持仓回报)
    	comb_position_return(组合持仓回报)
    	所有组成字段内容以QH字符串格式存放，中间以ASCII值1分割，其中委托回报根据委托回报类型，QH存放不同的回报内容；
    */
    //获取公共信息回报内容
    if (0 != p.p->SetCurrentDataset("common_info_return")) {
        while (!p.p->IsEOF()) {
            std::string common_info_return_str;
            p.Get("QH", common_info_return_str);
            //@todo: 根据common_info_return格式解QH字符串
            p.p->Next();
        }
    }
    //获取委托回报内容
    if (0 != p.p->SetCurrentDataset("entrust_return")) {
        std::string entrust_return_str;
        p.Get("QH", entrust_return_str);
        char entrust_return_type;
        entrust_return_type = p.p->GetChar("entrust_return_type");
        switch (entrust_return_type) {
            case 'O': {
                //根据order_return格式解QH字符串
                std::vector<std::string> fields = split_string(entrust_return_str, '\001');
                assert(fields.size() == 34);
                const static int ORDER_FIELD_ID_fund_account = 0; //	C18	资产账户
                const static int ORDER_FIELD_ID_confirm_id = 1; //	C20	主场单号
                const static int ORDER_FIELD_ID_entrust_no = 2; //	N10	委托编号
                const static int ORDER_FIELD_ID_session_no = 3; //	N8	会话编号
                const static int ORDER_FIELD_ID_entrust_reference = 4; //	C32	委托引用
                const static int ORDER_FIELD_ID_futu_exch_type = 5; //	C4	交易类别
                const static int ORDER_FIELD_ID_futu_code = 6; //	C30	合约代码
                const static int ORDER_FIELD_ID_entrust_bs = 7; //	C1	买卖方向
                const static int ORDER_FIELD_ID_futures_direction = 8; //	C1	开平仓方向
                const static int ORDER_FIELD_ID_hedge_type = 9; //	C1	投机 / 套保类型
                const static int ORDER_FIELD_ID_futu_entrust_price = 10; //	N12.6	委托价格
                const static int ORDER_FIELD_ID_entrust_amount = 11; //	N10	委托数量
                const static int ORDER_FIELD_ID_entrust_status = 12; //	C1	委托状态
                const static int ORDER_FIELD_ID_business_balance = 13; //	N16.2	成交金额
                const static int ORDER_FIELD_ID_total_business_amount = 14; //	N16.2	总成交数量
                const static int ORDER_FIELD_ID_withdraw_amount = 15; //	N10	撤单数量
                const static int ORDER_FIELD_ID_init_date = 16; //	N8	交易日期
                const static int ORDER_FIELD_ID_entrust_time = 17; //	N8	委托时间
                const static int ORDER_FIELD_ID_report_time = 18; //	N8	申报时间
                const static int ORDER_FIELD_ID_curr_entrust_margin = 19; //	N16.2	当前预冻结保证金
                const static int ORDER_FIELD_ID_curr_entrust_fare = 20; //	N16.2	当前预冻结总费用
                const static int ORDER_FIELD_ID_curr_entrust_premium = 21; //	N16.2	当前预冻结权利金
                const static int ORDER_FIELD_ID_arbit_code = 22; //	C30	套利合约号
                const static int ORDER_FIELD_ID_second_code = 23; //	C30	第二腿合约代码
                const static int ORDER_FIELD_ID_fex_min_volume = 24; //	N10	最小成交量
                const static int ORDER_FIELD_ID_spring_price = 25; //	N12.6	止损价格
                const static int ORDER_FIELD_ID_time_condition = 26; //	C1	有效期类型
                const static int ORDER_FIELD_ID_valid_date = 27; //	N8	有效日期
                const static int ORDER_FIELD_ID_volume_condition = 28; //	C1	成交量类型
                const static int ORDER_FIELD_ID_entrust_prop = 29; //	C3	委托属性
                const static int ORDER_FIELD_ID_weave_type = 30; //	C1	组合类型
                const static int ORDER_FIELD_ID_forcedrop_reason = 31; //	C1	强平原因
                const static int ORDER_FIELD_ID_error_message = 32; //	C255	提示信息
                const static int ORDER_FIELD_ID_entrust_occasion = 33; //	C32	委托场景
                std::string fund_account = fields[ORDER_FIELD_ID_fund_account];
                std::string futu_code = fields[6];
                std::string futu_exch_type = fields[5];
                std::string confirm_id = fields[ORDER_FIELD_ID_confirm_id];
                std::string session_no = fields[3];
                std::string entrust_refrence = fields[4];
                std::string order_key = session_no + "|" + entrust_refrence;
                if (session_no == "-1")
                    break;
                HsRemoteOrderKey remote_key;
                remote_key.exchange_id = futu_exch_type;
                remote_key.instrument_id = futu_code;
                remote_key.session_id = session_no;
                remote_key.order_ref = entrust_refrence;
                std::string local_key;
                OrderIdRemoteToLocal(remote_key, &local_key);
                HsRtnDataUser& user = GetUser(pack, fund_account);
                RtnDataOrder& order = user.orders[local_key];
                order.local_key = local_key;
                order.futu_exch_type = futu_exch_type;
                order.futu_code = futu_code;
                order.confirm_id = confirm_id;
                order.entrust_refrence = entrust_refrence;
                order.session_no = session_no;
                order.entrust_bs = fields[7][0];
                order.futures_direction = fields[8][0];
                order.hedge_type = fields[ORDER_FIELD_ID_hedge_type][0];
                order.futu_entrust_price = atof(fields[10].c_str());
                order.entrust_amount = atoi(fields[11].c_str());
                order.entrust_status = fields[12][0];
                order.total_business_amount = atoi(fields[14].c_str());
                order.withdraw_amount = atoi(fields[15].c_str());
                order.entrust_time = fields[17];
                order.time_condition = fields[ORDER_FIELD_ID_time_condition][0];
                order.volume_condition = fields[ORDER_FIELD_ID_volume_condition][0];
                order.entrust_prop = fields[29];
                order.error_message = fields[32];
                order.curr_entrust_margin = atoi(fields[19].c_str());
                order.forcedrop_reason = fields[31][0];
            }
            case 'E':
                //@todo: 根据exercise_return格式解QH字符串
                break;
        }
    }
    //获取成交回报
    if (0 != p.p->SetCurrentDataset("trade_return")) {
        while (!p.p->IsEOF()) {
            std::string trade_return_str;
            p.Get("QH", trade_return_str);
            std::vector<std::string> fields = split_string(trade_return_str, '\001');
            assert(fields.size() == 18);
            const static int TRADE_FIELD_ID_fund_account = 0;//	C18	资产账户
            const static int TRADE_FIELD_ID_business_id = 1;//	C32	成交编号
            const static int TRADE_FIELD_ID_confirm_id = 2;//	C20	主场单号
            const static int TRADE_FIELD_ID_entrust_no = 3;//	N10	委托编号
            const static int TRADE_FIELD_ID_session_no = 4;//	N8	会话编号
            const static int TRADE_FIELD_ID_entrust_reference = 5;//	C32	委托引用
            const static int TRADE_FIELD_ID_futu_exch_type = 6;//	C4	交易类别
            const static int TRADE_FIELD_ID_futu_code = 7;//	C30	合约代码
            const static int TRADE_FIELD_ID_entrust_bs = 8;//	C1	买卖方向
            const static int TRADE_FIELD_ID_futures_direction = 9;//	C1	开平仓方向
            const static int TRADE_FIELD_ID_hedge_type = 10;//	C1	投机 / 套保类型
            const static int TRADE_FIELD_ID_business_amount = 11;//	N10	成交数量
            const static int TRADE_FIELD_ID_futu_business_price = 12;//	N12.6	成交价格
            const static int TRADE_FIELD_ID_init_date = 13;//	N8	交易日期
            const static int TRADE_FIELD_ID_business_time = 14;//	N8	成交时间
            const static int TRADE_FIELD_ID_business_fare = 15;//	N16.2	成交手续费金额
            const static int TRADE_FIELD_ID_drop_income = 16;//	N16.2	平仓盯市盈亏
            const static int TRADE_FIELD_ID_drop_income_float = 17;//	N16.2	平仓浮动盈亏
            std::string fund_account = fields[TRADE_FIELD_ID_fund_account];
            std::string futu_exch_type = fields[TRADE_FIELD_ID_futu_exch_type];
            std::string futu_code = fields[TRADE_FIELD_ID_futu_code];
            std::string entrust_no = fields[TRADE_FIELD_ID_entrust_no];
            std::string session_no = fields[TRADE_FIELD_ID_session_no];
            std::string entrust_refrence = fields[TRADE_FIELD_ID_entrust_reference];
            std::string business_id = fields[TRADE_FIELD_ID_business_id];
            HsRemoteOrderKey remote_key;
            remote_key.exchange_id = futu_exch_type;
            remote_key.instrument_id = futu_code;
            remote_key.order_ref = entrust_refrence;
            remote_key.session_id = session_no;
            std::string local_key;
            OrderIdRemoteToLocal(remote_key, &local_key);
            std::string trade_key = local_key + "|" + business_id;
            RtnDataTrade& trade = GetUser(pack, fund_account).trades[trade_key];
            trade.local_key = local_key;
            trade.entrust_no = entrust_no;
            trade.futu_exch_type = futu_exch_type;
            trade.futu_code = futu_code;
            trade.entrust_refrence = entrust_refrence;
            trade.session_no = session_no;
            trade.business_id = business_id;
            trade.entrust_bs = fields[TRADE_FIELD_ID_entrust_bs][0]; // entrust_bs
            trade.futures_direction = fields[TRADE_FIELD_ID_futures_direction][0];
            trade.business_amount = atoi(fields[TRADE_FIELD_ID_business_amount].c_str());
            trade.futu_business_price = atof(fields[TRADE_FIELD_ID_futu_business_price].c_str());
            trade.business_time = fields[TRADE_FIELD_ID_business_time].c_str();
            trade.business_fare = atof(fields[TRADE_FIELD_ID_business_fare].c_str());
            p.p->Next();
        }
    }
    //获取持仓回报内容示例，持仓数据存在多行结果集的情况
    if (0 != p.p->SetCurrentDataset("position_return")) {
        while (!p.p->IsEOF()) {
            std::string position_return_str;
            p.Get("QH", position_return_str);
            std::vector<std::string> fields = split_string(position_return_str, '\001');
            assert(fields.size() == 18);
            std::string fund_account = fields[0];
            std::string futu_exch_type = fields[1];
            std::string futu_code = fields[2];
            RtnDataPosition& position = GetPosition(pack, fund_account, futu_exch_type, futu_code);
            if (fields[POSITION_FIELD_ID_entrust_bs] == "1") {
                position.fields_long = fields;
            } else {
                position.fields_short = fields;
            }
            p.p->Next();
        }
    }
    //获取资金回报内容
    if (0 != p.p->SetCurrentDataset("trading_account_return")) {
        while (!p.p->IsEOF()) {
            std::string trading_account_return_str;
            p.Get("QH", trading_account_return_str);
            std::vector<std::string> fields = split_string(trading_account_return_str, '\001');
            assert(fields.size() == 27);
            //trading_account_return	c2000	资金回报
            //QH		资金回报内容（所有组成字段以字符串格式存放，中间以ASCII值1分割）
            const static int ACCOUNT_FIELD_ID_fund_account = 0;//	C18	资产账户
            const static int ACCOUNT_FIELD_ID_begin_balance = 1;//	N16.2	期初余额
            const static int ACCOUNT_FIELD_ID_begin_hold_margin = 2;//	N16.2	上日持仓保证金
            const static int ACCOUNT_FIELD_ID_begin_equity_balance = 3;//	N16.2	期初客户权益
            const static int ACCOUNT_FIELD_ID_equity_balance = 4;//	N16.2	客户权益
            const static int ACCOUNT_FIELD_ID_dyna_market_value = 5;//	N16.2	期权持仓动态市值
            const static int ACCOUNT_FIELD_ID_market_balance = 6;//	N16.2	市值权益
            const static int ACCOUNT_FIELD_ID_enable_balance = 7;//	N16.2	可用资金
            const static int ACCOUNT_FIELD_ID_fetch_balance = 8;//	N16.2	可取金额
            const static int ACCOUNT_FIELD_ID_hold_margin = 9;//	N16.2	持仓保证金
            const static int ACCOUNT_FIELD_ID_curr_entrust_margin = 10;//	N16.2	当前预冻结保证金
            const static int ACCOUNT_FIELD_ID_client_risk_rate = 11;//	N16.4	客户风险率
            const static int ACCOUNT_FIELD_ID_premium = 12;//	N16.2	期权权利金收支
            const static int ACCOUNT_FIELD_ID_curr_entrust_premium = 13;//	N16.2	当前预冻结权利金
            const static int ACCOUNT_FIELD_ID_business_fare = 14;//	N16.2	成交手续费金额
            const static int ACCOUNT_FIELD_ID_curr_entrust_fare = 15;//	N16.2	当前预冻结总费用
            const static int ACCOUNT_FIELD_ID_drop_income_float = 16;//	N16.2	平仓浮动盈亏
            const static int ACCOUNT_FIELD_ID_hold_income_float = 17;//	N16.2	持仓浮动盈亏
            const static int ACCOUNT_FIELD_ID_drop_income = 18;//	N16.2	平仓盯市盈亏
            const static int ACCOUNT_FIELD_ID_hold_income = 19;//	N16.2	期货盯市盈亏
            const static int ACCOUNT_FIELD_ID_in_balance = 20;//	N16.2	转入金额
            const static int ACCOUNT_FIELD_ID_out_balance = 21;//	N16.2	转出金额
            const static int ACCOUNT_FIELD_ID_in_impawn_balance = 22;//	N16.2	质入金额
            const static int ACCOUNT_FIELD_ID_futu_impawn_balance = 23;//	N16.2	期货质押资金
            const static int ACCOUNT_FIELD_ID_frozen_balance = 24;//	N16.2	冻结资金
            const static int ACCOUNT_FIELD_ID_unfrozen_balance = 25;//	N16.2	解冻资金
            const static int ACCOUNT_FIELD_ID_money_type = 26;//	C3	币种类别
            std::string fund_account = fields[ACCOUNT_FIELD_ID_fund_account];
            std::string money_type = fields[26];
            std::string account_key = fund_account + "|" + money_type;
            RtnDataAccount& account = pack.data[0].trade[fund_account].accounts[account_key];
            account.fund_account = fund_account;
            account.money_type = money_type[0];
            account.balance = atof(fields[ACCOUNT_FIELD_ID_equity_balance].c_str());
            account.available = atof(fields[ACCOUNT_FIELD_ID_enable_balance].c_str());
            account.margin = atof(fields[ACCOUNT_FIELD_ID_hold_margin].c_str());
            account.frozen_margin = atof(fields[ACCOUNT_FIELD_ID_curr_entrust_margin].c_str());
            account.commission = atof(fields[ACCOUNT_FIELD_ID_business_fare].c_str());
            account.float_profit = atof(fields[ACCOUNT_FIELD_ID_hold_income_float].c_str());
            account.pre_balance = atof(fields[ACCOUNT_FIELD_ID_begin_equity_balance].c_str());
            account.deposit = atof(fields[ACCOUNT_FIELD_ID_in_balance].c_str());
            account.withdraw = atof(fields[ACCOUNT_FIELD_ID_out_balance].c_str());
            account.preminum = atof(fields[ACCOUNT_FIELD_ID_premium].c_str());
            account.position_profit = atof(fields[ACCOUNT_FIELD_ID_hold_income].c_str());
            account.close_profit = atof(fields[ACCOUNT_FIELD_ID_drop_income].c_str());
            account.risk_ratio = atof(fields[ACCOUNT_FIELD_ID_client_risk_rate].c_str()) / 100;
            account.frozen_commission = atof(fields[ACCOUNT_FIELD_ID_curr_entrust_fare].c_str());
            account.frozen_premium = atof(fields[ACCOUNT_FIELD_ID_curr_entrust_premium].c_str());
            account.static_balance = account.balance - account.position_profit;
            p.p->Next();
        }
    }
    std::string json_str;
    TinySerialize::VarToJsonString(pack, json_str);
    Output(json_str);
}

void TraderHs::ReqQryPosition()
{
    //338303期货持仓查询
    HsScopePacker p;
    ///加入字段名
    p.p->AddField("op_branch_no", 'I', 5);
    p.p->AddField("op_entrust_way", 'C', 1);
    p.p->AddField("op_station", 'S', 255);
    p.p->AddField("branch_no", 'I', 5);
    p.p->AddField("client_id", 'S', 18);
    p.p->AddField("fund_account", 'S', 18);
    p.p->AddField("password", 'S', 10);
    p.p->AddField("user_token", 'S', 40);
    //加入对应的字段值
    p.p->AddInt(m_op_branch_no);
    p.p->AddChar('7');
    p.p->AddStr("IPMAC");
    p.p->AddInt(m_branch_no);
    p.p->AddStr(m_client_id.c_str());
    p.p->AddStr(m_user_id.c_str());
    p.p->AddStr(m_password.c_str());
    p.p->AddStr(m_user_token.c_str());
    p.Finish();
    SendPack(338303, p.p->GetPackBuf(), p.p->GetPackLen());
}

void TraderHs::ReqQryAccount()
{
    //338300 期货客户资金查询
    HsScopePacker p;
    ///加入字段名
    p.p->AddField("op_branch_no", 'I', 5);
    p.p->AddField("op_entrust_way", 'C', 1);
    p.p->AddField("op_station", 'S', 255);
    p.p->AddField("branch_no", 'I', 5);
    p.p->AddField("client_id", 'S', 18);
    p.p->AddField("fund_account", 'S', 18);
    p.p->AddField("password", 'S', 10);
    p.p->AddField("password_type", 'C', 1);
    p.p->AddField("user_token", 'S', 40);
    //加入对应的字段值
    p.p->AddInt(m_op_branch_no);
    p.p->AddChar('7');
    p.p->AddStr("IPMAC");
    p.p->AddInt(m_branch_no);
    p.p->AddStr(m_client_id.c_str());
    p.p->AddStr(m_user_id.c_str());
    p.p->AddStr(m_password.c_str());
    p.p->AddChar('2');
    p.p->AddStr(m_user_token.c_str());
    p.Finish();
    SendPack(338300, p.p->GetPackBuf(), p.p->GetPackLen());
}

void TraderHs::OrderIdLocalToRemote(const std::string& local_order_key, HsRemoteOrderKey* remote_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    auto it = m_ordermap_local_remote.find(local_order_key);
    if (it == m_ordermap_local_remote.end()) {
        remote_order_key->session_id = m_session_no;
        remote_order_key->order_ref = std::to_string(m_order_ref++);
        m_ordermap_local_remote[local_order_key] = *remote_order_key;
        m_ordermap_remote_local[*remote_order_key] = local_order_key;
        it = m_ordermap_local_remote.find(local_order_key);
    } else {
        *remote_order_key = it->second;
    }
}

void TraderHs::OrderIdRemoteToLocal(const HsRemoteOrderKey& remote_order_key, std::string* local_order_key)
{
    std::unique_lock<std::mutex> lck(m_ordermap_mtx);
    auto it = m_ordermap_remote_local.find(remote_order_key);
    if (it == m_ordermap_remote_local.end()) {
        char buf[1024];
        sprintf(buf, "UNKNOWN.%s.%s", remote_order_key.session_id.c_str(), remote_order_key.order_ref.c_str());
        *local_order_key = buf;
        m_ordermap_local_remote[*local_order_key] = remote_order_key;
        m_ordermap_remote_local[remote_order_key] = *local_order_key;
    } else {
        *local_order_key = it->second;
    }
}

//void TraderHs::ReqQryTrade()
//{
//	//338302期货成交查询
//	HsScopePacker p;
//	///加入字段名
//	p.p->AddField("op_branch_no", 'I', 5);
//	p.p->AddField("op_entrust_way", 'C', 1);
//	p.p->AddField("op_station", 'S', 255);
//	p.p->AddField("branch_no", 'I', 5);
//	p.p->AddField("client_id", 'S', 18);
//	p.p->AddField("fund_account", 'S', 18);
//	p.p->AddField("password", 'S', 10);
//	p.p->AddField("password_type", 'C', 1);
//	p.p->AddField("user_token", 'S', 40);
//	p.p->AddField("futu_exch_type", 'S', 4);
//	//加入对应的字段值
//	p.p->AddInt(m_op_branch_no);
//	p.p->AddChar('7');
//	p.p->AddStr("IPMAC");
//	p.p->AddInt(m_branch_no);
//	p.p->AddStr(m_client_id.c_str());
//	p.p->AddStr(m_user_id.c_str());
//	p.p->AddStr(m_password.c_str());
//	p.p->AddChar('2');
//	p.p->AddStr(m_user_token.c_str());
//	p.p->AddStr("");
//	SendPack(338302, p.p->GetPackBuf(), p.p->GetPackLen());
//}

//int TraderHs::ReqQryOrder()
//{
//	//338301期货委托查询
//	HsScopePacker p;
//	///加入字段名
//	pPacker->AddField("op_branch_no", 'I', 5);
//	pPacker->AddField("op_entrust_way", 'C', 1);
//	pPacker->AddField("op_station", 'S', 255);
//	pPacker->AddField("branch_no", 'I', 5);
//	pPacker->AddField("client_id", 'S', 18);
//	pPacker->AddField("fund_account", 'S', 18);
//	pPacker->AddField("password", 'S', 10);
//	pPacker->AddField("user_token", 'S', 40);
//	pPacker->AddField("position_str", 'S');//定位串
//	pPacker->AddField("request_num", 'S');//请求行数
//
//	//加入对应的字段值
//	p.p->AddInt(m_op_branch_no);
//	p.p->AddChar('7');
//	p.p->AddStr("IPMAC");
//	p.p->AddInt(m_branch_no);
//	p.p->AddStr(m_client_id.c_str());
//	p.p->AddStr(m_user_id.c_str());
//	p.p->AddStr(m_password.c_str());
//	p.p->AddStr(m_user_token.c_str());
//	p.p->AddStr("");
//	p.p->AddStr("9999");
//	SendPack(338301, p.p->GetPackBuf(), p.p->GetPackLen());
//}

}
