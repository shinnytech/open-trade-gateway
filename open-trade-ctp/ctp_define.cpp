/////////////////////////////////////////////////////////////////////////
///@file ctp_define.cpp
///@brief	CTP接口相关数据结构及序列化函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "ctp_define.h"
#include "utility.h"

void SerializerCtp::DefineStruct(CThostFtdcReqTransferField& d)
{
	AddItem(d.BankID, ("bank_id"));
	AddItem(d.AccountID, ("future_account"));
	AddItem(d.Password, ("future_password"));
	AddItem(d.BankPassWord, ("bank_password"));
	AddItem(d.CurrencyID, ("currency"));
	AddItem(d.TradeAmount, ("amount"));
}

void SerializerCtp::DefineStruct(OrderKeyFile& d)
{
	AddItem(d.trading_day, "trading_day");
	AddItem(d.items, "items");
}

void SerializerCtp::DefineStruct(CThostFtdcTransferSerialField& d)
{
	AddItem(d.CurrencyID, ("currency"));
	AddItem(d.TradeAmount, ("amount"));
	if (d.TradeCode == "")
	{
		d.TradeAmount = 0 - d.TradeAmount;
	}
	DateTime dt;
	dt.time.microsecond = 0;
	sscanf(d.TradeDate, "%04d%02d%02d", &dt.date.year, &dt.date.month, &dt.date.day);
	sscanf(d.TradeTime, "%02d:%02d:%02d", &dt.time.hour, &dt.time.minute, &dt.time.second);
	long long trade_date_time = DateTimeToEpochNano(&dt);
	AddItem(trade_date_time, ("datetime"));
	AddItem(d.ErrorID, ("error_id"));
	AddItem(d.ErrorMsg, ("error_msg"));
}

void SerializerCtp::DefineStruct(OrderKeyPair& d)
{
	AddItem(d.local_key, "local");
	AddItem(d.remote_key, "remote");
}

void SerializerCtp::DefineStruct(LocalOrderKey& d)
{
	AddItem(d.user_id, "user_id");
	AddItem(d.order_id, "order_id");
}

void SerializerCtp::DefineStruct(RemoteOrderKey& d)
{
	AddItem(d.exchange_id, "exchange_id");
	AddItem(d.instrument_id, "instrument_id");
	AddItem(d.session_id, "session_id");
	AddItem(d.front_id, "front_id");
	AddItem(d.order_ref, "order_ref");
}

void SerializerCtp::DefineStruct(CtpActionInsertOrder& d)
{
	AddItem(d.local_key.user_id, ("user_id"));
	AddItem(d.local_key.order_id, ("order_id"));
	AddItem(d.f.ExchangeID, ("exchange_id"));
	AddItem(d.f.InstrumentID, ("instrument_id"));
	AddItemEnum(d.f.Direction, ("direction"), {
		{ THOST_FTDC_D_Buy, ("BUY") },
		{ THOST_FTDC_D_Sell, ("SELL") },
		});
	AddItemEnum(d.f.CombOffsetFlag[0], ("offset"), {
		{ THOST_FTDC_OF_Open, ("OPEN") },
		{ THOST_FTDC_OF_Close, ("CLOSE") },
		{ THOST_FTDC_OF_CloseToday, ("CLOSETODAY") },
		{ THOST_FTDC_OF_CloseYesterday, ("CLOSE") },
		{ THOST_FTDC_OF_ForceOff, ("CLOSE") },
		{ THOST_FTDC_OF_LocalForceClose, ("CLOSE") },
		});
	AddItem(d.f.LimitPrice, ("limit_price"));
	AddItem(d.f.VolumeTotalOriginal, ("volume"));
	AddItemEnum(d.f.OrderPriceType, ("price_type"), {
		{ THOST_FTDC_OPT_LimitPrice, ("LIMIT") },
		{ THOST_FTDC_OPT_AnyPrice, ("ANY") },
		{ THOST_FTDC_OPT_BestPrice, ("BEST") },
		{ THOST_FTDC_OPT_FiveLevelPrice, ("FIVELEVEL") },
		});
	AddItemEnum(d.f.VolumeCondition, ("volume_condition"), {
		{ THOST_FTDC_VC_AV, ("ANY") },
		{ THOST_FTDC_VC_MV, ("MIN") },
		{ THOST_FTDC_VC_CV, ("ALL") },
		});
	AddItemEnum(d.f.TimeCondition, ("time_condition"), {
		{ THOST_FTDC_TC_IOC, ("IOC") },
		{ THOST_FTDC_TC_GFS, ("GFS") },
		{ THOST_FTDC_TC_GFD, ("GFD") },
		{ THOST_FTDC_TC_GTD, ("GTD") },
		{ THOST_FTDC_TC_GTC, ("GTC") },
		{ THOST_FTDC_TC_GFA, ("GFA") },
		});
	d.f.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	AddItemEnum(d.f.CombHedgeFlag[0], ("hedge_flag"), {
	   { THOST_FTDC_HF_Speculation, ("SPECULATION") },
	   { THOST_FTDC_HF_Arbitrage, ("ARBITRAGE") },
	   { THOST_FTDC_HF_Hedge, ("HEDGE") },
	   { THOST_FTDC_HF_MarketMaker, ("MARKETMAKER") },
		});
	d.f.ContingentCondition = THOST_FTDC_CC_Immediately;
	AddItemEnum(d.f.ContingentCondition, ("contingent_condition"), {
		{ THOST_FTDC_CC_Immediately, ("IMMEDIATELY") },
		{ THOST_FTDC_CC_Touch, ("TOUCH") },
		{ THOST_FTDC_CC_TouchProfit, ("TOUCHPROFIT") },
		});
	d.f.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
}

