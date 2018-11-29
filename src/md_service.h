/////////////////////////////////////////////////////////////////////////
///@file md_service.h
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <boost/asio/io_context.hpp>


namespace md_service {
const int kOptionClassCall = 1;
const int kOptionClassPut = 2;

const int kProductClassAll = 0xFFFFFFFF;
const int kProductClassFutures = 0x00000001;
const int kProductClassOptions = 0x00000002;
const int kProductClassCombination = 0x00000004;
const int kProductClassFOption = 0x00000008;
const int kProductClassFutureIndex = 0x00000010;
const int kProductClassFutureContinuous = 0x00000020;
const int kProductClassStock = 0x00000040;
const int kProductClassFuturePack = kProductClassFutures | kProductClassFutureIndex | kProductClassFutureContinuous;

const int kExchangeShfe = 0x00000001;
const int kExchangeCffex = 0x00000002;
const int kExchangeCzce = 0x00000004;
const int kExchangeDce = 0x00000008;
const int kExchangeIne = 0x00000010;
const int kExchangeKq = 0x00000020;
const int kExchangeSswe = 0x00000040;
const int kExchangeUser = 0x10000000;
const int kExchangeAll = 0xFFFFFFFF;


struct Instrument {
    Instrument();
    bool expired;
    long product_class;
    long volume_multiple;
    volatile double margin;
    volatile double commission;
    volatile double price_tick;

    volatile double last_price;
    volatile double pre_settlement;
    volatile double upper_limit;
    volatile double lower_limit;
    volatile double ask_price1;
    volatile double bid_price1;
};


/*
MdService 从 Diff (http://www.shinnytech.com/diff) 的服务器获取合约及行情信息
Example:
    运行及停止 MdService:
        md_service::Init();  //初始化 合约及行情服务
        md_service::Cleanup();  //要求 合约及行情服务 停止运行

    md_service 初始化完成后, 可以提供合约及行情信息:
        md_service::Instrument* ins = md_service::GetInstrument("SHFE.cu1801");
        ins->volume_multiple; //合约乘数
        ins->last_price; //合约的最新价
*/

bool LoadInsList();
//初始化 MdService 实例
bool Init(boost::asio::io_context& ioc);
//要求 MdService 实例停止运行
void Stop();
//获取指定代码的合约/行情信息
Instrument* GetInstrument(const std::string& symbol);
}