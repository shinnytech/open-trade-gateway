#include "stdafx.h"
#include "trader_femas_open.h"

#include "utility.h"
#include "log.h"
#include <windows.h>
#include <process.h>
#include <direct.h>
#include "traderspi.h"
#include "tiny_serialize.h"

namespace trader_dll
{

struct ReqLoginFemasOpen {
    //req_login
    //服务器信息
    std::string broker_id;
    std::vector<std::string> trade_servers;
    std::vector<std::string> query_servers;
    //用户信息
    std::string user_id;
    std::string password;
    std::string product_info;
};
}

static void DefineStruct(CUstpFtdcAccountFieldReqField& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.BankID, _T("bank_id"));
    tl.AddItem(d.BankBrchID, _T("bank_brch_id"));
    tl.AddItem(d.BankAccount, _T("bank_account"));
    tl.AddItem(d.BankAccountPwd1, _T("bank_password"));
    tl.AddItem(d.CurrencyID, _T("currency"));
    tl.AddItem(d.AccountID, _T("future_account"));
    tl.AddItem(d.AccountPwd1, _T("future_account_password"));
}

static void DefineStruct(trader_dll::ReqLoginFemasOpen& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.broker_id, _T("broker_id"));
    tl.AddItem(d.trade_servers, _T("trade_servers"));
    tl.AddItem(d.query_servers, _T("query_servers"));
    tl.AddItem(d.user_id, _T("user_id"));
    tl.AddItem(d.password, _T("password"));
    tl.AddItem(d.product_info, _T("product_info"));
}

static void DefineStruct(CUstpFtdcInputOrderField& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.UserOrderLocalID, _T("order_id"));
    tl.AddItem(d.ExchangeID, _T("exchange_id"));
    tl.AddItem(d.InstrumentID, _T("instrument_id"));
    tl.AddItemEnum(d.Direction, _T("direction"), {
        { USTP_FTDC_D_Buy, _T("BUY") },
        { USTP_FTDC_D_Sell, _T("SELL") },
    });
    tl.AddItemEnum(d.OffsetFlag, _T("offset"), {
        { USTP_FTDC_OF_Open, _T("OPEN") },
        { USTP_FTDC_OF_Close, _T("CLOSE") },
        { USTP_FTDC_OF_CloseToday, _T("CLOSETODAY") },
    });
    tl.AddItem(d.LimitPrice, _T("limit_price"));
    tl.AddItem(d.Volume, _T("volume"));
    tl.AddItem(d.UserOrderLocalID, _T("order_id"));
    d.OrderPriceType = USTP_FTDC_OPT_LimitPrice;
    d.VolumeCondition = USTP_FTDC_VC_AV;
    d.ForceCloseReason = USTP_FTDC_FCR_NotForceClose;
    d.HedgeFlag = USTP_FTDC_CHF_Speculation;
    d.TimeCondition = USTP_FTDC_TC_GFD;
}

static void DefineStruct(CUstpFtdcOrderActionField& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.UserOrderLocalID, _T("order_id"));
    tl.AddItem(d.ExchangeID, _T("exchange_id"));
    tl.AddItem(d.UserOrderActionLocalID, _T("action_id"));
    d.ActionFlag = USTP_FTDC_AF_Delete;
    d.LimitPrice = 0;
    d.VolumeChange = 0;
}

static void DefineStruct(CUstpFtdcTransferFieldReqField& d, TinySerialize::SerializeBinder& tl)
{
    tl.AddItem(d.BankID, _T("bank_id"));
    tl.AddItem(d.BankBrchID, _T("bank_brch_id"));
    tl.AddItem(d.BankAccount, _T("bank_account"));
    tl.AddItem(d.BankAccountPwd1, _T("bank_password"));
    tl.AddItem(d.CurrencyID, _T("currency"));
    tl.AddItem(d.TradeAmount, _T("amount"));
    tl.AddItem(d.AccountID, _T("future_account"));
    tl.AddItem(d.AccountPwd1, _T("future_account_password"));
}

