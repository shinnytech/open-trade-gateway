#pragma once
#include "femas_open/USTPFtdcTraderApi.h"

namespace trader_dll
{

struct FemasRtnDataPack;
class TraderFemasOpen;
class CFemasOpenTraderSpiHandler
    : public CUstpFtdcTraderSpi
{
public:
    CFemasOpenTraderSpiHandler(TraderFemasOpen* p)
        : m_trader(p)
    {
    }

    ///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
    virtual void OnFrontConnected();
    virtual void OnQryFrontConnected();
    ///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
    ///@param nReason 错误原因
    ///        0x1001 网络读失败
    ///        0x1002 网络写失败
    ///        0x2001 接收心跳超时
    ///        0x2002 发送心跳失败
    ///        0x2003 收到错误报文
    virtual void OnFrontDisconnected(int nReason);
    virtual void OnQryFrontDisconnected(int nReason);

    ///心跳超时警告。当长时间未收到报文时，该方法被调用。
    ///@param nTimeLapse 距离上次接收报文的时间
    virtual void OnHeartBeatWarning(int nTimeLapse);

    ///报文回调开始通知。当API收到一个报文后，首先调用本方法，然后是各数据域的回调，最后是报文回调结束通知。
    ///@param nTopicID 主题代码（如私有流、公共流、行情流等）
    ///@param nSequenceNo 报文序号
    virtual void OnPackageStart(int nTopicID, int nSequenceNo);

    ///报文回调结束通知。当API收到一个报文后，首先调用报文回调开始通知，然后是各数据域的回调，最后调用本方法。
    ///@param nTopicID 主题代码（如私有流、公共流、行情流等）
    ///@param nSequenceNo 报文序号
    virtual void OnPackageEnd(int nTopicID, int nSequenceNo);


    ///错误应答
    virtual void OnRspError(CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///风控前置系统用户登录应答
    virtual void OnRspUserLogin(CUstpFtdcRspUserLoginField* pRspUserLogin, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///用户密码修改应答
    virtual void OnRspUserPasswordUpdate(CUstpFtdcUserPasswordUpdateField* pUserPasswordUpdate, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求对账单确认响应
    virtual void OnRspNotifyConfirm(CUstpFtdcInputNotifyConfirmField* pInputNotifyConfirm, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询对账单确认响应
    virtual void OnRspQryNotifyConfirm(CUstpFtdcNotifyConfirmField* pNotifyConfirm, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报单录入应答
    virtual void OnRspOrderInsert(CUstpFtdcInputOrderField* pInputOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报单操作应答
    virtual void OnRspOrderAction(CUstpFtdcOrderActionField* pOrderAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报价录入应答
    virtual void OnRspQuoteInsert(CUstpFtdcInputQuoteField* pInputQuote, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报价操作应答
    virtual void OnRspQuoteAction(CUstpFtdcQuoteActionField* pQuoteAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///询价请求应答
    virtual void OnRspForQuote(CUstpFtdcReqForQuoteField* pReqForQuote, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///客户申请组合应答
    virtual void OnRspMarginCombAction(CUstpFtdcInputMarginCombActionField* pInputMarginCombAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///数据流回退通知
    virtual void OnRtnFlowMessageCancel(CUstpFtdcFlowMessageCancelField* pFlowMessageCancel);

    ///成交回报
    virtual void OnRtnTrade(CUstpFtdcTradeField* pTrade);

    ///报单回报
    virtual void OnRtnOrder(CUstpFtdcOrderField* pOrder);

    ///报单录入错误回报
    virtual void OnErrRtnOrderInsert(CUstpFtdcInputOrderField* pInputOrder, CUstpFtdcRspInfoField* pRspInfo);

    ///报单操作错误回报
    virtual void OnErrRtnOrderAction(CUstpFtdcOrderActionField* pOrderAction, CUstpFtdcRspInfoField* pRspInfo);

    ///合约交易状态通知
    virtual void OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField* pInstrumentStatus);

    ///报价回报
    virtual void OnRtnQuote(CUstpFtdcRtnQuoteField* pRtnQuote);

    ///报价录入错误回报
    virtual void OnErrRtnQuoteInsert(CUstpFtdcInputQuoteField* pInputQuote, CUstpFtdcRspInfoField* pRspInfo);

    ///报价撤单错误回报
    virtual void OnErrRtnQuoteAction(CUstpFtdcOrderActionField* pOrderAction, CUstpFtdcRspInfoField* pRspInfo);

    ///询价回报
    virtual void OnRtnForQuote(CUstpFtdcReqForQuoteField* pReqForQuote);

    ///增加组合规则通知
    virtual void OnRtnMarginCombinationLeg(CUstpFtdcMarginCombinationLegField* pMarginCombinationLeg);

    ///客户申请组合确认
    virtual void OnRtnMarginCombAction(CUstpFtdcInputMarginCombActionField* pInputMarginCombAction);

    ///查询前置系统用户登录应答
    virtual void OnRspQueryUserLogin(CUstpFtdcRspUserLoginField* pRspUserLogin, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///报单查询应答
    virtual void OnRspQryOrder(CUstpFtdcOrderField* pOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///成交单查询应答
    virtual void OnRspQryTrade(CUstpFtdcTradeField* pTrade, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///可用投资者账户查询应答
    virtual void OnRspQryUserInvestor(CUstpFtdcRspUserInvestorField* pRspUserInvestor, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///交易编码查询应答
    virtual void OnRspQryTradingCode(CUstpFtdcRspTradingCodeField* pRspTradingCode, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///投资者资金账户查询应答
    virtual void OnRspQryInvestorAccount(CUstpFtdcRspInvestorAccountField* pRspInvestorAccount, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///合约查询应答
    virtual void OnRspQryInstrument(CUstpFtdcRspInstrumentField* pRspInstrument, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///交易所查询应答
    virtual void OnRspQryExchange(CUstpFtdcRspExchangeField* pRspExchange, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///投资者持仓查询应答
    virtual void OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField* pRspInvestorPosition, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///订阅主题应答
    virtual void OnRspSubscribeTopic(CUstpFtdcDisseminationField* pDissemination, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///合规参数查询应答
    virtual void OnRspQryComplianceParam(CUstpFtdcRspComplianceParamField* pRspComplianceParam, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///主题查询应答
    virtual void OnRspQryTopic(CUstpFtdcDisseminationField* pDissemination, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///投资者手续费率查询应答
    virtual void OnRspQryInvestorFee(CUstpFtdcInvestorFeeField* pInvestorFee, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///投资者保证金率查询应答
    virtual void OnRspQryInvestorMargin(CUstpFtdcInvestorMarginField* pInvestorMargin, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///交易编码组合持仓查询应答
    virtual void OnRspQryInvestorCombPosition(CUstpFtdcRspInvestorCombPositionField* pRspInvestorCombPosition, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///交易编码单腿持仓查询应答
    virtual void OnRspQryInvestorLegPosition(CUstpFtdcRspInvestorLegPositionField* pRspInvestorLegPosition, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///合约组信息查询应答
    virtual void OnRspQryInstrumentGroup(CUstpFtdcRspInstrumentGroupField* pRspInstrumentGroup, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///组合保证金类型查询应答
    virtual void OnRspQryClientMarginCombType(CUstpFtdcRspClientMarginCombTypeField* pRspClientMarginCombType, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///行权录入应答
    virtual void OnRspExecOrderInsert(CUstpFtdcInputExecOrderField* pInputExecOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///行权操作应答
    virtual void OnRspExecOrderAction(CUstpFtdcInputExecOrderActionField* pInputExecOrderAction, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///行权通知
    virtual void OnRtnExecOrder(CUstpFtdcExecOrderField* pExecOrder);

    ///行权录入错误回报
    virtual void OnErrRtnExecOrderInsert(CUstpFtdcInputExecOrderField* pInputExecOrder, CUstpFtdcRspInfoField* pRspInfo);

    ///行权操作错误回报
    virtual void OnErrRtnExecOrderAction(CUstpFtdcInputExecOrderActionField* pInputExecOrderAction, CUstpFtdcRspInfoField* pRspInfo);

    ///系统时间查询应答
    virtual void OnRspQrySystemTime(CUstpFtdcRspQrySystemTimeField* pRspQrySystemTime, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///风险通知
    virtual void OnRtnRiskNotify(CUstpFtdcRiskNotifyField* pRiskNotify);

    ///请求查询投资者结算结果响应
    virtual void OnRspQuerySettlementInfo(CUstpFtdcSettlementRspField* pSettlementRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///资金账户口令更新请求响应
    virtual void OnRspTradingAccountPasswordUpdate(CUstpFtdcTradingAccountPasswordUpdateRspField* pTradingAccountPasswordUpdateRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询仓单折抵信息响应
    virtual void OnRspQueryEWarrantOffset(CUstpFtdcEWarrantOffsetFieldRspField* pEWarrantOffsetFieldRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询转帐流水响应
    virtual void OnRspQueryTransferSeriaOffset(CUstpFtdcTransferSerialFieldRspField* pTransferSerialFieldRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询银期签约关系响应
    virtual void OnRspQueryAccountregister(CUstpFtdcAccountregisterRspField* pAccountregisterRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///期货发起银行资金转期货应答
    virtual void OnRspFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///期货发起银行资金转期货通知
    virtual void OnRtnFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq);

    ///期货发起期货资金转银行应答
    virtual void OnRspFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///期货发起期货资金转银行通知
    virtual void OnRtnFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField* pTransferFieldReq);

    ///银行发起期货资金转银行通知
    virtual void OnRtnFromFutureToBankByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq);

    ///银行发起银行资金转期货通知
    virtual void OnRtnFromBankToFutureByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq);

    ///期货发起查询银行余额应答
    virtual void OnRspQueryBankAccountMoneyByFuture(CUstpFtdcAccountFieldRspField* pAccountFieldRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///期货发起查询银行余额通知
    virtual void OnRtnQueryBankBalanceByFuture(CUstpFtdcAccountFieldRtnField* pAccountFieldRtn);

    ///银行发起银期开户通知
    virtual void OnRtnOpenAccountByBank(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField);

    ///银行发起银期销户通知
    virtual void OnRtnCancelAccountByBank(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField);

    ///银行发起银行资金转期货应答
    virtual void OnRspFromBankToFutureByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///银行发起期货资金转银行应答
    virtual void OnRspFromFutureToBankByBank(CUstpFtdcTransferFieldReqField* pTransferFieldReq, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///金仕达发起银行资金转期货通知
    virtual void OnRtnFromBankToFutureByJSD(CUstpFtdcJSDMoneyField* pJSDMoney);

    ///金仕达发起期货资金转银行通知
    virtual void OnRtnFromFutureToBankByJSD(CUstpFtdcJSDMoneyField* pJSDMoney);

    ///银期转账投资者资金账户变动通知
    virtual void OnRtnTransferInvestorAccountChanged(CUstpFtdcTransferFieldReqField* pTransferFieldReq);

    ///期货发起银行账户签约应答
    virtual void OnRspSignUpAccountByFuture(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///期货发起银行账户解约应答
    virtual void OnRspCancleAccountByFuture(CUstpFtdcSignUpOrCancleAccountRspFieldField* pSignUpOrCancleAccountRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///请求查询银行签约状态响应
    virtual void OnRspQuerySignUpOrCancleAccountStatus(CUstpFtdcQuerySignUpOrCancleAccountStatusRspFieldField* pQuerySignUpOrCancleAccountStatusRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///期货发起银行账户变更应答
    virtual void OnRspChangeAccountByFuture(CUstpFtdcChangeAccountRspFieldField* pChangeAccountRspField, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///仿真交易帐户申请响应
    virtual void OnRspOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField* pSimTradeAccountInfo, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///仿真交易帐户查询响应
    virtual void OnRspCheckOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField* pSimTradeAccountInfo, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///查询保证金监管系统经纪公司资金账户密钥响应
    virtual void OnRspCFMMCTradingAccountKey(CUstpFtdcCFMMCTradingAccountKeyRspField* pCFMMCTradingAccountKeyRsp, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///消息通知
    virtual void OnRtnMsgNotify(CUstpFtdcMsgNotifyField* pMsgNotify);

    ///持仓变化通知
    virtual void OnRtnClientPositionChange(CUstpFtdcClientPositionChangeField* pClientPositionChange);

    ///资金变化通知
    virtual void OnRtnInvestorAccountChange(CUstpFtdcInvestorAccountChangeField* pInvestorAccountChange);

    ///允许持仓变化通知应答
    virtual void OnRspQryEnableRtnMoneyPositoinChange(CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///历史订单查询应答
    virtual void OnRspQueryHisOrder(CUstpFtdcOrderField* pOrder, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

    ///历史成交单查询应答
    virtual void OnRspQueryHisTrade(CUstpFtdcTradeField* pTrade, CUstpFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast);

private:
    void OutputPack(FemasRtnDataPack& pack);
    TraderFemasOpen* m_trader;
};

}