void SerializerCtp::DefineStruct(CtpActionCancelOrder& d)
{
	AddItem(d.local_key.user_id, ("user_id"));
	AddItem(d.local_key.order_id, ("order_id"));
}

void SerializerCtp::DefineStruct(CThostFtdcUserPasswordUpdateField& d)
{
	AddItem(d.OldPassword, ("old_password"));
	AddItem(d.NewPassword, ("new_password"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcRspAuthenticateField& d)
{
	AddItem("CThostFtdcRspAuthenticateField", ("aid"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strUserProductInfo = GBKToUTF8(d.UserProductInfo);
	AddItem(strUserProductInfo, ("UserProductInfo"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcRspUserLoginField& d)
{
	AddItem("CThostFtdcRspUserLoginField", ("aid"));

	std::string strTradingDay = GBKToUTF8(d.TradingDay);
	AddItem(strTradingDay, ("TradingDay"));

	std::string strLoginTime = GBKToUTF8(d.LoginTime);
	AddItem(strLoginTime, ("LoginTime"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	std::string strSystemName = GBKToUTF8(d.SystemName);
	AddItem(strSystemName, ("SystemName"));

	AddItem(d.FrontID, ("FrontID"));

	AddItem(d.SessionID, ("SessionID"));

	AddItem(d.MaxOrderRef, ("MaxOrderRef"));

	AddItem(d.SHFETime, ("SHFETime"));

	AddItem(d.DCETime, ("DCETime"));

	AddItem(d.CZCETime, ("CZCETime"));

	AddItem(d.FFEXTime, ("FFEXTime"));

	AddItem(d.INETime, ("INETime"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcSettlementInfoConfirmField& d)
{
	AddItem("CThostFtdcSettlementInfoConfirmField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(d.BrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.ConfirmDate, ("ConfirmDate"));
	AddItem(d.ConfirmTime, ("ConfirmTime"));

	AddItem(d.SettlementID, ("SettlementID"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcSettlementInfoField& d)
{
	AddItem("CThostFtdcSettlementInfoField", ("aid"));

	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.SettlementID, ("SettlementID"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.SequenceNo, ("SequenceNo"));

	std::string strContent = GBKToUTF8(d.Content);
	AddItem(strContent, ("Content"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcUserPasswordUpdateField& d)
{
	AddItem("CThostFtdcUserPasswordUpdateField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	std::string strOldPassword = GBKToUTF8(d.OldPassword);
	AddItem(strOldPassword, ("OldPassword"));

	std::string strNewPassword = GBKToUTF8(d.NewPassword);
	AddItem(strNewPassword, ("NewPassword"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcInputOrderField& d)
{
	AddItem("CThostFtdcInputOrderField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.InstrumentID, ("InstrumentID"));
	AddItem(d.OrderRef, ("OrderRef"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	AddItem(d.OrderPriceType, ("OrderPriceType"));
	AddItem(d.Direction, ("Direction"));
	AddItem(d.CombOffsetFlag, ("CombOffsetFlag"));

	AddItem(d.CombHedgeFlag, ("CombHedgeFlag"));
	AddItem(d.LimitPrice, ("LimitPrice"));
	AddItem(d.VolumeTotalOriginal, ("VolumeTotalOriginal"));
	AddItem(d.TimeCondition, ("TimeCondition"));

	AddItem(d.GTDDate, ("GTDDate"));
	AddItem(d.VolumeCondition, ("VolumeCondition"));
	AddItem(d.MinVolume, ("MinVolume"));
	AddItem(d.ContingentCondition, ("ContingentCondition"));

	AddItem(d.StopPrice, ("StopPrice"));
	AddItem(d.ForceCloseReason, ("ForceCloseReason"));
	AddItem(d.IsAutoSuspend, ("IsAutoSuspend"));

	std::string strBusinessUnit = GBKToUTF8(d.BusinessUnit);
	AddItem(strBusinessUnit, ("BusinessUnit"));

	AddItem(d.RequestID, ("RequestID"));
	AddItem(d.UserForceClose, ("UserForceClose"));
	AddItem(d.IsSwapOrder, ("IsSwapOrder"));
	AddItem(d.ExchangeID, ("ExchangeID"));

	std::string strInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(strInvestUnitID, ("InvestUnitID"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	std::string strClientID = GBKToUTF8(d.ClientID);
	AddItem(strClientID, ("ClientID"));

	std::string strIPAddress = GBKToUTF8(d.IPAddress);
	AddItem(strIPAddress, ("IPAddress"));

	std::string strMacAddress = GBKToUTF8(d.MacAddress);
	AddItem(strMacAddress, ("MacAddress"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcInputOrderActionField& d)
{
	AddItem("CThostFtdcInputOrderActionField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.OrderActionRef, ("OrderActionRef"));
	AddItem(d.OrderRef, ("OrderRef"));


	AddItem(d.RequestID, ("RequestID"));
	AddItem(d.FrontID, ("FrontID"));
	AddItem(d.SessionID, ("SessionID"));
	AddItem(d.ExchangeID, ("ExchangeID"));

	AddItem(d.OrderSysID, ("OrderSysID"));
	AddItem(d.ActionFlag, ("ActionFlag"));
	AddItem(d.LimitPrice, ("LimitPrice"));
	AddItem(d.VolumeChange, ("VolumeChange"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));


	AddItem(d.InstrumentID, ("InstrumentID"));

	std::string strInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(strInvestUnitID, ("InvestUnitID"));

	std::string strIPAddress = GBKToUTF8(d.IPAddress);
	AddItem(strIPAddress, ("IPAddress"));

	std::string strMacAddress = GBKToUTF8(d.MacAddress);
	AddItem(strMacAddress, ("MacAddress"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcOrderActionField& d)
{
	AddItem("CThostFtdcOrderActionField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.OrderActionRef, ("OrderActionRef"));
	AddItem(d.OrderRef, ("OrderRef"));

	AddItem(d.RequestID, ("RequestID"));
	AddItem(d.FrontID, ("FrontID"));
	AddItem(d.SessionID, ("SessionID"));
	AddItem(d.ExchangeID, ("ExchangeID"));

	AddItem(d.OrderSysID, ("OrderSysID"));
	AddItem(d.ActionFlag, ("ActionFlag"));
	AddItem(d.LimitPrice, ("LimitPrice"));
	AddItem(d.VolumeChange, ("VolumeChange"));

	AddItem(d.ActionDate, ("ActionDate"));
	AddItem(d.ActionTime, ("ActionTime"));
	AddItem(d.TraderID, ("TraderID"));
	AddItem(d.InstallID, ("InstallID"));

	AddItem(d.OrderLocalID, ("OrderLocalID"));
	AddItem(d.ActionLocalID, ("ActionLocalID"));
	AddItem(d.ParticipantID, ("ParticipantID"));
	AddItem(d.ClientID, ("ClientID"));

	std::string strBusinessUnit = GBKToUTF8(d.BusinessUnit);
	AddItem(strBusinessUnit, ("BusinessUnit"));

	AddItem(d.OrderActionStatus, ("OrderActionStatus"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	std::string strStatusMsg = GBKToUTF8(d.StatusMsg);
	AddItem(strStatusMsg, ("StatusMsg"));

	AddItem(d.InstrumentID, ("InstrumentID"));
	AddItem(d.BranchID, ("BranchID"));

	std::string strInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(strInvestUnitID, ("InvestUnitID"));

	std::string strIPAddress = GBKToUTF8(d.IPAddress);
	AddItem(strIPAddress, ("IPAddress"));

	std::string strMacAddress = GBKToUTF8(d.MacAddress);
	AddItem(strMacAddress, ("MacAddress"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcInvestorPositionField& d)
{
	AddItem("CThostFtdcInvestorPositionField", ("aid"));

	AddItem(d.InstrumentID, ("InstrumentID"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.PosiDirection, ("PosiDirection"));

	AddItem(d.HedgeFlag, ("HedgeFlag"));
	AddItem(d.PositionDate, ("PositionDate"));
	AddItem(d.YdPosition, ("YdPosition"));
	AddItem(d.Position, ("Position"));

	AddItem(d.LongFrozen, ("LongFrozen"));
	AddItem(d.ShortFrozen, ("ShortFrozen"));
	AddItem(d.LongFrozenAmount, ("LongFrozenAmount"));
	AddItem(d.ShortFrozenAmount, ("ShortFrozenAmount"));

	AddItem(d.OpenVolume, ("OpenVolume"));
	AddItem(d.CloseVolume, ("CloseVolume"));
	AddItem(d.OpenAmount, ("OpenAmount"));
	AddItem(d.CloseAmount, ("CloseAmount"));

	AddItem(d.PositionCost, ("PositionCost"));
	AddItem(d.PreMargin, ("PreMargin"));
	AddItem(d.UseMargin, ("UseMargin"));
	AddItem(d.FrozenMargin, ("FrozenMargin"));

	AddItem(d.FrozenCash, ("FrozenCash"));
	AddItem(d.FrozenCommission, ("FrozenCommission"));
	AddItem(d.CashIn, ("CashIn"));
	AddItem(d.Commission, ("Commission"));

	AddItem(d.CloseProfit, ("CloseProfit"));
	AddItem(d.PositionProfit, ("PositionProfit"));
	AddItem(d.PreSettlementPrice, ("PreSettlementPrice"));
	AddItem(d.SettlementPrice, ("SettlementPrice"));

	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.SettlementID, ("SettlementID"));
	AddItem(d.OpenCost, ("OpenCost"));
	AddItem(d.ExchangeMargin, ("ExchangeMargin"));

	AddItem(d.CombPosition, ("CombPosition"));
	AddItem(d.CombLongFrozen, ("CombLongFrozen"));
	AddItem(d.CombShortFrozen, ("CombShortFrozen"));
	AddItem(d.CloseProfitByDate, ("CloseProfitByDate"));

	AddItem(d.CloseProfitByTrade, ("CloseProfitByTrade"));
	AddItem(d.TodayPosition, ("TodayPosition"));
	AddItem(d.MarginRateByMoney, ("MarginRateByMoney"));
	AddItem(d.MarginRateByVolume, ("MarginRateByVolume"));

	AddItem(d.StrikeFrozen, ("StrikeFrozen"));
	AddItem(d.StrikeFrozenAmount, ("StrikeFrozenAmount"));
	AddItem(d.AbandonFrozen, ("AbandonFrozen"));
	AddItem(d.ExchangeID, ("ExchangeID"));

	AddItem(d.YdStrikeFrozen, ("YdStrikeFrozen"));

	std::string strInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(strInvestUnitID, ("InvestUnitID"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcBrokerTradingParamsField& d)
{
	AddItem("CThostFtdcBrokerTradingParamsField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.MarginPriceType, ("MarginPriceType"));
	AddItem(d.Algorithm, ("Algorithm"));

	AddItem(d.AvailIncludeCloseProfit, ("AvailIncludeCloseProfit"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	AddItem(d.OptionRoyaltyPriceType, ("OptionRoyaltyPriceType"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcTradingAccountField& d)
{
	AddItem("CThostFtdcTradingAccountField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));
	
	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	AddItem(d.PreMortgage, ("PreMortgage"));
	AddItem(d.PreCredit, ("PreCredit"));

	AddItem(d.PreDeposit, ("PreDeposit"));
	AddItem(d.PreBalance, ("PreBalance"));
	AddItem(d.PreMargin, ("PreMargin"));
	AddItem(d.InterestBase, ("InterestBase"));

	AddItem(d.Interest, ("Interest"));
	AddItem(d.Deposit, ("Deposit"));
	AddItem(d.Withdraw, ("Withdraw"));
	AddItem(d.FrozenMargin, ("FrozenMargin"));

	AddItem(d.FrozenCash, ("FrozenCash"));
	AddItem(d.FrozenCommission, ("FrozenCommission"));
	AddItem(d.CurrMargin, ("CurrMargin"));
	AddItem(d.CashIn, ("CashIn"));

	AddItem(d.Commission, ("Commission"));
	AddItem(d.CloseProfit, ("CloseProfit"));
	AddItem(d.PositionProfit, ("PositionProfit"));
	AddItem(d.Balance, ("Balance"));

	AddItem(d.Available, ("Available"));
	AddItem(d.WithdrawQuota, ("WithdrawQuota"));
	AddItem(d.Reserve, ("Reserve"));
	AddItem(d.TradingDay, ("TradingDay"));

	AddItem(d.SettlementID, ("SettlementID"));
	AddItem(d.Credit, ("Credit"));
	AddItem(d.Mortgage, ("Mortgage"));
	AddItem(d.ExchangeMargin, ("ExchangeMargin"));

	AddItem(d.DeliveryMargin, ("DeliveryMargin"));
	AddItem(d.ExchangeDeliveryMargin, ("ExchangeDeliveryMargin"));
	AddItem(d.ReserveBalance, ("ReserveBalance"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	AddItem(d.PreFundMortgageIn, ("PreFundMortgageIn"));
	AddItem(d.PreFundMortgageOut, ("PreFundMortgageOut"));
	AddItem(d.FundMortgageIn, ("FundMortgageIn"));
	AddItem(d.FundMortgageOut, ("FundMortgageOut"));

	AddItem(d.FundMortgageAvailable, ("FundMortgageAvailable"));
	AddItem(d.MortgageableFund, ("MortgageableFund"));
	AddItem(d.SpecProductMargin, ("SpecProductMargin"));
	AddItem(d.SpecProductFrozenMargin, ("SpecProductFrozenMargin"));

	AddItem(d.SpecProductCommission, ("SpecProductCommission"));
	AddItem(d.SpecProductFrozenCommission, ("SpecProductFrozenCommission"));
	AddItem(d.SpecProductPositionProfit, ("SpecProductPositionProfit"));
	AddItem(d.SpecProductCloseProfit, ("SpecProductCloseProfit"));

	AddItem(d.SpecProductPositionProfitByAlg, ("SpecProductPositionProfitByAlg"));
	AddItem(d.SpecProductExchangeMargin, ("SpecProductExchangeMargin"));
	AddItem(d.BizType, ("BizType"));
	AddItem(d.FrozenSwap, ("FrozenSwap"));

	AddItem(d.RemainSwap, ("RemainSwap"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcContractBankField& d)
{
	AddItem("CThostFtdcContractBankField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strBankID = GBKToUTF8(d.BankID);
	AddItem(strBankID, ("BankID"));

	std::string strBankBrchID = GBKToUTF8(d.BankBrchID);
	AddItem(strBankBrchID, ("BankBrchID"));

	std::string strBankName = GBKToUTF8(d.BankName);
	AddItem(strBankName, ("BankName"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcAccountregisterField& d)
{
	AddItem("CThostFtdcAccountregisterField", ("aid"));

	AddItem(d.TradeDay, ("TradeDay"));

	std::string strBankID = GBKToUTF8(d.BankID);
	AddItem(strBankID, ("BankID"));

	std::string strBankBranchID = GBKToUTF8(d.BankBranchID);
	AddItem(strBankBranchID, ("BankBranchID"));

	std::string strBankAccount = GBKToUTF8(d.BankAccount);
	AddItem(strBankAccount, ("BankAccount"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strBrokerBranchID = GBKToUTF8(d.BrokerBranchID);
	AddItem(strBrokerBranchID, ("BrokerBranchID"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	AddItem(d.IdCardType, ("IdCardType"));

	std::string strIdentifiedCardNo = GBKToUTF8(d.IdentifiedCardNo);
	AddItem(strIdentifiedCardNo, ("IdentifiedCardNo"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	AddItem(d.OpenOrDestroy, ("OpenOrDestroy"));

	AddItem(d.RegDate, ("RegDate"));

	AddItem(d.OutDate, ("OutDate"));

	AddItem(d.TID, ("TID"));

	AddItem(d.CustType, ("CustType"));
	AddItem(d.BankAccType, ("BankAccType"));

	std::string strCustomerName = GBKToUTF8(d.CustomerName);
	AddItem(strCustomerName, ("CustomerName"));

	std::string strLongCustomerName = GBKToUTF8(d.LongCustomerName);
	AddItem(strLongCustomerName, ("LongCustomerName"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcTransferSerialField& d)
{
	AddItem("CThostFtdcTransferSerialField", ("aid"));

	AddItem(d.PlateSerial, ("PlateSerial"));
	AddItem(d.TradeDate, ("TradeDate"));
	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.TradeTime, ("TradeTime"));

	AddItem(d.TradeCode, ("TradeCode"));
	AddItem(d.SessionID, ("SessionID"));

	std::string strBankID = GBKToUTF8(d.BankID);
	AddItem(strBankID, ("BankID"));

	std::string strBankBranchID = GBKToUTF8(d.BankBranchID);
	AddItem(strBankBranchID, ("BankBranchID"));

	AddItem(d.BankAccType, ("BankAccType"));

	std::string strBankAccount = GBKToUTF8(d.BankAccount);
	AddItem(strBankAccount, ("BankAccount"));

	std::string strBankSerial = GBKToUTF8(d.BankSerial);
	AddItem(strBankSerial, ("BankSerial"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strBrokerBranchID = GBKToUTF8(d.BrokerBranchID);
	AddItem(strBrokerBranchID, ("BrokerBranchID"));

	AddItem(d.FutureAccType, ("FutureAccType"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));


	AddItem(d.FutureSerial, ("FutureSerial"));
	AddItem(d.IdCardType, ("IdCardType"));

	std::string strIdentifiedCardNo = GBKToUTF8(d.IdentifiedCardNo);
	AddItem(strIdentifiedCardNo, ("IdentifiedCardNo"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	AddItem(d.TradeAmount, ("TradeAmount"));
	AddItem(d.CustFee, ("CustFee"));
	AddItem(d.BrokerFee, ("BrokerFee"));
	AddItem(d.AvailabilityFlag, ("AvailabilityFlag"));

	std::string strOperatorCode = GBKToUTF8(d.OperatorCode);
	AddItem(strOperatorCode, ("OperatorCode"));

	std::string strBankNewAccount = GBKToUTF8(d.BankNewAccount);
	AddItem(strBankNewAccount, ("BankNewAccount"));

	AddItem(d.ErrorID, ("ErrorID"));

	std::string strErrorMsg = GBKToUTF8(d.ErrorMsg);
	AddItem(strErrorMsg, ("ErrorMsg"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcRspTransferField& d)
{
	AddItem("CThostFtdcRspTransferField", ("aid"));

	AddItem(d.TradeCode, ("TradeCode"));

	std::string strBankID = GBKToUTF8(d.BankID);
	AddItem(strBankID, ("BankID"));

	std::string strBrokerBranchID = GBKToUTF8(d.BrokerBranchID);
	AddItem(strBrokerBranchID, ("BrokerBranchID"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	AddItem(d.TradeDate, ("TradeDate"));
	AddItem(d.TradeTime, ("TradeTime"));

	std::string strBankSerial = GBKToUTF8(d.BankSerial);
	AddItem(strBankSerial, ("BankSerial"));

	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.PlateSerial, ("PlateSerial"));
	AddItem(d.LastFragment, ("LastFragment"));
	AddItem(d.SessionID, ("SessionID"));

	AddItem(d.IdCardType, ("IdCardType"));

	std::string strIdentifiedCardNo = GBKToUTF8(d.IdentifiedCardNo);
	AddItem(strIdentifiedCardNo, ("IdentifiedCardNo"));

	AddItem(d.CustType, ("CustType"));

	std::string strBankAccount = GBKToUTF8(d.BankAccount);
	AddItem(strBankAccount, ("BankAccount"));

	std::string strBankPassWord = GBKToUTF8(d.BankPassWord);
	AddItem(strBankPassWord, ("BankPassWord"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strPassword = GBKToUTF8(d.Password);
	AddItem(strPassword, ("Password"));

	AddItem(d.InstallID, ("InstallID"));

	AddItem(d.FutureSerial, ("FutureSerial"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	AddItem(d.VerifyCertNoFlag, ("VerifyCertNoFlag"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	AddItem(d.TradeAmount, ("TradeAmount"));
	AddItem(d.FutureFetchAmount, ("FutureFetchAmount"));
	AddItem(d.FeePayFlag, ("FeePayFlag"));
	AddItem(d.CustFee, ("CustFee"));

	AddItem(d.BrokerFee, ("BrokerFee"));
	AddItem(d.BankAccType, ("BankAccType"));

	std::string strDeviceID = GBKToUTF8(d.DeviceID);
	AddItem(strDeviceID, ("DeviceID"));

	AddItem(d.BankSecuAccType, ("BankSecuAccType"));

	std::string strBrokerIDByBank = GBKToUTF8(d.BrokerIDByBank);
	AddItem(strBrokerIDByBank, ("BrokerIDByBank"));

	AddItem(d.BankSecuAcc, ("BankSecuAcc"));
	AddItem(d.BankPwdFlag, ("BankPwdFlag"));
	AddItem(d.SecuPwdFlag, ("SecuPwdFlag"));

	std::string strOperNo = GBKToUTF8(d.OperNo);
	AddItem(strOperNo, ("OperNo"));

	AddItem(d.RequestID, ("RequestID"));

	AddItem(d.TID, ("TID"));
	AddItem(d.TransferStatus, ("TransferStatus"));

	AddItem(d.ErrorID, ("ErrorID"));

	std::string strCustomerName = GBKToUTF8(d.CustomerName);
	AddItem(strCustomerName, ("CustomerName"));

	std::string strMessage = GBKToUTF8(d.Message);
	AddItem(strMessage, ("Message"));

	std::string strDigest = GBKToUTF8(d.Digest);
	AddItem(strDigest, ("Digest"));

	std::string strErrorMsg = GBKToUTF8(d.ErrorMsg);
	AddItem(strErrorMsg, ("ErrorMsg"));

	std::string strLongCustomerName = GBKToUTF8(d.LongCustomerName);
	AddItem(strLongCustomerName, ("LongCustomerName"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcReqTransferField& d)
{
	AddItem("CThostFtdcReqTransferField", ("aid"));

	AddItem(d.TradeCode, ("TradeCode"));

	std::string strBankID = GBKToUTF8(d.BankID);
	AddItem(strBankID, ("BankID"));

	std::string strBankBranchID = GBKToUTF8(d.BankBranchID);
	AddItem(strBankBranchID, ("BankBranchID"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strBrokerBranchID = GBKToUTF8(d.BrokerBranchID);
	AddItem(strBrokerBranchID, ("BrokerBranchID"));

	AddItem(d.TradeDate, ("TradeDate"));
	AddItem(d.TradeTime, ("TradeTime"));
	AddItem(d.BankSerial, ("BankSerial"));

	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.PlateSerial, ("PlateSerial"));
	AddItem(d.LastFragment, ("LastFragment"));
	AddItem(d.SessionID, ("SessionID"));

	AddItem(d.IdCardType, ("IdCardType"));

	std::string strIdentifiedCardNo = GBKToUTF8(d.IdentifiedCardNo);
	AddItem(strIdentifiedCardNo, ("IdentifiedCardNo"));

	AddItem(d.CustType, ("CustType"));

	std::string strBankAccount = GBKToUTF8(d.BankAccount);
	AddItem(strBankAccount, ("BankAccount"));

	std::string strBankPassWord = GBKToUTF8(d.BankPassWord);
	AddItem(strBankPassWord, ("BankPassWord"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strPassword = GBKToUTF8(d.Password);
	AddItem(strPassword, ("Password"));

	AddItem(d.InstallID, ("InstallID"));

	AddItem(d.FutureSerial, ("FutureSerial"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	AddItem(d.VerifyCertNoFlag, ("VerifyCertNoFlag"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	AddItem(d.TradeAmount, ("TradeAmount"));
	AddItem(d.FutureFetchAmount, ("FutureFetchAmount"));
	AddItem(d.FeePayFlag, ("FeePayFlag"));
	AddItem(d.CustFee, ("CustFee"));

	AddItem(d.BrokerFee, ("BrokerFee"));
	AddItem(d.BankAccType, ("BankAccType"));

	std::string strDeviceID = GBKToUTF8(d.DeviceID);
	AddItem(strDeviceID, ("DeviceID"));

	AddItem(d.BankSecuAccType, ("BankSecuAccType"));

	std::string strBrokerIDByBank = GBKToUTF8(d.BrokerIDByBank);
	AddItem(strBrokerIDByBank, ("BrokerIDByBank"));

	std::string strBankSecuAcc = GBKToUTF8(d.BankSecuAcc);
	AddItem(strBankSecuAcc, ("BankSecuAcc"));

	AddItem(d.BankPwdFlag, ("BankPwdFlag"));

	AddItem(d.SecuPwdFlag, ("SecuPwdFlag"));

	std::string strOperNo = GBKToUTF8(d.OperNo);
	AddItem(strOperNo, ("OperNo"));

	AddItem(d.RequestID, ("RequestID"));
	AddItem(d.TID, ("TID"));
	AddItem(d.TransferStatus, ("TransferStatus"));

	std::string strCustomerName = GBKToUTF8(d.CustomerName);
	AddItem(strCustomerName, ("CustomerName"));

	std::string strMessage = GBKToUTF8(d.Message);
	AddItem(strMessage, ("Message"));

	std::string strDigest = GBKToUTF8(d.Digest);
	AddItem(strDigest, ("Digest"));

	std::string strLongCustomerName = GBKToUTF8(d.LongCustomerName);
	AddItem(strLongCustomerName, ("LongCustomerName"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcOrderField& d)
{
	AddItem("CThostFtdcOrderField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.InstrumentID, ("InstrumentID"));
	AddItem(d.OrderRef, ("OrderRef"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(strUserID, ("UserID"));

	AddItem(d.OrderPriceType, ("OrderPriceType"));
	AddItem(d.Direction, ("Direction"));
	AddItem(d.CombOffsetFlag, ("CombOffsetFlag"));

	AddItem(d.CombHedgeFlag, ("CombHedgeFlag"));
	AddItem(d.LimitPrice, ("LimitPrice"));
	AddItem(d.VolumeTotalOriginal, ("VolumeTotalOriginal"));
	AddItem(d.TimeCondition, ("TimeCondition"));

	AddItem(d.GTDDate, ("GTDDate"));
	AddItem(d.VolumeCondition, ("VolumeCondition"));
	AddItem(d.MinVolume, ("MinVolume"));
	AddItem(d.ContingentCondition, ("ContingentCondition"));

	AddItem(d.StopPrice, ("StopPrice"));
	AddItem(d.ForceCloseReason, ("ForceCloseReason"));
	AddItem(d.IsAutoSuspend, ("IsAutoSuspend"));

	std::string strBusinessUnit = GBKToUTF8(d.BusinessUnit);
	AddItem(strBusinessUnit, ("BusinessUnit"));

	AddItem(d.RequestID, ("RequestID"));
	AddItem(d.OrderLocalID, ("OrderLocalID"));
	AddItem(d.ExchangeID, ("ExchangeID"));

	std::string strParticipantID = GBKToUTF8(d.ParticipantID);
	AddItem(strParticipantID, ("ParticipantID"));

	std::string strClientID = GBKToUTF8(d.ClientID);
	AddItem(strClientID, ("ClientID"));

	AddItem(d.ExchangeInstID, ("ExchangeInstID"));
	AddItem(d.TraderID, ("TraderID"));
	AddItem(d.InstallID, ("InstallID"));

	AddItem(d.OrderSubmitStatus, ("OrderSubmitStatus"));
	AddItem(d.NotifySequence, ("NotifySequence"));
	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.SettlementID, ("SettlementID"));

	AddItem(d.OrderSysID, ("OrderSysID"));
	AddItem(d.OrderSource, ("OrderSource"));
	AddItem(d.OrderStatus, ("OrderStatus"));
	AddItem(d.OrderType, ("OrderType"));

	AddItem(d.VolumeTraded, ("VolumeTraded"));
	AddItem(d.VolumeTotal, ("VolumeTotal"));
	AddItem(d.InsertDate, ("InsertDate"));
	AddItem(d.InsertTime, ("InsertTime"));

	AddItem(d.ActiveTime, ("ActiveTime"));
	AddItem(d.SuspendTime, ("SuspendTime"));
	AddItem(d.UpdateTime, ("UpdateTime"));
	AddItem(d.CancelTime, ("CancelTime"));

	std::string strActiveTraderID = GBKToUTF8(d.ActiveTraderID);
	AddItem(strActiveTraderID, ("ActiveTraderID"));

	std::string strClearingPartID = GBKToUTF8(d.ClearingPartID);
	AddItem(strClearingPartID, ("ClearingPartID"));

	AddItem(d.SequenceNo, ("SequenceNo"));
	AddItem(d.FrontID, ("FrontID"));

	AddItem(d.SessionID, ("SessionID"));
	AddItem(d.UserProductInfo, ("UserProductInfo"));
	AddItem(d.UserForceClose, ("UserForceClose"));

	std::string strActiveUserID = GBKToUTF8(d.ActiveUserID);
	AddItem(strActiveUserID, ("ActiveUserID"));

	AddItem(d.BrokerOrderSeq, ("BrokerOrderSeq"));
	AddItem(d.RelativeOrderSysID, ("RelativeOrderSysID"));
	AddItem(d.ZCETotalTradedVolume, ("ZCETotalTradedVolume"));
	AddItem(d.IsSwapOrder, ("IsSwapOrder"));

	std::string strBranchID = GBKToUTF8(d.BranchID);
	AddItem(d.BranchID, ("BranchID"));

	std::string strInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(strInvestUnitID, ("InvestUnitID"));

	std::string strAccountID = GBKToUTF8(d.AccountID);
	AddItem(strAccountID, ("AccountID"));

	std::string strCurrencyID = GBKToUTF8(d.CurrencyID);
	AddItem(strCurrencyID, ("CurrencyID"));

	std::string strIPAddress = GBKToUTF8(d.IPAddress);
	AddItem(strIPAddress, ("IPAddress"));

	std::string strMacAddress = GBKToUTF8(d.MacAddress);
	AddItem(strMacAddress, ("MacAddress"));

	std::string strStatusMsg = GBKToUTF8(d.StatusMsg);
	AddItem(strStatusMsg, ("StatusMsg"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcTradeField& d)
{
	AddItem("CThostFtdcTradeField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.InstrumentID, ("InstrumentID"));
	AddItem(d.OrderRef, ("OrderRef"));

	std::string strUserID = GBKToUTF8(d.UserID);
	AddItem(d.UserID, ("UserID"));

	AddItem(d.ExchangeID, ("ExchangeID"));
	AddItem(d.TradeID, ("TradeID"));
	AddItem(d.Direction, ("Direction"));

	AddItem(d.OrderSysID, ("OrderSysID"));

	std::string strParticipantID = GBKToUTF8(d.ParticipantID);
	AddItem(strParticipantID, ("ParticipantID"));

	std::string strClientID = GBKToUTF8(d.ClientID);
	AddItem(strClientID, ("ClientID"));

	AddItem(d.TradingRole, ("TradingRole"));

	AddItem(d.ExchangeInstID, ("ExchangeInstID"));
	AddItem(d.OffsetFlag, ("OffsetFlag"));
	AddItem(d.HedgeFlag, ("HedgeFlag"));
	AddItem(d.Price, ("Price"));

	AddItem(d.Volume, ("Volume"));
	AddItem(d.TradeDate, ("TradeDate"));
	AddItem(d.TradeTime, ("TradeTime"));
	AddItem(d.TradeType, ("TradeType"));

	AddItem(d.PriceSource, ("PriceSource"));
	AddItem(d.TraderID, ("TraderID"));
	AddItem(d.OrderLocalID, ("OrderLocalID"));

	std::string strClearingPartID = GBKToUTF8(d.ClearingPartID);
	AddItem(strClearingPartID, ("ClearingPartID"));

	std::string strBusinessUnit = GBKToUTF8(d.BusinessUnit);
	AddItem(strBusinessUnit, ("BusinessUnit"));

	AddItem(d.SequenceNo, ("SequenceNo"));
	AddItem(d.TradingDay, ("TradingDay"));
	AddItem(d.SettlementID, ("SettlementID"));

	AddItem(d.BrokerOrderSeq, ("BrokerOrderSeq"));


	AddItem(d.TradeSource, ("TradeSource"));

	std::string stInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(stInvestUnitID, ("InvestUnitID"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcTradingNoticeInfoField& d)
{
	AddItem("CThostFtdcTradingNoticeInfoField", ("aid"));

	std::string strBrokerID = GBKToUTF8(d.BrokerID);
	AddItem(strBrokerID, ("BrokerID"));

	std::string strInvestorID = GBKToUTF8(d.InvestorID);
	AddItem(strInvestorID, ("InvestorID"));

	AddItem(d.SendTime, ("SendTime"));
	AddItem(d.SequenceSeries, ("SequenceSeries"));

	AddItem(d.SequenceNo, ("SequenceNo"));

	std::string stInvestUnitID = GBKToUTF8(d.InvestUnitID);
	AddItem(stInvestUnitID, ("InvestUnitID"));

	std::string strContent = GBKToUTF8(d.FieldContent);
	AddItem(strContent, ("FieldContent"));
}

void SerializerLogCtp::DefineStruct(CThostFtdcInstrumentStatusField& d)
{
	AddItem("CThostFtdcInstrumentStatusField", ("aid"));

	AddItem(d.ExchangeID, ("ExchangeID"));
	AddItem(d.ExchangeInstID, ("ExchangeInstID"));
	std::string strSettlementGroupID = GBKToUTF8(d.SettlementGroupID);
	AddItem(strSettlementGroupID, ("SettlementGroupID"));
	AddItem(d.InstrumentID, ("InstrumentID"));
	AddItem(d.InstrumentStatus, ("InstrumentStatus"));
	AddItem(d.TradingSegmentSN, ("TradingSegmentSN"));
	AddItem(d.EnterTime, ("EnterTime"));
	AddItem(d.EnterReason, ("EnterReason"));
}

ctp_condition_order_task::ctp_condition_order_task()
	: has_order_to_cancel(false)
	, orders_to_cancel()
	, has_first_orders_to_send(false)
	, first_orders_to_send()
	, has_second_orders_to_send(false)
	, second_orders_to_send()
	, condition_order()
{
}