namespace trader_dll
{

TraderFemasOpen::TraderFemasOpen()
    : m_pThostTraderSpiHandler(NULL)
    , m_api(NULL)
{
    RegisterProcessor("req_login_femas_open", std::bind(&TraderFemasOpen::OnClientReqLogin, this, std::placeholders::_1));
    RegisterProcessor("insert_order", std::bind(&TraderFemasOpen::OnClientReqInsertOrder, this, std::placeholders::_1));
    RegisterProcessor("cancel_order", std::bind(&TraderFemasOpen::OnClientReqCancelOrder, this, std::placeholders::_1));
    RegisterProcessor("req_transfer", std::bind(&TraderFemasOpen::OnClientReqTransfer, this, std::placeholders::_1));
    RegisterProcessor("req_query_bank_account", std::bind(&TraderFemasOpen::OnClientReqQueryBankAccount, this, std::placeholders::_1));
}

TraderFemasOpen::~TraderFemasOpen(void)
{
    if (m_api)
        m_api->Release();
}

void TraderFemasOpen::OnClientReqLogin(const std::string& json_str)
{
    ReqLoginFemasOpen fs;
    if (!TinySerialize::JsonStringToVar(fs, json_str))
        return;
    std::string flow_file_name = GenerateUniqFileName();
    m_api = CUstpFtdcTraderApi::CreateFtdcTraderApi(flow_file_name.c_str());
    m_pThostTraderSpiHandler = new CFemasOpenTraderSpiHandler(this);
    m_api->RegisterSpi(m_pThostTraderSpiHandler);
    //m_api->OpenRequestLog("c:/tmp/req.log");
    //m_api->OpenResponseLog("c:/tmp/rsp.log");
    m_api->SetHeartbeatTimeout(60);
    m_broker_id = fs.broker_id;
    for (auto it = fs.trade_servers.begin(); it != fs.trade_servers.end(); ++it) {
        CThostApiHelperStringPtr sp(*it);
        m_api->RegisterFront(sp.GetPtr());
    }
    for (auto it = fs.query_servers.begin(); it != fs.query_servers.end(); ++it) {
        CThostApiHelperStringPtr sp(*it);
        m_api->RegisterQryFront(sp.GetPtr());
    }
    m_api->SubscribePrivateTopic(USTP_TERT_QUICK);
    m_api->SubscribePublicTopic(USTP_TERT_QUICK);
    m_user_id = fs.user_id;
    m_password = fs.password;
    m_product_info = fs.product_info;
    m_api->Init();
}

void TraderFemasOpen::ReqLogin()
{
    CUstpFtdcReqUserLoginField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.Password, m_password.c_str());
    strcpy_x(field.UserProductInfo, m_product_info.c_str());
    m_api->ReqUserLogin(&field, 33);
}

void TraderFemasOpen::OnClientReqInsertOrder(const std::string& json_str)
{
    CUstpFtdcInputOrderField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    if (TinySerialize::JsonStringToVar(field, json_str)) {
        m_api->ReqOrderInsert(&field, 0);
    }
}

void TraderFemasOpen::OnClientReqCancelOrder(const std::string& json_str)
{
    CUstpFtdcOrderActionField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    if (TinySerialize::JsonStringToVar(field, json_str)) {
        m_api->ReqOrderAction(&field, 0);
    }
}

void TraderFemasOpen::OnClientReqTransfer(const std::string& json_str)
{
    CUstpFtdcTransferFieldReqField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    if (TinySerialize::JsonStringToVar(field, json_str)) {
        strcpy_x(field.AccountID, m_user_id.c_str());
        if (field.TradeAmount > 0) {
            strcpy_x(field.TradeCode, "15");
            m_api->ReqFromBankToFutureByFuture(&field, 0);
        } else {
            strcpy_x(field.TradeCode, "16");
            field.TradeAmount = -field.TradeAmount;
            m_api->ReqFromFutureToBankByFuture(&field, 0);
        }
    }
    /////期货发起查询银行余额请求
    //virtual int ReqQueryBankAccountMoneyByFuture(CUstpFtdcAccountFieldReqField *pAccountFieldReq, int nRequestID) = 0;
}

void TraderFemasOpen::OnClientReqQueryBankAccount(const std::string& json_str)
{
    CUstpFtdcAccountFieldReqField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    if (TinySerialize::JsonStringToVar(field, json_str)) {
        m_api->ReqQueryBankAccountMoneyByFuture(&field, 0);
    }
}

void TraderFemasOpen::ReqQryAccount()
{
    CUstpFtdcQryInvestorAccountField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    m_api->ReqQryInvestorAccount(&field, 0);
}

void TraderFemasOpen::ReqQryPosition()
{
    CUstpFtdcQryInvestorPositionField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    m_api->ReqQryInvestorPosition(&field, 0);
}

void TraderFemasOpen::ReqQryOrder()
{
    CUstpFtdcQryOrderField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    m_api->ReqQryOrder(&field, 0);
}

void TraderFemasOpen::ReqQryTrade()
{
    CUstpFtdcQryTradeField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.InvestorID, m_user_id.c_str());
    m_api->ReqQryTrade(&field, 0);
}

void TraderFemasOpen::ReqEnablePush()
{
    CUstpFtdcEnableRtnMoneyPositoinChangeField field;
    memset(&field, 0, sizeof(field));
    field.Enable = 1;
    m_api->ReqQryEnableRtnMoneyPositoinChange(&field, 0);
}

void TraderFemasOpen::ReqQueryBank()
{
    CUstpFtdcAccountregisterReqField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.AccountID, m_user_id.c_str());
    m_api->ReqQueryAccountregister(&field, 0);
}

void TraderFemasOpen::ReqQueryTransferLog()
{
    ///请求查询转帐流水
    CUstpFtdcTransferSerialFieldReqField field;
    memset(&field, 0, sizeof(field));
    strcpy_x(field.BrokerID, m_broker_id.c_str());
    strcpy_x(field.UserID, m_user_id.c_str());
    strcpy_x(field.AccountID, m_user_id.c_str());
    strcpy_x(field.StartDay, "20170101");
    strcpy_x(field.EndDay, "20171231");
    m_api->ReqQueryTransferSeria(&field, 0);
}

}