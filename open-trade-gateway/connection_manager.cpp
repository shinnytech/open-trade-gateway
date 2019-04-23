/////////////////////////////////////////////////////////////////////////
///@file connection_manager.cpp
///@brief	连接管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "connection_manager.h"

connection_manager::connection_manager()
{
}

void connection_manager::start(connection_ptr c)
{
	connections_.insert(c);
	c->start();
}

void connection_manager::stop(connection_ptr c)
{
	connections_.erase(c);
	c->stop();
}

void connection_manager::stop_all()
{
	for (auto c : connections_)
		c->stop();
	connections_.clear();
}