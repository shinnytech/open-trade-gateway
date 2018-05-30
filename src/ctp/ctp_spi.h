/////////////////////////////////////////////////////////////////////////
///@file ctp_spi.h
///@brief	CTP回调接口实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once
#include "ctp/ThostFtdcTraderApi.h"
#include "ctp_define.h"


namespace trader_dll
{

struct CtpRtnDataPack;
class TraderCtp;


class CCtpSpiHandler
    : public CThostFtdcTraderSpi
{
public:
    CCtpSpiHandler(TraderCtp* p)
        : m_trader(p)
    {
    }

private:
    void OutputPack(CtpRtnDataPack& pack);
    TraderCtp* m_trader;
    std::map<std::string, CtpRtnTradeUnit> m_trade_units;
    std::map<std::string, int> m_order_volume;
    void ProcessUnitTrade(std::map<std::string, CtpRtnTradeUnit>* units, std::string unit_id, std::string symbol, long direction, long offset, int volume, double price);
    void ProcessOrder(std::map<std::string, CtpRtnTradeUnit>* units, std::string order_id, std::string symbol, long direction, long offset, int volume);
    void ProcessUnitOrder(std::map<std::string, CtpRtnTradeUnit>* units, std::string unit_id, std::string symbol, long direction, long offset, int volume_change);
    int GetOrderChangeVolume(std::string order_id, int current_volume);
    void ProcessTrade(std::map<std::string, CtpRtnTradeUnit>* units, std::string order_id, std::string symbol, long direction, long offset, int volume, double price);

public:
    ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
    virtual void OnFrontConnected();

    ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
    ///@param nReason 错误原因
    ///        0x1001 网络读失败
    ///        0x1002 网络写失败
    ///        0x2001 接收心跳超时
    ///        0x2002 发送心跳失败
    ///        0x2003 收到错误报文
    virtual void OnFrontDisconnected(int nReason);

    ///登录请求响应
    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报单录入请求响应
    virtual void OnRspOrderInsert(CThostFtdcInputOrderField* pInputOrder, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报单操作请求响应
    virtual void OnRspOrderAction(CThostFtdcInputOrderActionField* pInputOrderAction, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询投资者持仓响应
    virtual void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField* pInvestorPosition, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询资金账户响应
    virtual void OnRspQryTradingAccount(CThostFtdcTradingAccountField* pTradingAccount, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///错误应答
    virtual void OnRspError(CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报单通知
    virtual void OnRtnOrder(CThostFtdcOrderField* pOrder);

    ///成交通知
    virtual void OnRtnTrade(CThostFtdcTradeField* pTrade);
};

}