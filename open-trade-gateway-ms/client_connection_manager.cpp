/////////////////////////////////////////////////////////////////////////
///@file client_connection_manager.cpp
///@brief	连接管理类
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "client_connection_manager.h"

client_connection_manager::client_connection_manager()
{
}

void client_connection_manager::start(client_connection_ptr c)
{
	client_connections_.insert(c);
	c->start();
}

void client_connection_manager::stop(client_connection_ptr c)
{	
	try
	{
		client_connections_.erase(c);
		c->stop();
	}
	catch (std::exception& ex)
	{
		LogMs(LOG_WARNING,nullptr
			, "fun=stop;msg=client_connection_manager stop connection;errmsg=%s;key=gatewayms"
			, ex.what());
	}	
}

void client_connection_manager::stop_all()
{
	for (auto c : client_connections_)
		c->stop();
	client_connections_.clear();
}