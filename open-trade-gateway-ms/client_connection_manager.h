/////////////////////////////////////////////////////////////////////////
///@file client_connection_manager.h
///@brief	连接管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "client_connection.h"

#include <set>

class client_connection_manager
{
public:
	client_connection_manager();

	client_connection_manager(const client_connection_manager&) = delete;

	client_connection_manager& operator=(const client_connection_manager&) = delete;

	void start(client_connection_ptr c);

	void stop(client_connection_ptr c);

	void stop_all();
private:
	std::set<client_connection_ptr> client_connections_;
};