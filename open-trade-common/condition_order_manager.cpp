#include "condition_order_manager.h"
#include "condition_order_serializer.h"
#include "config.h"
#include "utility.h"
#include "ins_list.h"
#include "numset.h"
#include "datetime.h"

#include <chrono>
#include <cmath>
#include <boost/date_time.hpp>

using namespace std;

const int MAX_NEW_CONDITION_ORDER_COUNT_PER_DAY = 20;

const int MAX_VALID_CONDITION_ORDER_COUNT_ALL = 50;

ConditionOrderManager::ConditionOrderManager(const std::string& userKey
	, ConditionOrderData& condition_order_data
	, ConditionOrderHisData& condition_order_his_data
	, IConditionOrderCallBack& callBack)
	:m_userKey(userKey)
	,m_user_file_path("")
	,m_condition_order_data(condition_order_data)
	,m_current_day_condition_order_count(0)
	,m_current_valid_condition_order_count(0)
	,m_condition_order_his_data(condition_order_his_data)
	,m_callBack(callBack)
	,m_run_server(true)
	,m_openmarket_condition_order_map()
	, m_time_condition_order_set()
	,m_price_condition_order_map()
	,m_localTime(0)
	,m_SHFETime(0)
	,m_DCETime(0)
	,m_INETime(0)
	,m_FFEXTime(0)
	,m_CZCETime(0)
{	
	LoadConditionOrderConfig();
}

void ConditionOrderManager::LoadConditionOrderConfig()
{
	condition_order_config tmp_co_config;
	SerializerConditionOrderData ss;
	if (!ss.FromFile("/etc/open-trade-gateway/config-condition-order.json"))
	{
		Log().WithField("fun","LoadConditionOrderConfig")
			.WithField("fileName","/etc/open-trade-gateway/config-condition-order.json")
			.Log(LOG_INFO, "load condition order config  json file fail!");
		return;
	}

	ss.ToVar(tmp_co_config);
	m_run_server = tmp_co_config.run_server;
}

ConditionOrderManager::~ConditionOrderManager()
{
}

void ConditionOrderManager::NotifyPasswordUpdate(const std::string& strOldPassword,
	const std::string& strNewPassword)
{
	if ((strOldPassword == m_condition_order_data.user_password) 
		&& (strNewPassword!= m_condition_order_data.user_password))
	{
		m_condition_order_data.user_password = strNewPassword;
		SaveCurrent();
	}

	if ((strOldPassword == m_condition_order_his_data.user_password)
		&& (strNewPassword != m_condition_order_his_data.user_password))
	{
		m_condition_order_his_data.user_password = strNewPassword;
		SaveHistory();
	}
}

