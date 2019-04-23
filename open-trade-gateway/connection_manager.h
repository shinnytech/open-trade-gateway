/////////////////////////////////////////////////////////////////////////
///@file connection_manager.h
///@brief	连接管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "connection.h"

#include <set>

class connection_manager
{
public:
	connection_manager();

	connection_manager(const connection_manager&) = delete;

	connection_manager& operator=(const connection_manager&) = delete;

	void start(connection_ptr c);

	void stop(connection_ptr c);

	void stop_all();
private:
	std::set<connection_ptr> connections_;
};