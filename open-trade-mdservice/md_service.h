/////////////////////////////////////////////////////////////////////////
///@file md_service.h
///@brief	合约及行情服务
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "md_connection.h"

class mdservice
{
public:
	mdservice(boost::asio::io_context& ios);

	mdservice(const mdservice&) = delete;

	mdservice& operator=(const mdservice&) = delete;

	bool init();

	void stop();

	void OnConnectionnClose();

	void OnConnectionnError();
private:
	boost::asio::io_context& io_context_;

	boost::interprocess::managed_shared_memory* m_segment;

	ShmemAllocator* m_alloc_inst;

	//合约及行情数据
	InsMapType* m_ins_map;	

	std::string m_req_subscribe_quote;

	std::string m_req_peek_message;

	md_connection_ptr m_md_connection_ptr;

	boost::asio::deadline_timer _timer;

	bool m_stop_reconnect;

	bool m_need_reconnect;

	bool OpenInstList();
	
	void StartConnect();

	void ReStartConnect();
};