void ConditionOrderManager::Load(const std::string& bid
	, const std::string& user_id
	, const std::string& user_password
	, const std::string& trading_day)
{
	m_condition_order_data.condition_orders.clear();
	m_condition_order_his_data.his_condition_orders.clear();

	if (!g_config.user_file_path.empty())
	{
		m_user_file_path = g_config.user_file_path + "/" + bid;
	}
	else
	{
		m_user_file_path = "/var/local/lib/open-trade-gateway/" + bid;
	}
		
	//加载条件单数据
	std::vector<ConditionOrder> tmp_his_condition_orders;
	std::string fn = m_user_file_path + "/" + m_userKey +".co";
	SerializerConditionOrderData nss;
	bool loadfile = nss.FromFile(fn.c_str());
	if (!loadfile)
	{
		Log().WithField("fun","Load")
			.WithField("key",m_userKey)
			.WithField("bid",bid)
			.WithField("user_name",user_id)
			.WithField("fileName",fn)
			.Log(LOG_INFO, "ConditionOrderManager load condition order data file failed!");			
		
		m_condition_order_data.broker_id = bid;
		m_condition_order_data.user_id = user_id;
		m_condition_order_data.user_password = user_password;
		m_condition_order_data.trading_day = trading_day;
		m_current_valid_condition_order_count = 0;
		m_current_day_condition_order_count = 0;
	}
	else
	{
		Log().WithField("fun","Load")
			.WithField("key",m_userKey)
			.WithField("bid",bid)
			.WithField("user_name",user_id)
			.WithField("fileName",fn)
			.Log(LOG_INFO, "ConditionOrderManager load condition order data file success!");

		nss.ToVar(m_condition_order_data);

		m_condition_order_data.broker_id = bid;
		m_condition_order_data.user_id = user_id;		
		m_condition_order_data.user_password = user_password;	
		//如果不是同一个交易日,需要对条件单数据进行调整
		if (m_condition_order_data.trading_day != trading_day)
		{
			for (auto it = m_condition_order_data.condition_orders.begin();
				it != m_condition_order_data.condition_orders.end();)
			{
				ConditionOrder& order = it->second;

				//校验触发条件的合约状态
				for (auto& cond : order.condition_list)
				{
					std::string strSymbol = cond.exchange_id + "." + cond.instrument_id;
					Instrument* inst=GetInstrument(strSymbol);
					if ((nullptr == inst) || (inst->expired))
					{
						order.status = EConditionOrderStatus::discard;
						break;
					}
				}

				//校验订单的合约状态
				for (auto& o : order.order_list)
				{
					std::string strSymbol = o.exchange_id + "." + o.instrument_id;
					Instrument* inst = GetInstrument(strSymbol);
					if ((nullptr == inst) || (inst->expired))
					{
						order.status = EConditionOrderStatus::discard;
						break;
					}
				}
								
				//如果已经撤消或者触发或者废弃
				if ((order.status == EConditionOrderStatus::cancel)
					|| (order.status == EConditionOrderStatus::touched)
					|| (order.status == EConditionOrderStatus::discard)
					)
				{
					//如果不是当天新增的条件单
					if (order.trading_day != atoi(trading_day.c_str()))
					{
						//放入历史条件单列表
						tmp_his_condition_orders.push_back(order);
						//从当前条件单列表中删除
						m_condition_order_data.condition_orders.erase(it++);
						continue;
					}					
				}

				//如果条件单只是当日有效
				if (order.time_condition_type == ETimeConditionType::GFD)
				{
					//如果不是当天新增的条件单
					if (order.trading_day != atoi(trading_day.c_str()))
					{
						//放入历史条件单列表
						tmp_his_condition_orders.push_back(order);
						//从当前条件单列表中删除
						m_condition_order_data.condition_orders.erase(it++);
						continue;
					}					
				}

				//如果条件单指定日前有效
				if (order.time_condition_type == ETimeConditionType::GTD)
				{
					//有效日期小于当前交易日
					if (order.GTD_date < atoi(trading_day.c_str()))
					{
						//放入历史条件单列表
						tmp_his_condition_orders.push_back(order);
						//从当前条件单列表中删除
						m_condition_order_data.condition_orders.erase(it++);
						continue;
					}
				}	

				it++;
			}

			m_condition_order_data.trading_day = trading_day;
		}
		
		//计算当天的新增的条件单和现在有效的条件单
		m_current_valid_condition_order_count = 0;
		m_current_day_condition_order_count = 0;
		for (auto it = m_condition_order_data.condition_orders.begin();
			it != m_condition_order_data.condition_orders.end();)
		{
			ConditionOrder& order = it->second;

			order.changed = true;

			if (order.trading_day == atoi(trading_day.c_str()))
			{
				m_current_day_condition_order_count++;
			}

			m_current_valid_condition_order_count++;

			it++;
		}

	}
	
	//加载条件单历史数据
	fn = m_user_file_path + "/" + m_userKey + ".coh";
	SerializerConditionOrderData nss_his;
	loadfile = nss_his.FromFile(fn.c_str());
	if (!loadfile)
	{
		Log().WithField("fun","Load")
			.WithField("key",m_userKey)
			.WithField("bid",bid)
			.WithField("user_name",user_id)
			.WithField("fileName",fn)
			.Log(LOG_INFO, "ConditionOrderManager load history condition order file failed!");
		
		m_condition_order_his_data.broker_id = bid;
		m_condition_order_his_data.user_id = user_id;
		m_condition_order_his_data.user_password = user_password;
		m_condition_order_his_data.trading_day = trading_day;
		//放入历史条件单
		if (!tmp_his_condition_orders.empty())
		{
			m_condition_order_his_data.his_condition_orders.assign(
				tmp_his_condition_orders.begin()
				, tmp_his_condition_orders.end());
		}		
	}
	else
	{
		Log().WithField("fun","Load")
			.WithField("key",m_userKey)
			.WithField("bid",bid)
			.WithField("user_name",user_id)
			.WithField("fileName",fn)
			.Log(LOG_INFO, "ConditionOrderManager load history condition order file success!");
		
		nss_his.ToVar(m_condition_order_his_data);
		m_condition_order_his_data.broker_id = bid;
		m_condition_order_his_data.user_id = user_id;
		m_condition_order_his_data.user_password = user_password;
		if (m_condition_order_his_data.trading_day != trading_day)
		{
			for (auto it = m_condition_order_his_data.his_condition_orders.begin();
				it != m_condition_order_his_data.his_condition_orders.end();)
			{
				ConditionOrder& order = *it;
				long long currentTime = GetLocalEpochNano();
				if (currentTime > order.insert_date_time)
				{
					//时间差,单位:秒 
					long long timeSpan = (currentTime - order.insert_date_time) % 1000000000LL;
					//历史条件单最多保存30天(自然日)
					long long thirty_days = 30 * 24 * 60 * 60;
					if (timeSpan > thirty_days)
					{
						it=m_condition_order_his_data.his_condition_orders.erase(it);
						continue;
					}
				}
				it++;				
			}

			//放入历史条件单
			if (!tmp_his_condition_orders.empty())
			{
				m_condition_order_his_data.his_condition_orders.assign(
					tmp_his_condition_orders.begin()
					, tmp_his_condition_orders.end());
			}

			m_condition_order_his_data.trading_day = trading_day;
		}
	}

	SaveCurrent();

	SaveHistory();
	
	BuildConditionOrderIndex();

	m_callBack.OnUserDataChange();
}

