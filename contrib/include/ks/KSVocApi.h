/////////////////////////////////////////////////////////////////////////
///@system 新一代交易系统
///@company SunGard China
///@file KSVocApi.h
///@brief 定义了客户端客户定制接口
/////////////////////////////////////////////////////////////////////////

#ifndef __KSVOCAPI_H_INCLUDED_
#define __KSVOCAPI_H_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KSUserApiStructEx.h"
#include "KSVocApiStruct.h"

#if defined(ISLIB) && defined(WIN32) && !defined(KSTRADEAPI_STATIC_LIB)
#ifdef LIB_TRADER_API_EXPORT
#define TRADER_VOCAPI_EXPORT __declspec(dllexport)
#else
#define TRADER_VOCAPI_EXPORT __declspec(dllimport)
#endif
#else
#ifdef WIN32
#define TRADER_VOCAPI_EXPORT 
#else
#define TRADER_VOCAPI_EXPORT __attribute__((visibility("default")))
#endif

#endif

namespace KingstarAPI
{

	class CKSVocSpi
	{
	public:
		///查询开盘前的持仓明细应答
		virtual void OnRspQryInvestorOpenPosition(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///查询开盘前的组合持仓明细应答
		virtual void OnRspQryInvestorOpenCombinePosition(CThostFtdcInvestorPositionCombineDetailField *pInvestorPositionCombineDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///批量报单撤除请求响应
		virtual void OnRspBulkCancelOrder(CThostFtdcBulkCancelOrderField *pBulkCancelOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///平仓策略查询响应
		virtual void OnRspQryCloseStrategy(CKSCloseStrategyResultField *pCloseStrategy, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///组合策略查询响应
		virtual void OnRspQryCombStrategy(CKSCombStrategyResultField *pCombStrategy, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///期权组合策略查询响应
		virtual void OnRspQryOptionCombStrategy(CKSOptionCombStrategyResultField *pOptionCombStrategy, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///请求查询客户转帐信息响应
		virtual void OnRspQryTransferInfo(CKSTransferInfoResultField *pResultField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询交易通知响应
		virtual void OnRspQryKSTradingNotice(CKSTradingNoticeField *pTradingNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///用户端产品资源查询应答
		virtual void OnRspQryUserProductUrl(CKSUserProductUrlField *pUserProductUrl, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///最大组合拆分单量查询请求响应
		virtual void OnRspQryMaxCombActionVolume(CKSMaxCombActionVolumeField *pMaxCombActionVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///交易通知
		virtual void OnRtnKSTradingNotice(CKSTradingNoticeField *pTradingNoticeInfo) {};

		///请求查询期权合约手续费响应
		virtual void OnRspQryKSInstrCommRate(CKSInstrCommRateField *pInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询ETF期权合约手续费响应
		virtual void OnRspQryKSETFOptionInstrCommRate(CKSETFOptionInstrCommRateField *pETFOptionInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询合约保证金率响应
		virtual void OnRspQryKSInstrumentMarginRate(CKSInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///客户每日出金额度申请响应
		virtual void OnRspWithdrawCreditApply(CKSInputWithdrawCreditApplyField *pWithdrawCreditApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///客户每日出金额度查询响应
		virtual void OnRspQryWithdrawCredit(CKSRspQryWithdrawCreditField *pRspQryWithdrawCredit, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///客户每日出金额度申请查询响应
		virtual void OnRspQryWithdrawCreditApply(CKSRspQryWithdrawCreditApplyField *pRspQryWithdrawCreditApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///大额出金预约（取消）申请响应
		virtual void OnRspLargeWithdrawApply(CKSLargeWithdrawApplyField *pLargeWithdrawApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///大额出金预约查询响应
		virtual void OnRspQryLargeWithdrawApply(CKSRspLargeWithdrawApplyField *pRspLargeWithdrawApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询个股单张保证金响应
		virtual void OnRspQryKSOptionMargin(CKSOptionMarginField *pOptionMargin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询KS交易参数响应
		virtual void OnRspQryKSTradingParams(CKSTradingParamsField *pTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询KS资金账户响应
		virtual void OnRspQryKSTradingAccount(CKSTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询预约出金帐号响应
		virtual void OnRspQryWithdrawReservedAccount(CKSWithdrawReservedAccountField *pWithdrawReservedAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///预约出金申请响应
		virtual void OnRspWithdrawReservedApply(CKSWithdrawReservedApplyField *pWithdrawReservedApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///预约出金撤销申请响应
		virtual void OnRspWithdrawReservedCancel(CKSWithdrawReservedCancelField *pWithdrawReservedCancel, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///请求查询预约出金响应
		virtual void OnRspQryWithdrawReservedApply(CKSRspWithdrawReservedApplyField *pRspWithdrawReservedApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///期权自对冲更新应答
		virtual void OnRspOptionSelfCloseUpdate(CKSInputOptionSelfCloseField *pInputOptionSelfClose, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///期权自对冲更新回报
		virtual void OnRtnOptionSelfCloseUpdate(CKSOptionSelfCloseField *pOptionSelfClose) {};

		///期权自对冲操作应答
		virtual void OnRspOptionSelfCloseAction(CKSOptionSelfCloseActionField *pOptionSelfCloseAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///期权自对冲查询应答
		virtual void OnRspQryOptionSelfClose(CKSOptionSelfCloseField *pOptionSelfClose, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		//请求查询组合保证金合约规则响应
		virtual void OnRspQryCombInstrumentMarginRule(CKSCombInstrumentMarginRuleField *pCombInstrumentMarginRuleField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};
	
		//请求查询组合合约名称响应
		virtual void OnRspQryCombInstrumentName(CKSCombInstrumentNameField *pCombInstrumentNameField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///9909测试应答
		virtual void OnRspEncryptionTest(CKSEncryptionTestField *pEncryptionTest, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};
	};

	class TRADER_VOCAPI_EXPORT CKSVocApi
	{
	public:
		///查询开盘前的持仓明细
		virtual int ReqQueryInvestorOpenPosition(CThostFtdcQryInvestorPositionDetailField *pQryInvestorOpenPosition, int nRequestID) = 0;

		///查询开盘前的组合持仓明细
		virtual int ReqQueryInvestorOpenCombinePosition(CThostFtdcQryInvestorPositionCombineDetailField *pQryInvestorOpenCombinePosition, int nRequestID) = 0;

		///批量撤单
		virtual int ReqBulkCancelOrder (CThostFtdcBulkCancelOrderField *pBulkCancelOrder, int nRequestID) = 0;

		///平仓策略查询请求
		virtual int ReqQryCloseStrategy(CKSCloseStrategy *pCloseStrategy, int nRequestID) = 0;

		///组合策略查询请求
		virtual int ReqQryCombStrategy(CKSCombStrategy *pCombStrategy, int nRequestID) = 0;

		///期权组合策略查询请求
		virtual int ReqQryOptionCombStrategy(CKSOptionCombStrategy *pOptionCombStrategy, int nRequestID) = 0;

		///请求查询客户转帐信息
		virtual int ReqQryTransferInfo(CKSTransferInfo *pTransferInfo, int nRequestID) = 0;

		///请求查询交易通知
		virtual int ReqQryKSTradingNotice(CKSQryTradingNoticeField *pQryTradingNotice, int nRequestID) = 0;

		///用户端产品资源查询请求
		virtual int ReqQryUserProductUrl (CKSQryUserProductUrlField *pQryUserProductUrl, int nRequestID) = 0;

		///最大组合拆分单量查询请求
		virtual int ReqQryMaxCombActionVolume(CKSQryMaxCombActionVolumeField *pQryMaxCombActionVolume, int nRequestID) = 0;

		///请求查询合约手续费
		virtual int ReqQryKSInstrCommRate(CKSQryInstrCommRateField *pQryInstrCommRate, int nRequestID) = 0;

		///请求查询ETF期权合约手续费
		virtual int ReqQryKSETFOptionInstrCommRate(CKSQryETFOptionInstrCommRateField *pQryETFOptionInstrCommRate, int nRequestID) = 0;

		///请求查询合约保证金率
		virtual int ReqQryKSInstrumentMarginRate(CKSQryInstrumentMarginRateField *pQryInstrumentMarginRate, int nRequestID) = 0;

		///客户每日出金额度申请
		virtual int ReqWithdrawCreditApply(CKSInputWithdrawCreditApplyField *pWithdrawCreditApply, int nRequestID) = 0;

		///客户每日出金额度查询
		virtual int ReqQryWithdrawCredit(CKSQryWithdrawCreditField *pQryWithdrawCredit, int nRequestID) = 0;

		///客户每日出金额度申请查询
		virtual int ReqQryWithdrawCreditApply(CKSQryWithdrawCreditApplyField *pQryWithdrawCreditApply, int nRequestID) = 0;

		///大额出金预约（取消）申请
		virtual int ReqLargeWithdrawApply(CKSLargeWithdrawApplyField *pLargeWithdrawApply, int nRequestID) = 0;

		///大额出金预约查询
		virtual int ReqQryLargeWithdrawApply(CKSQryLargeWithdrawApplyField *pQryLargeWithdrawApply, int nRequestID) = 0;

		///高华认证邮箱登录请求
		virtual int ReqKSEMailLogin(CKSReqEMailLoginField *pReqEMailLoginField, int nRequestID) = 0;

		///请求查询个股单张保证金
		virtual int ReqQryKSOptionMargin(CKSQryOptionMarginField *pQryOptionMargin, int nRequestID) = 0;

		///请求查询KS交易参数
		virtual int ReqQryKSTradingParams(CKSQryTradingParamsField *pQryTradingParams, int nRequestID) = 0;

		///请求查询KS资金账户
		virtual int ReqQryKSTradingAccount(CKSQryTradingAccountField *pQryTradingAccount, int nRequestID) = 0;

		///预约出金帐号查询
		virtual int ReqQryWithdrawReservedAccount(CKSQryWithdrawReservedAccountField *pQryWithdrawReservedAccount, int nRequestID) = 0;

		///预约出金申请
		virtual int ReqWithdrawReservedApply(CKSWithdrawReservedApplyField *pWithdrawReservedApply, int nRequestID) = 0;

		///预约出金撤销申请
		virtual int ReqWithdrawReservedCancel(CKSWithdrawReservedCancelField *pWithdrawReservedCancel, int nRequestID) = 0;

		///预约出金查询
		virtual int ReqQryWithdrawReservedApply(CKSQryWithdrawReservedApplyField *pQryWithdrawReservedApply, int nRequestID) = 0;

		///期权自对冲更新请求
		virtual int ReqOptionSelfCloseUpdate(CKSInputOptionSelfCloseField *pInputOptionSelfClose, int nRequestID) = 0;

		///期权自对冲操作请求
		virtual int ReqOptionSelfCloseAction(CKSOptionSelfCloseActionField *pOptionSelfCloseAction, int nRequestID) = 0;

		///期权自对冲查询请求
		virtual int ReqQryOptionSelfClose(CKSQryOptionSelfCloseField *pQryOptionSelfClose, int nRequestID) = 0;

		///请求查询组合保证金合约规则
		virtual int ReqQryCombInstrumentMarginRule(CKSQryCombInstrumentMarginRuleField *pQryCombInstrumentMarginRule, int nRequestId) = 0;
	
		///请求查询组合合约名称
		virtual int ReqQryCombInstrumentName(CKSQryCombInstrumentNameField *pQryCombInstrumentName, int nRequestID) = 0;

		///查询确认结算单模式
		///SettlementInfo为输出字段
		virtual int ReqQryTraderParameter(CKSSettlementInfoField *SettlementInfo, int nRequestID) = 0;

		///9909测试接口
		virtual int ReqEncryptionTest(CKSEncryptionTestField *pEncryptionTest, int nRequestID) = 0;

	protected:
		~CKSVocApi(){};
	};

}	// end of namespace KingstarAPI
#endif