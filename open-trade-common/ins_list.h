/////////////////////////////////////////////////////////////////////////
///@file ins_list.h
///@brief	查询合约信息
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

//获取指定代码的合约/行情信息
Instrument* GetInstrument(const std::string& symbol);

bool GenInstrumentExchangeIdMap();

std::string GuessExchangeId(const std::string& instrument_id);