void ConditionOrderManager::BuildConditionOrderIndex()
{
	m_openmarket_condition_order_map.clear();
	m_time_condition_order_set.clear();
	m_price_condition_order_map.clear();

	for (auto it : m_condition_order_data.condition_orders)
	{
		const std::string& order_id = it.first;
		ConditionOrder& co = it.second;
		if (co.status != EConditionOrderStatus::live)
		{
			continue;
		}

		for (auto& c : co.condition_list)
		{
			if (c.is_touched)
			{
				continue;
			}

			if (c.contingent_type == EContingentType::market_open)
			{
				std::string strInstId = c.instrument_id;
				CutDigital(strInstId);
				std::string strSymbol = c.exchange_id + "." + strInstId;
				std::vector<std::string>& orderIdList = m_openmarket_condition_order_map[strSymbol];
				orderIdList.push_back(order_id);
			}
			else if (c.contingent_type == EContingentType::time)
			{
				if (m_time_condition_order_set.find(order_id) == m_time_condition_order_set.end())
				{
					m_time_condition_order_set.insert(order_id);
				}				
			}
			else
			{
				std::string strSymbol = c.exchange_id + "." + c.instrument_id;
				std::vector<std::string>& orderIdList = m_price_condition_order_map[strSymbol];
				orderIdList.push_back(order_id);
			}
		}

	}	
}

