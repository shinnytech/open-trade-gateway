/////////////////////////////////////////////////////////////////////////
///@system 新一代交易系统
///@company SunGard China
///@file KSVocApiDataType.h
///@brief 定义了客户端接口使用的业务数据类型
/////////////////////////////////////////////////////////////////////////

#ifndef __KSVOCAPIDATATYPE_H_INCLUDED_
#define __KSVOCAPIDATATYPE_H_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace KingstarAPI
{
	//数据表标识类型
	enum KS_TABLEID_TYPE
	{
		KS_DTI_Product = 0,
		KS_DTI_Instrument,
		KS_DTI_DepthMarketData,
		KS_DTI_InstrumentRate,
		KS_DTI_TradingAccount,
		KS_DTI_InvestorPosition
	};

	//扩展API类型
	enum KS_EXTAPI_TYPE
	{
		KS_COS_API = 0,
		KS_OPT_API,
		KS_VOC_API,
		KS_VOC_MDAPI,
		KS_PRD_API
	};

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalTypeType是一个条件类型
	/////////////////////////////////////////////////////////////////////////
	///大于等于条件价
#define KSCOS_GreaterEqualTermPrice '0'
	///小于等于条件价
#define KSCOS_LesserThanTermPrice '1'

	typedef char TKSConditionalTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderType是一个条件单类型
	/////////////////////////////////////////////////////////////////////////
	///行情触发
#define KSCOS_TRIGGERTYPE_QUOTATION '0'
	///开盘触发
#define KSCOS_TRIGGERTYPE_OPEN '1'
	///时间触发
#define KSCOS_TRIGGERTYPE_TIME '2'
	///行情和时间触发
#define KSCOS_TRIGGERTYPE_QUOTAIONANDTIME '5' 

	typedef char TKSConditionalOrderType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderStateAlterType是一个暂停或激活操作标志类型
	/////////////////////////////////////////////////////////////////////////
	///暂停
#define KSCOS_State_PAUSE '0'
	///激活
#define KSCOS_State_ACTIVE '1'

	typedef char TKSConditionalOrderStateAlterType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderSelectResultType是一个选择结果类型
	/////////////////////////////////////////////////////////////////////////
	// 重试
#define KSCOS_Select_AGAIN '0'
	// 跳过
#define KSCOS_Select_SKIP '1'
	// 终止
#define KSCOS_Select_ABORT '2'

	typedef char TKSConditionalOrderSelectResultType;

	/////////////////////////////////////////////////////////////////////////
	///TKSOrderPriceTypeType是一个报单价格类型类型
	/////////////////////////////////////////////////////////////////////////
	// 最新价
#define KSCOS_OrderPrice_LastPrice '0'
	// 买价
#define KSCOS_OrderPrice_BidPrice '1'
	//卖价
#define KSCOS_OrderPrice_AskPrice '2'

	typedef char TKSOrderPriceTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSScurrencyTypeType是一个委托价格类型类型
	/////////////////////////////////////////////////////////////////////////
	//最新价
#define KSCOS_Scurrency_LastPrice '1'
	//买卖价
#define KSCOS_Scurrency_SalePrice '2'
	//指定价
#define KSCOS_Scurrency_AnyPrice '3'
	//市价
#define KSCOS_Scurrency_MarketPrice '4'
	//涨跌停价
#define KSCOS_Scurrency_CHGPrice '5'

	typedef char TKSScurrencyTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalSourceType是一个条件单来源类型
	/////////////////////////////////////////////////////////////////////////
	//普通
#define KSCOS_ConditionalSource_Ordinary '0'
	//盈损单生成
#define KSCOS_ConditionalSource_ProfitAndLoss '1'

	typedef char TKSConditionalSourceType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCloseModeType是一个平仓价格类型
	/////////////////////////////////////////////////////////////////////////
	// 市价
#define KSPL_Close_MarketPrice '0'
	// 加减买卖价
#define KSPL_Close_SalePrice '1'
	//加减最新价
#define KSPL_Close_LastPrice '2'

	typedef char TKSCloseModeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSOffsetValueType是一个生成止损止盈价的方式类型
	/////////////////////////////////////////////////////////////////////////
	// 指定值
#define KSPL_OffsetValue_TermPrice '0'
	// 开仓成交价的相对偏移值
#define KSPL_OffsetValue_TradePrice '1'

	typedef char TKSOffsetValueType;

	/////////////////////////////////////////////////////////////////////////
	///TKSSpringTypeType是一个报单价格条件类型
	/////////////////////////////////////////////////////////////////////////
	///最新价
#define KSPL_SPRING_LastPrice '0'
	///买卖价
#define KSPL_SPRING_SalePrice '1'

	typedef char TKSSpringTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderStatusType是一个条件单状态类型
	/////////////////////////////////////////////////////////////////////////
	// 暂停
#define KSCOS_OrderStatus_PAUSENOTOUCHED      '0'
	// 未触发
#define KSCOS_OrderStatus_ACTIVENOTOUCHED    '1'
	//删除
#define KSCOS_OrderStatus_Deleted                     '2'
	//已触发未发送
#define KSCOS_OrderStatus_TOUCHEDNOSEND   '3'
	// 发送超时
#define KSCOS_OrderStatus_SENDTIMEOUT   '4'
	//发送成功
#define KSCOS_OrderStatus_SENDSUCCESS   '5'
	// 等待选择
#define KSCOS_OrderStatus_WAITSELECT   '6'
	// 选择跳过
#define KSCOS_OrderStatus_SELECTSKIP   '7'
	// 选择终止
#define KSCOS_OrderStatus_SELECTABORT   '8'

	typedef char TKSConditionalOrderStatusType;

	/////////////////////////////////////////////////////////////////////////
	///TKSProfitAndLossOrderStatusType是一个止损止盈单状态类型
	/////////////////////////////////////////////////////////////////////////
	// 未生成条件单
#define KSZSZY_OrderStatus_NOTGENERATED	'0'
	// 生成止损委托单
#define KSZSZY_OrderStatus_GENERATEORDERZS '1'
	// 生成条件单
#define KSZSZY_OrderStatus_ALREADYGENERATED '2'
	// 错误委托
#define KSZSZY_OrderStatus_ERRORDER	'3'
	// 生成止盈委托单
#define KSZSZY_OrderStatus_GENERATEORDERZY	'4'
	// 删除
#define KSZSZY_OrderStatus_DELETED	'd'

	typedef char TKSProfitAndLossOrderStatusTyp;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderIDType是一个条件单编号类型
	/////////////////////////////////////////////////////////////////////////
	typedef int TKSConditionalOrderIDType;

	/////////////////////////////////////////////////////////////////////////
	///TKSProfitAndLossOrderIDType是一个止损止盈单编号类型
	/////////////////////////////////////////////////////////////////////////
	typedef int TKSProfitAndLossOrderIDType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderSelectTypeType是一个条件单选择方式类型
	/////////////////////////////////////////////////////////////////////////
	// 确认、取消 
#define KSCOS_Select_ConfirmORCancel '1'
	// 重试、跳过、终止 
#define KS_Select_AgainOrSkipOrAbort '2'

	typedef char TKSConditionalOrderSelectTypeType;


	/////////////////////////////////////////////////////////////////////////
	const int  MAX_ORDER_COUNT = 20;

	/////////////////////////////////////////////////////////////////////////
	///TKSCloseStrategyType是一个平仓策略类型
	/////////////////////////////////////////////////////////////////////////
	///先开先平，先普通后组合
#define KSVOC_OpenFCloseF_OrdiComb '1'
	///先普通后组合，先开先平
#define KSVOC_OrdiComb_OpenFCloseF '2'
	///开仓日期倒序(后开日期的持仓先平，最先平今仓)
#define KSVOC_TodayF_TIME '3'

	typedef char TKSCloseStrategyType;

	/////////////////////////////////////////////////////////////////////////
	///TKSStrategyIDType是一个策略代码类型
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSStrategyIDType[12];

	/////////////////////////////////////////////////////////////////////////
	///TKSCombTypeType是一个组合类型类型
	/////////////////////////////////////////////////////////////////////////
	///套利
#define KSVOC_CombType_Arbitrage '0'
	///互换
#define KSVOC_CombType_Swap '1'

	typedef char TKSCombTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSStrikePriceType是一个执行价类型
	/////////////////////////////////////////////////////////////////////////
	// 执行价规则本腿低对腿高
#define KSVOC_StrikePrice_Low 'L'
	// 执行价规则本腿高对腿低
#define KSVOC_StrikePrice_High 'H'
	// 执行价规则两腿相同
#define KSVOC_StrikePrice_Minus 'E'
	// 不校验价格关系
#define KSVOC_StrikePrice_Plus 'N'

	typedef char TKSStrikePriceType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCalcFlagType是一个计算符号类型
	/////////////////////////////////////////////////////////////////////////
	// 加
#define KSVOC_CalcFlag_Plus '1'
	// 减
#define KSVOC_CalcFlag_Minus '2'

	typedef char TKSCalcFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TThostFtdcVolRatioType是一个数量类型
	/////////////////////////////////////////////////////////////////////////
	typedef double TThostFtdcVolRatioType;

	/////////////////////////////////////////////////////////////////////////
	///TThostFtdcMoneyRatioType是一个资金类型
	/////////////////////////////////////////////////////////////////////////
	typedef double TThostFtdcMoneyRatioType;

	/////////////////////////////////////////////////////////////////////////
	///TKSInfoTypeType是一个信息类型类型
	/////////////////////////////////////////////////////////////////////////
	// 普通
#define KSVOC_InfoType_Common '1'
	// 警告
#define KSVOC_InfoType_Warn '2'
	// 危险
#define KSVOC_InfoType_Risk '3'
	// 滚动
#define KSVOC_InfoType_Roll '4'
	// 强制确认类
#define KSVOC_InfoType_Force '5'

	typedef char TKSInfoTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConfirmFlagType是一个确认标志类型
	/////////////////////////////////////////////////////////////////////////
	// 未确认
#define KSVOC_ConfirmFlag_UnConfirm '0'
	// 已确认
#define KSVOC_ConfirmFlag_Confirmed '1'

	typedef char TKSConfirmFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSProductVersionType是一个版本号类型
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSProductVersionType[21];

	/////////////////////////////////////////////////////////////////////////
	///TKSLimitFlagType是一个限仓类型类型
	/////////////////////////////////////////////////////////////////////////
	// 权利仓
#define KSVOC_PF_F  'F'
	// 总持仓
#define KSVOC_PF_T  'T'
	// 当日买开上限
#define KSVOC_PF_D 'D'
	// 当日开仓限额
#define KSVOC_PF_O 'O'

	typedef char TKSLimitFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSControlRangeType是一个控制范围类型
	/////////////////////////////////////////////////////////////////////////
	// 品种
#define KSVOC_CR_Product '0'
	// 所有
#define KSVOC_CR_ALL  '1'

	typedef char TKSControlRangeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSTradeLevelType是一个交易级别类型
	/////////////////////////////////////////////////////////////////////////
	// 一级
#define KSVOC_TL_Level1 '1'
	// 二级
#define KSVOC_TL_Level2  '2'
	// 三级
#define KSVOC_TL_Level3  '3'

	typedef char TKSTradeLevelType;

	/////////////////////////////////////////////////////////////////////////
	///TKSSOPosiDirectionType是一个个股持仓方向类型
	/////////////////////////////////////////////////////////////////////////
	// 权利仓
#define KSVOC_SOPD_Buy '1'
	// 义务仓
#define KSVOC_SOPD_Sell  '2'

	typedef char TKSSOPosiDirectionType;

	/////////////////////////////////////////////////////////////////////////
	///TKSSODelivModeType是一个个股交收查询类型
	/////////////////////////////////////////////////////////////////////////
	// 行权标的净额交收
#define KSVOC_SODM_Product '1'
	// 行权现金结算交收明细
#define KSVOC_SODM_Cash  '2'
	// 行权违约处置扣券及返还
#define KSVOC_SODM_Dispos  '3'

	typedef char TKSSODelivModeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCombActionType是一个强拆标记类型
	/////////////////////////////////////////////////////////////////////////
	///非强拆
#define KSVOC_CAT_False '0'
	///强拆
#define KSVOC_CAT_True '1'

	typedef char TKSCombActionType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCombStrategyIDType是一个个股组合策略代码类型
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSCombStrategyIDType[12];

	/////////////////////////////////////////////////////////////////////////
	///TKSProfitAndLossFlagType是一个止损止盈标志类型
	/////////////////////////////////////////////////////////////////////////
	// 非止损非止盈
#define KSCOS_PLF_NotProfitNotLoss '0'
	// 止损 
#define KSCOS_PLF_Loss '1'
	// 止盈 
#define KSCOS_PLF_Profit '2'
	// 止损止盈
#define KSCOS_PLF_ProfitAndLoss '3'

	typedef char TKSProfitAndLossFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSFOCreditApplyType是一个当日出金额度操作类型
	/////////////////////////////////////////////////////////////////////////
	///申请
#define KSVOC_FOCAT_Confirm '0'
	///取消申请
#define KSVOC_FOCAT_NoConfirm '1'

	typedef char TKSFOCreditApplyType;

	/////////////////////////////////////////////////////////////////////////
	///TKSFOCreditStatusType是一个当日出金额度处理状态类型
	/////////////////////////////////////////////////////////////////////////
	///待审核
#define KSVOC_FOCST_Sending '0'
	///已通过
#define KSVOC_FOCST_Accepted '1'
	///已否决
#define KSVOC_FOCST_Reject '2'

	typedef char TKSFOCreditStatusType;


	/////////////////////////////////////////////////////////////////////////
	///TKSSOBusinessFlagType是一个业务标志类型
	/////////////////////////////////////////////////////////////////////////
	///买入开仓
#define KSVOC_SOOF_BuyOpen "0d"
	///卖出平仓
#define KSVOC_SOOF_SellClose "0e"
	///卖出开仓
#define KSVOC_SOOF_SellOpen "0f"
	///买入平仓
#define KSVOC_SOOF_BuyClose "0g"
	///备兑开仓
#define KSVOC_SOOF_CoveredOpen "0h"
	///备兑平仓
#define KSVOC_SOOF_CoveredClose "0i"
	///期权行权
#define KSVOC_SOOF_ExecOrder "0j"
	///认购期权行权指派权利方
#define KSVOC_SOOF_CallAssignedRight "2a"
	///认沽期权行权指派权利方
#define KSVOC_SOOF_PutAssignedRight "2b"
	///认购期权行权指派义务方（非备兑）
#define KSVOC_SOOF_CallAssignedDuty "2c"
	///认沽期权行权指派义务方
#define KSVOC_SOOF_PutAssignedDuty "2d"
	///认购期权行权指派义务方（备兑）
#define KSVOC_SOOF_CallAssignedDuty_Covered "2e"
	///认购期权行权交收权利方
#define KSVOC_SOOF_CallSettledRight "2f"
	///认沽期权行权交收权利方
#define KSVOC_SOOF_PutSettledRight "2g"
	///认购期权行交收义务方 (非备兑)
#define KSVOC_SOOF_CallSettledDuty "2h"
	///认沽期权行交收义务方
#define KSVOC_SOOF_PutSettledDuty "2i"
	///认购期权行交收义务方 (备兑)
#define KSVOC_SOOF_CallSettledDuty_Covered "2j"
	///行权交收交券违约(针对违约一方)
#define KSVOC_SOOF_ExecSettledDefault "2k"
	///行权交收交券违约(针对被违约一方)
#define KSVOC_SOOF_ExecSettledDefaulted "2l"
	///权利方合约持仓变动数据
#define KSVOC_SOOF_RightPosition "2m"
	///义务方合约持仓变动数据(非备兑)
#define KSVOC_SOOF_DutyPosition "2n"
	///义务方合约持仓变动数据(备兑)
#define KSVOC_SOOF_DutyPosition_Covered "2o"
	///转处置划出
#define KSVOC_SOOF_DisposalOut "2p"
	///转处置划回
#define KSVOC_SOOF_DisposalBack "2q"
	///行权交收应收证券
#define KSVOC_SOOF_ExecSettledRecv "2r"
	///行权交收应付证券
#define KSVOC_SOOF_ExecSettledDeal "2s"
	///期权行权欠款扣券交收（针对券商大账户）
#define KSVOC_SOOF_BrokerAccountDeal "2t"
	///备兑锁券业务代码
#define KSVOC_SOOF_CoveredLock "2u"

	typedef char TKSSOBusinessFlagType[3];

	/////////////////////////////////////////////////////////////////////////
	///TKSExecFrozenMarginParamsType是一个实值额是否计入执行冻结保证金参数类型
	/////////////////////////////////////////////////////////////////////////
	///实值额不计入执行冻结保证金
#define KSVOC_EFMP_ITM_NotInclude '0'
	///实值额计入执行冻结保证金
#define KSVOC_EFMP_ITM_Include '1'

	typedef char TKSExecFrozenMarginParamsType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCancelAutoExecParamsType是一个取消到期日自动行权处理方式参数类型
	/////////////////////////////////////////////////////////////////////////
	///手数为0的行权指令
#define KSVOC_CAEP_0 '0'
	///实值额计入执行冻结保证金
#define KSVOC_CAEP_1 '1'

	typedef char TKSCancelAutoExecParamsType;

	/////////////////////////////////////////////////////////////////////////
	///TKSRiskLevelType是一个风险级别类型
	/////////////////////////////////////////////////////////////////////////
	// 正常
#define KSVOC_RL_Level0 '0'
	// 追加
#define KSVOC_RL_Level1  '1'
	// 强平
#define KSVOC_RL_Level2  '2'
	// 穿仓
#define KSVOC_RL_Level3  '3'
	// 低权
#define KSVOC_RL_Level4  '4'

	typedef char TKSRiskLevelType;

	/////////////////////////////////////////////////////////////////////////
	///TKSStatusMsgType是一个处理状态信息类型
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSStatusMsgType[256];

	/////////////////////////////////////////////////////////////////////////
	///TKSResultMsgType是一个处理结果信息类型
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSResultMsgType[256];

	/////////////////////////////////////////////////////////////////////////
	///TKSOptSelfCloseFlagType是一个期权行权是否自对冲类型
	/////////////////////////////////////////////////////////////////////////
	///自对冲期权仓位
#define KSVOC_OSCF_CloseSelfOptionPosition '1'
	///保留期权仓位
#define KSVOC_OSCF_ReserveOptionPosition '2'
	///自对冲卖方履约后的期货仓位
#define KSVOC_OSCF_SellCloseSelfFuturePosition '3'
	///取消到期日自动行权
#define KSVOC_OSCF_RemoveAutoRightPosition '4'

	typedef char TKSOptSelfCloseFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TThostFtdcHedgingFlagType是一个对冲标志类型
	/////////////////////////////////////////////////////////////////////////
	///不对冲
#define KSVOC_HF_UnHedging '0'
	///对冲
#define KSVOC_HF_Hedging '1'

	typedef char TThostFtdcHedgingFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSTurnResOrTurnCashFlagType是一个转备兑转现金标志类型
	/////////////////////////////////////////////////////////////////////////
	///转备兑
#define KSVOC_ROC_TurnRes 'h'
	///转现金
#define KSVOC_ROC_TurnCash 'i'

	typedef char TKSTurnResOrTurnCashFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSBusinessLocalIDType是一个本地业务标识类型
	/////////////////////////////////////////////////////////////////////////
	//typedef int TKSBusinessLocalIDType;

	/////////////////////////////////////////////////////////////////////////
	///TKSOptionSelfCloseSysIDType是一个期权自对冲系统编号类型
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSOptionSelfCloseSysIDType[13];

	/////////////////////////////////////////////////////////////////////////
	///TKSTradingParamsTypeType是一个交易参数类别类型
	/////////////////////////////////////////////////////////////////////////
	///实值额是否计入执行冻结保证金参数
#define KSVOC_TPT_ExecFrozenMargin '1'
	///对冲
#define KSVOC_TPT_CancelAutoExec '2'

	typedef char TKSTradingParamsTypeType;

	////////////////////////////////////////////////////////////////////////
	///TKSSettlementTypeType是一个登陆时候柜台交易参数设置
	////////////////////////////////////////////////////////////////////////
	//不取对账单
#define KSVOC_ST_NOAchieveAccount '0'
	//取对账单，不必确认
#define KSVOC_ST_AchieveAndNoConfirm '1'
	//取对账单，必须确认
#define KSVOC_ST_AchieveAndConfirm '2'

	typedef char TKSSettlementTypeType;

	////////////////////////////////////////////////////////////////////////
	///TKSSourceType是一个来源类型
	////////////////////////////////////////////////////////////////////////
	//柜台正常
#define KSVOC_SOURCE_CounterCommon '0'
	//交易所
#define KSVOC_SOURCE_Exchange '1'
	//柜台通知
#define KSVOC_SOURCE_CounterRTN '2'
	//网关通知
#define KSVOC_SOURCE_SPXGateway '3'

	typedef char TKSSourceType;
}	// end of namespace KingstarAPI
#endif