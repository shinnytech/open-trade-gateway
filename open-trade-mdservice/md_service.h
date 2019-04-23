/////////////////////////////////////////////////////////////////////////
///@file md_service.h
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"
#include <boost/asio/io_context.hpp>

namespace md_service
{
	bool LoadInsList();

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

	//初始化 MdService 实例
	bool Init(boost::asio::io_context& ioc);

	//要求 MdService 实例停止运行
	void Stop();
}