void ConditionOrderManager::SaveCurrent()
{
	//保存条件单数据
	std::string fn = m_user_file_path + "/" + m_userKey + ".co";

	Log().WithField("fun","SaveCurrent")
		.WithField("key",m_userKey)
		.WithField("bid",m_condition_order_data.broker_id)
		.WithField("user_name", m_condition_order_data.user_id)
		.WithField("fileName", fn)
		.Log(LOG_INFO, "try to save current condition order file!");
	
	SerializerConditionOrderData nss;
	nss.dump_all = true;
	nss.FromVar(m_condition_order_data);
	bool saveFile = nss.ToFile(fn.c_str());
	if (!saveFile)
	{
		Log().WithField("fun","SaveCurrent")
			.WithField("key",m_userKey)
			.WithField("bid",m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.WithField("fileName",fn)
			.Log(LOG_INFO, "save condition order data file failed!");	
	}
}

void ConditionOrderManager::SaveHistory()
{
	//保存条件单历史数据
	std::string fn = m_user_file_path + "/" + m_userKey + ".coh";
	SerializerConditionOrderData nss_his;
	nss_his.dump_all = true;
	nss_his.FromVar(m_condition_order_his_data);
	bool saveFile = nss_his.ToFile(fn.c_str());
	if (!saveFile)
	{
		Log().WithField("fun","SaveHistory")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.WithField("fileName", fn)
			.Log(LOG_INFO, "save history condition order data file failed!");		
	}
}

bool ConditionOrderManager::ValidConditionOrder(const ConditionOrder& order)
{
	//检验条件	
	for (auto& cond : order.condition_list)
	{
		std::string symbol = cond.exchange_id + "." + cond.instrument_id;
		Instrument* ins = GetInstrument(symbol);
		if (nullptr == ins)
		{
			m_callBack.OutputNotifyAll(
				1
				, u8"条件单已被服务器拒绝,条件单触发条件中的合约ID不存在:"+ symbol
				, "WARNING","MESSAGE");
			return false;
		}

		if (cond.contingent_type == EContingentType::time)
		{			
			if (cond.contingent_time <= GetExchangeTime(cond.exchange_id))
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,时间触发条件指定的触发时间小于当前时间" 
					, "WARNING", "MESSAGE");
				return false;
			}
		}
		else if (cond.contingent_type == EContingentType::price)
		{
			if (!IsValid(cond.contingent_price)
				||IsZero(cond.contingent_price))
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,价格触发条件指定的触发价格不合法"
					, "WARNING", "MESSAGE");
				return false;
			}
		}
		else if (cond.contingent_type == EContingentType::price_range)
		{
			if (!IsValid(cond.contingent_price_left)
				||IsZero(cond.contingent_price_left)
				||!IsValid(cond.contingent_price_right)
				||IsZero(cond.contingent_price_right)
				|| (cond.contingent_price_left> cond.contingent_price_right)
				)
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,价格区间触发条件指定的价格区间不合法"
					, "WARNING", "MESSAGE");
				return false;
			}
		}
		else if (cond.contingent_type == EContingentType::break_even)
		{
			if (!IsValid(cond.break_even_price)
				||IsZero(cond.break_even_price)
				)
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,固定价格止盈触发条件指定的固定价格不合法"
					, "WARNING", "MESSAGE");
				return false;
			}
		}

	}

	//检验订单
	for (auto& co : order.order_list)
	{
		std::string symbol = co.exchange_id + "." + co.instrument_id;
		Instrument* ins = GetInstrument(symbol);
		if (nullptr == ins)
		{
			m_callBack.OutputNotifyAll(
				1
				, u8"条件单已被服务器拒绝,条件单触发的订单列表中的合约ID不存在:" + symbol
				, "WARNING", "MESSAGE");
			return false;
		}

		if (co.volume_type == EVolumeType::num)
		{
			if (co.volume <= 0)
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,条件单触发的订单手数设置不合法"
					, "WARNING", "MESSAGE");
				return false;
			}
		}

		if (co.price_type == EPriceType::limit)
		{
			if (!IsValid(co.limit_price)
				||IsZero(co.limit_price)
				)
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,条件单触发的订单价格设置不合法"
					, "WARNING", "MESSAGE");
				return false;
			}
		}

		if (co.price_type == EPriceType::contingent)
		{
			bool flag = false;
			for (auto cond : order.condition_list)
			{
				if (cond.contingent_type != EContingentType::price)
				{
					continue;
				}

				std::string symbol2 = cond.exchange_id + "." + cond.instrument_id;
				if (symbol2 == symbol)
				{
					if (IsValid(cond.contingent_price)
						&&!IsZero(cond.contingent_price)
						)
					{
						flag = true;
						break;
					}					
				}

			}

			if (!flag)
			{
				m_callBack.OutputNotifyAll(
					1
					, u8"条件单已被服务器拒绝,条件单触发的订单价格设置不合法"
					, "WARNING", "MESSAGE");
				return false;
			}
		}
	}

	if (order.time_condition_type == ETimeConditionType::GTD)
	{
		if (order.GTD_date < order.trading_day)
		{
			m_callBack.OutputNotifyAll(
				1
				, u8"条件单已被服务器拒绝,条件单有效日期设置不合法"
				, "WARNING", "MESSAGE");
			return false;
		}
	}

	if (m_current_day_condition_order_count + 1 > MAX_NEW_CONDITION_ORDER_COUNT_PER_DAY)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单已被服务器拒绝,当前交易日新增条件单数量超过最大数量限制:"+std::to_string(MAX_NEW_CONDITION_ORDER_COUNT_PER_DAY)
			, "WARNING", "MESSAGE");
		return false;
	}


	if (m_current_valid_condition_order_count + 1 > MAX_VALID_CONDITION_ORDER_COUNT_ALL)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单已被服务器拒绝,当前有效条件单数量超过最大数量限制:" + std::to_string(MAX_VALID_CONDITION_ORDER_COUNT_ALL)
			, "WARNING", "MESSAGE");
		return false;
	}
	
	return true;	
}

void ConditionOrderManager::InsertConditionOrder(const std::string& msg)
{
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun","InsertConditionOrder")
			.WithField("key",m_userKey)
			.WithField("bid",m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "not invalid InsertConditionOrder msg!");		
		return;
	}
	
	if (!m_run_server)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单已被服务器拒绝,原因:条件单服务器已经暂时停止运行"
			, "WARNING", "MESSAGE");
		return;
	}

	req_insert_condition_order insert_co;
	nss.ToVar(insert_co);

	if (insert_co.order_id.empty())
	{
		insert_co.order_id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	std::string order_key = insert_co.order_id;
	auto it = m_condition_order_data.condition_orders.find(order_key);
	if (it != m_condition_order_data.condition_orders.end())
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单已被服务器拒绝,原因:单号重复"
			, "WARNING","MESSAGE");
		return;
	}

	if (insert_co.user_id.substr(0, m_condition_order_data.user_id.size())
		!= m_condition_order_data.user_id)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单已被服务器拒绝,原因:下单指令中的用户名错误"
			, "WARNING", "MESSAGE");
		return;
	}
		
	ConditionOrder order;
	order.order_id = insert_co.order_id;
	order.trading_day = atoi(m_condition_order_data.trading_day.c_str());
	order.insert_date_time = GetLocalEpochNano();
	order.condition_list.assign(insert_co.condition_list.begin(),
		insert_co.condition_list.end());
	order.conditions_logic_oper = insert_co.conditions_logic_oper;
	order.order_list.assign(insert_co.order_list.begin(),
		insert_co.order_list.end());
	order.time_condition_type = insert_co.time_condition_type;
	order.GTD_date = insert_co.GTD_date;
	order.is_cancel_ori_close_order = insert_co.is_cancel_ori_close_order;
	
	if (ValidConditionOrder(order))
	{
		order.status = EConditionOrderStatus::live;
		order.changed = true;
		m_condition_order_data.condition_orders.insert(
			std::map<std::string, ConditionOrder>::value_type(order.order_id
				,order));
		m_current_day_condition_order_count++;
		m_current_valid_condition_order_count++;
		m_callBack.OutputNotifyAll(0, u8"条件单下单成功","INFO","MESSAGE");
		
		Log().WithField("fun","InsertConditionOrder")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "insert condition order ok");

		SaveCurrent();

		BuildConditionOrderIndex();
	}
	else
	{
		order.status = EConditionOrderStatus::discard;
		order.changed = true;
		m_condition_order_data.condition_orders.insert(
			std::map<std::string,ConditionOrder>::value_type(order.order_id
			,order));

		Log().WithField("fun", "InsertConditionOrder")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "insert condition order fail");

		SaveCurrent();
	}

	m_callBack.OnUserDataChange();
}

