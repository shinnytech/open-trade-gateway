/////////////////////////////////////////////////////////////////////////
///@file tradectp3.cpp
///@brief	CTP交易逻辑实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "tradectp.h"
#include "config.h"
#include "ctp_define.h"
#include "utility.h"
#include "SerializerTradeBase.h"
#include "ins_list.h"
#include "numset.h"

using namespace trader_dll;

void traderctp::LoadFromFile()
{
	if (m_user_file_path.empty())
	{
		return;
	}		
	std::string fn = m_user_file_path + "/" + _req_login.broker.ctp_broker_id + "_" + _req_login.user_name;
	SerializerCtp s;
	if (s.FromFile(fn.c_str())) 
	{
		OrderKeyFile kf;
		s.ToVar(kf);
		for (auto it = kf.items.begin(); it != kf.items.end(); ++it)
		{
			m_ordermap_local_remote[it->local_key] = it->remote_key;
			m_ordermap_remote_local[it->remote_key] = it->local_key;
		}
		m_trading_day = kf.trading_day;
	}
}

void traderctp::SaveToFile()
{
	if (m_user_file_path.empty())
	{
		return;
	}

	SerializerCtp s;
	OrderKeyFile kf;
	kf.trading_day = m_trading_day;

	for (auto it = m_ordermap_local_remote.begin();
		it != m_ordermap_local_remote.end(); ++it)
	{
		OrderKeyPair item;
		item.local_key = it->first;
		item.remote_key = it->second;
		kf.items.push_back(item);
	}
	s.FromVar(kf);	
	std::string fn = m_user_file_path + "/" + _req_login.broker.ctp_broker_id + "_" + _req_login.user_name;
	s.ToFile(fn.c_str());	
}

using namespace std::chrono;

bool traderctp::NeedReset()
{
	if (m_req_login_dt == 0)
		return false;
	long long now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	if (now > m_req_login_dt + 60)
		return true;
	return false;
};

void traderctp::OnIdle()
{
	if (m_need_save_file.load())
	{
		this->SaveToFile();
		m_need_save_file.store(false);
	}

	//有空的时候, 标记为需查询的项, 如果离上次查询时间够远, 应该发起查询
	long long now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	if (m_peeking_message && (m_next_send_dt < now))
	{
		m_next_send_dt = now + 100;		
		SendUserData();
	}

	if (m_next_qry_dt >= now)
	{
		return;
	}

	if (!m_b_login)
	{
		return;
	}
		
	if (m_req_position_id > m_rsp_position_id) 
	{
		ReqQryPosition(m_req_position_id);
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_need_query_broker_trading_params)
	{
		ReqQryBrokerTradingParams();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_req_account_id > m_rsp_account_id) 
	{
		ReqQryAccount(m_req_account_id);
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_need_query_settlement.load()) 
	{
		ReqQrySettlementInfo();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_need_query_bank.load()) 
	{
		ReqQryBank();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_need_query_register.load()) 
	{
		ReqQryAccountRegister();
		m_next_qry_dt = now + 1100;
		return;
	}

	if (m_confirm_settlement_status.load() == 1)
	{
		ReqConfirmSettlement();
		m_next_qry_dt = now + 1100;
		return;
	}
}

int traderctp::ReqQryBrokerTradingParams()
{
	CThostFtdcQryBrokerTradingParamsField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryBrokerTradingParams(&field, 0);
	if (0 != r)
	{
		Log(LOG_INFO, NULL
			, "ctpse ReqQryBrokerTradingParams,instance=%p,bid=%s,UserID=%s, ret=%d"
			, this
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, r);
	}
	return r;
}

int traderctp::ReqQryAccount(int reqid)
{
	CThostFtdcQryTradingAccountField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryTradingAccount(&field, reqid);
	if (0 != r)
	{
		Log(LOG_INFO, NULL
			, "ctpse ReqQryTradingAccount,instance=%p,bid=%s,UserID=%s,ret=%d"
			, this
			, _req_login.bid.c_str()
			, _req_login.user_name.c_str()
			, r);
	}
	return r;
}