void ConditionOrderManager::CancelConditionOrder(const std::string& msg)
{
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "CancelConditionOrder")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "not invalid CancelConditionOrder msg!");		
		return;
	}

	if (!m_run_server)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单撤单请求已被服务器拒绝,原因:条件单服务器已经暂时停止运行"
			, "WARNING", "MESSAGE");
		return;
	}

	req_cancel_condition_order cancel_co;
	nss.ToVar(cancel_co);

	if (cancel_co.user_id.substr(0, m_condition_order_data.user_id.size())
		!= m_condition_order_data.user_id)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单撤单请求已被服务器拒绝,原因:撤单请求中的用户名错误"
			, "WARNING", "MESSAGE");
		return;
	}

	std::string order_key = cancel_co.order_id;
	auto it = m_condition_order_data.condition_orders.find(order_key);
	if (it == m_condition_order_data.condition_orders.end())
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单撤单请求已被服务器拒绝,原因:单号不存在"
			, "WARNING", "MESSAGE");
		return;
	}

	ConditionOrder& order = it->second;
	if (order.status == EConditionOrderStatus::touched)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单撤单请求已被服务器拒绝,原因:条件单已触发"
			, "WARNING", "MESSAGE");
		return;
	}

	if (order.status == EConditionOrderStatus::cancel)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单撤单请求已被服务器拒绝,原因:条件单已撤"
			, "WARNING", "MESSAGE");
		return;
	}

	if (order.status == EConditionOrderStatus::discard)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单撤单请求已被服务器拒绝,原因:条件单是废单"
			, "WARNING", "MESSAGE");
		return;
	}

	it->second.status = EConditionOrderStatus::cancel;
	it->second.changed = true;
	m_callBack.OutputNotifyAll(0, u8"条件单撤单成功", "INFO", "MESSAGE");
	m_current_day_condition_order_count--;
	m_current_valid_condition_order_count--;
	SaveCurrent();
	BuildConditionOrderIndex();
	m_callBack.OnUserDataChange();
}

void ConditionOrderManager::PauseConditionOrder(const std::string& msg)
{
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "PauseConditionOrder")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "not invalid PauseConditionOrder msg!");		
		return;
	}

	if (!m_run_server)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:条件单服务器已经暂时停止运行"
			, "WARNING", "MESSAGE");
		return;
	}

	req_pause_condition_order pause_co;
	nss.ToVar(pause_co);

	if (pause_co.user_id.substr(0, m_condition_order_data.user_id.size())
		!= m_condition_order_data.user_id)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:暂停请求中的用户名错误"
			, "WARNING", "MESSAGE");
		return;
	}

	std::string order_key = pause_co.order_id;
	auto it = m_condition_order_data.condition_orders.find(order_key);
	if (it == m_condition_order_data.condition_orders.end())
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:单号不存在"
			, "WARNING", "MESSAGE");
		return;
	}

	ConditionOrder& order = it->second;
	if (order.status == EConditionOrderStatus::touched)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:条件单已触发"
			, "WARNING", "MESSAGE");
		return;
	}

	if (order.status == EConditionOrderStatus::cancel)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:条件单已撤"
			, "WARNING", "MESSAGE");
		return;
	}

	if (order.status == EConditionOrderStatus::discard)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:条件单是废单"
			, "WARNING", "MESSAGE");
		return;
	}

	if (order.status == EConditionOrderStatus::suspend)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单暂停请求已被服务器拒绝,原因:条件单已经暂停"
			, "WARNING", "MESSAGE");
		return;
	}

	it->second.status = EConditionOrderStatus::suspend;
	it->second.changed = true;
	m_callBack.OutputNotifyAll(0, u8"条件单暂停成功", "INFO", "MESSAGE");
	SaveCurrent();	
	BuildConditionOrderIndex();
	m_callBack.OnUserDataChange();
}

void ConditionOrderManager::ResumeConditionOrder(const std::string& msg)
{
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "ResumeConditionOrder")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "not invalid ResumeConditionOrder msg!");	
		return;
	}

	if (!m_run_server)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单恢复请求已被服务器拒绝,原因:条件单服务器已经暂时停止运行"
			, "WARNING", "MESSAGE");
		return;
	}

	req_resume_condition_order resume_co;
	nss.ToVar(resume_co);

	if (resume_co.user_id.substr(0, m_condition_order_data.user_id.size())
		!= m_condition_order_data.user_id)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单恢复请求已被服务器拒绝,原因:恢复请求中的用户名错误"
			, "WARNING", "MESSAGE");
		return;
	}

	std::string order_key = resume_co.order_id;
	auto it = m_condition_order_data.condition_orders.find(order_key);
	if (it == m_condition_order_data.condition_orders.end())
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单恢复请求已被服务器拒绝,原因:单号不存在"
			, "WARNING", "MESSAGE");
		return;
	}

	ConditionOrder& order = it->second;
	if (order.status != EConditionOrderStatus::suspend)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"条件单恢复请求已被服务器拒绝,原因:条件单不是处于暂停状态"
			, "WARNING", "MESSAGE");
		return;
	}
	
	
	it->second.status = EConditionOrderStatus::live;
	it->second.changed = true;
	m_callBack.OutputNotifyAll(0, u8"条件单恢复成功", "INFO", "MESSAGE");
	SaveCurrent();	
	BuildConditionOrderIndex();
	m_callBack.OnUserDataChange();
}

void ConditionOrderManager::ChangeCOSStatus(const std::string& msg)
{
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "ChangeCOSStatus")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "not invalid ChangeCOSStatus msg!");		
		return;
	}

	req_ccos_status req;
	nss.ToVar(req);
	m_run_server = req.run_server;
	if (!m_run_server)
	{
		Log().WithField("fun", "ChangeCOSStatus")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO, "cos is stoped!");		
	}
	else
	{
		Log().WithField("fun", "ChangeCOSStatus")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_data.broker_id)
			.WithField("user_name", m_condition_order_data.user_id)
			.Log(LOG_INFO,"cos is started!");
	}
}

TInstOrderIdListMap& ConditionOrderManager::GetOpenmarketCoMap()
{
	return m_openmarket_condition_order_map;
}

std::set<std::string>& ConditionOrderManager::GetTimeCoSet()
{
	return m_time_condition_order_set;
}

TInstOrderIdListMap& ConditionOrderManager::GetPriceCoMap()
{
	return m_price_condition_order_map;
}

void ConditionOrderManager::OnMarketOpen(const std::string& strSymbol)
{
	if (!m_run_server)
	{
		return;
	}

	TInstOrderIdListMap::iterator it = m_openmarket_condition_order_map.find(strSymbol);
	if (it == m_openmarket_condition_order_map.end())
	{
		return;
	}

	bool flag = false;
	std::vector<std::string>& orderIdList = it->second;
	for (auto orderId : orderIdList)
	{
		std::map<std::string, ConditionOrder>::iterator it 
			= m_condition_order_data.condition_orders.find(orderId);
		if (it == m_condition_order_data.condition_orders.end())
		{
			continue;
		}

		ConditionOrder& conditionOrder = it->second;
		if (conditionOrder.status != EConditionOrderStatus::live)
		{
			continue;
		}

		std::vector<ContingentCondition>& condition_list = conditionOrder.condition_list;
		for (ContingentCondition& c : condition_list)
		{
			if (c.contingent_type != EContingentType::market_open)
			{
				continue;
			}

			if (c.is_touched)
			{
				continue;
			}
			std::string strInstId= c.instrument_id;
			CutDigital(strInstId);
			std::string symbol = c.exchange_id + "." + strInstId;
			if (symbol != strSymbol)
			{
				continue;
			}

			c.is_touched = true;
			flag = true;
		}

		if (IsContingentConditionTouched(condition_list, conditionOrder.conditions_logic_oper))
		{
			conditionOrder.status = EConditionOrderStatus::touched;
			conditionOrder.changed = true;
			//发单
			m_callBack.OnTouchConditionOrder(conditionOrder);
			flag = true;
		}
		
	}

	if (flag)
	{
		SaveCurrent();		
		BuildConditionOrderIndex();
		m_callBack.OnUserDataChange();
	}
}