int traderctp::ReqQryPosition(int reqid)
{
	CThostFtdcQryInvestorPositionField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID, _req_login.user_name.c_str());
	int r = m_pTdApi->ReqQryInvestorPosition(&field, reqid);
	Log(LOG_INFO, NULL, "ctpse ReqQryInvestorPosition,instance=%p,bid=%s,UserID=%s, ret=%d"
		, this
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, r);
	return r;
}

void traderctp::ReqQryBank()
{
	CThostFtdcQryContractBankField field;
	memset(&field,0,sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	m_pTdApi->ReqQryContractBank(&field, 0);
	int r = m_pTdApi->ReqQryContractBank(&field, 0);
	Log(LOG_INFO, NULL, "ctpse ReqQryContractBank,instance=%p,bid=%s,UserID=%s, ret=%d"
		, this
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, r);
}

void traderctp::ReqQryAccountRegister()
{
	CThostFtdcQryAccountregisterField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	m_pTdApi->ReqQryAccountregister(&field, 0);
	int r = m_pTdApi->ReqQryAccountregister(&field, 0);
	Log(LOG_INFO, NULL, "ctpse ReqQryAccountregister,instance=%p,bid=%s,UserID=%s, ret=%d"
		, this
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, r);
}

void traderctp::ReqQrySettlementInfo()
{
	CThostFtdcQrySettlementInfoField field;
	memset(&field, 0, sizeof(field));
	strcpy_x(field.BrokerID, m_broker_id.c_str());
	strcpy_x(field.InvestorID,_req_login.user_name.c_str());
	strcpy_x(field.AccountID,_req_login.user_name.c_str());
	int r = m_pTdApi->ReqQrySettlementInfo(&field, 0);
	Log(LOG_INFO, NULL, "ctpse ReqQrySettlementInfo,instance=%p,bid=%s,UserID=%s,ret=%d"
		, this
		, _req_login.bid.c_str()
		, _req_login.user_name.c_str()
		, r);
}

Account& traderctp::GetAccount(const std::string account_key)
{
	return m_data.m_accounts[account_key];
}

Position& traderctp::GetPosition(const std::string symbol)
{
	Position& position = m_data.m_positions[symbol];	
	return position;
}

Bank& traderctp::GetBank(const std::string& bank_id)
{
	return m_data.m_banks[bank_id];
}

Trade& traderctp::GetTrade(const std::string trade_key)
{
	return m_data.m_trades[trade_key];
}

TransferLog& traderctp::GetTransferLog(const std::string& seq_id)
{
	return m_data.m_transfers[seq_id];
}

void traderctp::ReSendSettlementInfo(int connectId)
{
	if (m_need_query_settlement.load())
	{
		return;
	}

	if (m_confirm_settlement_status.load() != 0)
	{
		return;
	}

	OutputNotifySycn(connectId, 0, m_settlement_info, "INFO", "SETTLEMENT");
}

void traderctp::SendUserDataImd(int connectId)
{
	//重算所有持仓项的持仓盈亏和浮动盈亏
	double total_position_profit = 0;
	double total_float_profit = 0;
	for (auto it = m_data.m_positions.begin();
		it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;
		Position& ps = it->second;
		if (nullptr == ps.ins)
		{
			ps.ins = GetInstrument(symbol);
		}
		if (nullptr == ps.ins)
		{
			Log(LOG_ERROR, NULL, "ctpse miss symbol %s when processing position,\
			 instance=%p,bid=%s,UserID=%s"
				, symbol.c_str()
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
			);
			continue;
		}
		ps.volume_long = ps.volume_long_his + ps.volume_long_today;
		ps.volume_short = ps.volume_short_his + ps.volume_short_today;
		ps.volume_long_frozen = ps.volume_long_frozen_today + ps.volume_long_frozen_his;
		ps.volume_short_frozen = ps.volume_short_frozen_today + ps.volume_short_frozen_his;
		ps.margin = ps.margin_long + ps.margin_short;
		double last_price = ps.ins->last_price;
		if (!IsValid(last_price))
			last_price = ps.ins->pre_settlement;
		if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
		{
			ps.last_price = last_price;
			ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
			ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
			ps.position_profit = ps.position_profit_long + ps.position_profit_short;
			ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
			ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
			ps.float_profit = ps.float_profit_long + ps.float_profit_short;
			if (ps.volume_long > 0)
			{
				ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
				ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
			}
			if (ps.volume_short > 0)
			{
				ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
				ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
			}
			ps.changed = true;
			m_something_changed = true;
		}
		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;
		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;
	}

	//重算资金账户
	if (m_something_changed)
	{
		Account& acc = GetAccount("CNY");
		double dv = total_position_profit - acc.position_profit;
		double po_ori = 0;
		double po_curr = 0;
		double av_diff = 0;
		switch (m_Algorithm_Type)
		{
		case THOST_FTDC_AG_All:
			po_ori = acc.position_profit;
			po_curr = total_position_profit;
			break;
		case THOST_FTDC_AG_OnlyLost:
			if (acc.position_profit < 0)
			{
				po_ori = acc.position_profit;
			}
			if (total_position_profit < 0)
			{
				po_curr = total_position_profit;
			}
			break;
		case THOST_FTDC_AG_OnlyGain:
			if (acc.position_profit > 0)
			{
				po_ori = acc.position_profit;
			}
			if (total_position_profit > 0)
			{
				po_curr = total_position_profit;
			}
			break;
		case THOST_FTDC_AG_None:
			po_ori = 0;
			po_curr = 0;
			break;
		default:
			break;
		}
		av_diff = po_curr - po_ori;
		acc.position_profit = total_position_profit;
		acc.float_profit = total_float_profit;
		acc.available += av_diff;
		acc.balance += dv;
		if (IsValid(acc.available) && IsValid(acc.balance) && !IsZero(acc.balance))
			acc.risk_ratio = 1.0 - acc.available / acc.balance;
		else
			acc.risk_ratio = NAN;
		acc.changed = true;
	}

	//构建数据包		
	SerializerTradeBase nss;
	nss.dump_all = true;
	rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");
	rapidjson::Value node_data;
	nss.FromVar(m_data, &node_data);
	rapidjson::Value node_user_id;
	node_user_id.SetString(_req_login.user_name, nss.m_doc->GetAllocator());
	rapidjson::Value node_user;
	node_user.SetObject();
	node_user.AddMember(node_user_id, node_data, nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc, node_user);
	std::string json_str;
	nss.ToString(&json_str);	
	//发送	
	std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
	_ios_out.post(boost::bind(&traderctp::SendMsg,this,connectId,msg_ptr));	
}

void traderctp::SendUserData()
{
	if (!m_peeking_message)
	{
		return;
	}

	if (m_data.m_accounts.size() == 0)
		return;

	if (!m_position_ready)
		return;

	//重算所有持仓项的持仓盈亏和浮动盈亏
	double total_position_profit = 0;
	double total_float_profit = 0;
	for (auto it = m_data.m_positions.begin(); 
		it != m_data.m_positions.end(); ++it)
	{
		const std::string& symbol = it->first;		
		Position& ps = it->second;
		if (nullptr == ps.ins)
		{
			ps.ins = GetInstrument(symbol);
		}			
		if (nullptr == ps.ins)
		{
			Log(LOG_ERROR, NULL, "ctpse miss symbol %s when processing position \
				, instance=%p,bid=%s,UserID=%s"
				, symbol.c_str()
				, this
				, _req_login.bid.c_str()
				, _req_login.user_name.c_str()
			);
			continue;
		}		
		ps.volume_long = ps.volume_long_his + ps.volume_long_today;
		ps.volume_short = ps.volume_short_his + ps.volume_short_today;
		ps.volume_long_frozen = ps.volume_long_frozen_today + ps.volume_long_frozen_his;
		ps.volume_short_frozen = ps.volume_short_frozen_today + ps.volume_short_frozen_his;
		ps.margin = ps.margin_long + ps.margin_short;
		double last_price = ps.ins->last_price;
		if (!IsValid(last_price))
			last_price = ps.ins->pre_settlement;		
		if (IsValid(last_price) && (last_price != ps.last_price || ps.changed))
		{
			ps.last_price = last_price;
			ps.position_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.position_cost_long;
			ps.position_profit_short = ps.position_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
			ps.position_profit = ps.position_profit_long + ps.position_profit_short;
			ps.float_profit_long = ps.last_price * ps.volume_long * ps.ins->volume_multiple - ps.open_cost_long;
			ps.float_profit_short = ps.open_cost_short - ps.last_price * ps.volume_short * ps.ins->volume_multiple;
			ps.float_profit = ps.float_profit_long + ps.float_profit_short;
			if (ps.volume_long > 0)
			{
				ps.open_price_long = ps.open_cost_long / (ps.volume_long * ps.ins->volume_multiple);
				ps.position_price_long = ps.position_cost_long / (ps.volume_long * ps.ins->volume_multiple);
			}
			if (ps.volume_short > 0)
			{
				ps.open_price_short = ps.open_cost_short / (ps.volume_short * ps.ins->volume_multiple);
				ps.position_price_short = ps.position_cost_short / (ps.volume_short * ps.ins->volume_multiple);
			}
			ps.changed = true;
			m_something_changed = true;
		}
		if (IsValid(ps.position_profit))
			total_position_profit += ps.position_profit;
		if (IsValid(ps.float_profit))
			total_float_profit += ps.float_profit;
	}	
	//重算资金账户
	if (m_something_changed)
	{
		Account& acc = GetAccount("CNY");		
		double dv = total_position_profit - acc.position_profit;		
		double po_ori = 0;
		double po_curr = 0;
		double av_diff = 0;
		switch (m_Algorithm_Type)
		{
		case THOST_FTDC_AG_All:
			po_ori = acc.position_profit;
			po_curr = total_position_profit;
			break;
		case THOST_FTDC_AG_OnlyLost:
			if (acc.position_profit < 0)
			{
				po_ori = acc.position_profit;
			}
			if (total_position_profit < 0)
			{
				po_curr = total_position_profit;
			}
			break;
		case THOST_FTDC_AG_OnlyGain:
			if (acc.position_profit > 0)
			{
				po_ori = acc.position_profit;
			}
			if (total_position_profit > 0)
			{
				po_curr = total_position_profit;
			}
			break;
		case THOST_FTDC_AG_None:
			po_ori = 0;
			po_curr = 0;
			break;
		default:
			break;
		}
		av_diff = po_curr - po_ori;
		acc.position_profit = total_position_profit;
		acc.float_profit = total_float_profit;
		acc.available += av_diff;
		acc.balance += dv;
		if (IsValid(acc.available) && IsValid(acc.balance) && !IsZero(acc.balance))
			acc.risk_ratio = 1.0 - acc.available / acc.balance;
		else
			acc.risk_ratio = NAN;
		acc.changed = true;
	}	
	if (!m_something_changed)
		return;
	//构建数据包	
	m_data.m_trade_more_data = false;
	SerializerTradeBase nss;
	rapidjson::Pointer("/aid").Set(*nss.m_doc, "rtn_data");
	rapidjson::Value node_data;
	nss.FromVar(m_data, &node_data);
	rapidjson::Value node_user_id;
	node_user_id.SetString(_req_login.user_name, nss.m_doc->GetAllocator());
	rapidjson::Value node_user;
	node_user.SetObject();
	node_user.AddMember(node_user_id, node_data, nss.m_doc->GetAllocator());
	rapidjson::Pointer("/data/0/trade").Set(*nss.m_doc, node_user);	
	std::string json_str;
	nss.ToString(&json_str);
	//发送		
	std::string str = GetConnectionStr();
	if (!str.empty())
	{
		std::shared_ptr<std::string> msg_ptr(new std::string(json_str));
		std::shared_ptr<std::string> conn_ptr(new std::string(str));
		_ios_out.post(boost::bind(&traderctp::SendMsgAll, this, conn_ptr, msg_ptr));
	}	
	m_something_changed = false;
	m_peeking_message = false;
}

void traderctp::AfterLogin()
{
	if (g_config.auto_confirm_settlement)
	{
		if (0 == m_confirm_settlement_status.load())
		{
			m_confirm_settlement_status.store(1);
		}
		ReqConfirmSettlement();
	}
	else if (m_settlement_info.empty())
	{
		ReqQrySettlementInfoConfirm();
	}
	m_req_position_id++;
	m_req_account_id++;
	m_need_query_bank.store(true);
	m_need_query_register.store(true);
	m_need_query_broker_trading_params.store(true);
}