void ConditionOrderManager::OnCheckTime()
{
	if (!m_run_server)
	{
		return;
	}

	bool flag = false;
	for (auto orderId : m_time_condition_order_set)
	{
		std::map<std::string, ConditionOrder>::iterator it
			= m_condition_order_data.condition_orders.find(orderId);
		if (it == m_condition_order_data.condition_orders.end())
		{
			continue;
		}

		ConditionOrder& conditionOrder = it->second;
		if (conditionOrder.status != EConditionOrderStatus::live)
		{
			continue;
		}

		std::vector<ContingentCondition>& condition_list = conditionOrder.condition_list;

		for (ContingentCondition& c : condition_list)
		{
			if (c.contingent_type != EContingentType::time)
			{
				continue;
			}

			if (c.is_touched)
			{
				continue;
			}

			int exchangeTime = GetExchangeTime(c.exchange_id);

			if ((exchangeTime>=c.contingent_time)
				&&(exchangeTime< c.contingent_time+100))//时间离的比较远就不会触发,考虑到时钟的差异
			{
				c.is_touched = true;
				flag = true;
			}
		}

		if (IsContingentConditionTouched(condition_list, conditionOrder.conditions_logic_oper))
		{
			conditionOrder.status = EConditionOrderStatus::touched;
			conditionOrder.changed = true;
			//发单
			m_callBack.OnTouchConditionOrder(conditionOrder);
			flag = true;
		}
	}

	if (flag)
	{
		SaveCurrent();		
		BuildConditionOrderIndex();
		m_callBack.OnUserDataChange();
	}
}

void ConditionOrderManager::OnCheckPrice()
{
	if (!m_run_server)
	{
		return;
	}

	bool flag = false;
	for (auto it : m_price_condition_order_map)
	{
		std::string strSymbol = it.first;
		std::vector<std::string>& orderIdList = it.second;
		Instrument* ins = GetInstrument(strSymbol);
		if (nullptr==ins)
		{
			continue;
		}
		double last_price=ins->last_price;
		for (auto orderId : orderIdList)
		{
			std::map<std::string, ConditionOrder>::iterator it
				= m_condition_order_data.condition_orders.find(orderId);
			if (it == m_condition_order_data.condition_orders.end())
			{
				continue;
			}

			ConditionOrder& conditionOrder = it->second;
			if (conditionOrder.status != EConditionOrderStatus::live)
			{
				continue;
			}

			std::vector<ContingentCondition>& condition_list = conditionOrder.condition_list;

			for (ContingentCondition& c : condition_list)
			{
				if (c.is_touched)
				{
					continue;
				}

				if (c.contingent_type == EContingentType::price)
				{
					switch (c.price_relation)
					{
						case EPriceRelationType::G:
							if (isgreater(last_price, c.contingent_price))
							{
								c.is_touched = true;
								flag = true;
							}
							break;
						case EPriceRelationType::GE:
							if (isgreaterequal(last_price, c.contingent_price))
							{
								c.is_touched = true;
								flag = true;
							}
							break;
						case EPriceRelationType::L:
							if (isless(last_price, c.contingent_price))
							{
								c.is_touched = true;
								flag = true;
							}
							break;
						case EPriceRelationType::LE:
							if (islessequal(last_price, c.contingent_price))
							{
								c.is_touched = true;
								flag = true;
							}
							break;
					default:
							break;
					}
				}
				else if (c.contingent_type == EContingentType::price_range)
				{
					if (
						islessequal(last_price, c.contingent_price_right)
						&& isgreaterequal(last_price, c.contingent_price_left)
						)
					{
						c.is_touched = true;
						flag = true;
					}
				}
				else if (c.contingent_type == EContingentType::break_even)
				{
					//多头
					if (c.break_even_direction == EOrderDirection::buy)
					{
						if (c.m_has_break_event)
						{
							//又向下回到保本价
							if (islessequal(last_price, c.break_even_price))
							{
								c.is_touched = true;
								flag = true;
							}
						}
						//先向上突破止盈价
						else
						{
							if (isgreater(last_price, c.break_even_price))
							{
								c.m_has_break_event = true;
								flag = true;
							}
						}
					}
					//空头
					else if (c.break_even_direction == EOrderDirection::sell)
					{
						if (c.m_has_break_event)
						{
							//又向上回到保本价
							if (isgreaterequal(last_price, c.break_even_price))
							{
								c.is_touched = true;
								flag = true;
							}
						}
						else
						{
							//先向下突破止盈价
							if (isless(last_price, c.break_even_price))
							{
								c.m_has_break_event = true;
								flag = true;
							}
						}
					}					
				}
				else
				{
					continue;
				}
				
			}

			if (IsContingentConditionTouched(condition_list, conditionOrder.conditions_logic_oper))
			{
				conditionOrder.status = EConditionOrderStatus::touched;
				conditionOrder.changed = true;
				//发单
				m_callBack.OnTouchConditionOrder(conditionOrder);
				flag = true;
			}
		}
	}

	if (flag)
	{
		SaveCurrent();		
		BuildConditionOrderIndex();
		m_callBack.OnUserDataChange();
	}
}


bool ConditionOrderManager::IsContingentConditionTouched(std::vector<ContingentCondition>& condition_list
	,ELogicOperator logicOperator)
{
	if (condition_list.size() == 0)
	{
		return false;
	}
	else if (condition_list.size() == 1)
	{
		return condition_list[0].is_touched;
	}

	if (logicOperator == ELogicOperator::logic_and)
	{
		bool flag = true;
		for (const ContingentCondition& c : condition_list)
		{
			flag = flag && c.is_touched;
		}
		return flag;
	}
	else if (logicOperator == ELogicOperator::logic_or)
	{
		bool flag = false;
		for (const ContingentCondition& c : condition_list)
		{
			flag = flag || c.is_touched;
			if (flag)
			{
				break;
			}
		}
		return flag;
	}
	else
	{
		return false;
	}	
}

void  ConditionOrderManager::SetExchangeTime(int localTime, int SHFETime, int DCETime
	, int INETime, int FFEXTime, int CZCETime)
{
	m_localTime = localTime;
	m_SHFETime = SHFETime;
	m_DCETime = DCETime;
	m_INETime = INETime;
	m_FFEXTime = FFEXTime;
	m_CZCETime = CZCETime;	
}

int ConditionOrderManager::GetExchangeTime(const std::string& exchange_id)
{
	boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
	DateTime dtLocalTime;
	dtLocalTime.date.year = tm.date().year();
	dtLocalTime.date.month = tm.date().month();
	dtLocalTime.date.day = tm.date().day();
	dtLocalTime.time.hour = tm.time_of_day().hours();
	dtLocalTime.time.minute = tm.time_of_day().minutes();
	dtLocalTime.time.second = tm.time_of_day().seconds();
	dtLocalTime.time.microsecond = 0;

	int nLocalTime = DateTimeToEpochSeconds(dtLocalTime);
	int nTimeDelta = nLocalTime - m_localTime;
	if (exchange_id == "SHFE")
	{
		return m_SHFETime + nTimeDelta;
	}
	else if (exchange_id == "INE")
	{
		return m_INETime + nTimeDelta;
	}
	else if (exchange_id == "CZCE")
	{
		return m_CZCETime + nTimeDelta;
	}
	else if (exchange_id == "DCE")
	{
		return m_DCETime + nTimeDelta;
	}
	else
	{
		return m_FFEXTime+ nTimeDelta;
	}
}

void ConditionOrderManager::QryHisConditionOrder(int connId,const std::string& msg)
{
	SerializerConditionOrderData nss;
	if (!nss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "QryHisConditionOrder")
			.WithField("key", m_userKey)
			.WithField("bid", m_condition_order_his_data.broker_id)
			.WithField("user_name", m_condition_order_his_data.user_id)
			.Log(LOG_INFO, "not invalid QryHisConditionOrder msg!");		
		return;
	}

	if (!m_run_server)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"历史条件单查询请求已被服务器拒绝,原因:条件单服务器已经暂时停止运行"
			, "WARNING", "MESSAGE");
		return;
	}

	qry_histroy_condition_order qry_his_co;
	nss.ToVar(qry_his_co);

	if (qry_his_co.user_id.substr(0, m_condition_order_his_data.user_id.size())
		!= m_condition_order_his_data.user_id)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"历史条件单查询请求已被服务器拒绝,原因:查询请求中的用户名错误"
			, "WARNING", "MESSAGE");
		return;
	}

	if (qry_his_co.action_day <= 0)
	{
		m_callBack.OutputNotifyAll(
			1
			, u8"历史条件单查询请求已被服务器拒绝,原因:查询请求中的日期输入有误"
			, "WARNING", "MESSAGE");
		return;
	}

	std::vector<ConditionOrder> condition_orders;
	for (auto order : m_condition_order_his_data.his_condition_orders)
	{
		DateTime dt;
		SetDateTimeFromEpochNano(&dt, order.insert_date_time);
		int insert_day = dt.date.year * 10000 + dt.date.month * 100 + dt.date.day;
		if (insert_day == qry_his_co.action_day)
		{
			condition_orders.push_back(order);
		}
	}

	SerializerConditionOrderData nss_his;

	rapidjson::Pointer("/aid").Set(*nss_his.m_doc, "rtn_his_condition_orders");
	rapidjson::Pointer("/user_id").Set(*nss_his.m_doc, m_condition_order_data.user_id);
	rapidjson::Pointer("/action_day").Set(*nss_his.m_doc, qry_his_co.action_day);

	rapidjson::Value node_data;
	nss_his.FromVar(condition_orders, &node_data);
	rapidjson::Pointer("/his_condition_orders").Set(*nss_his.m_doc, node_data);

	std::string json_str;
	nss_his.ToString(&json_str);

	m_callBack.SendDataDirect(connId,json_str);
}