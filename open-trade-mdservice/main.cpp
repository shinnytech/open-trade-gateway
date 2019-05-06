/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "log.h"
#include "md_service.h"
#include "ins_list.h"

#include <iostream>
#include <string>


#include <boost/asio.hpp>
#include <boost/thread.hpp>

int main(int argc, char* argv[])
{
	Log2(LOG_INFO,"mdservice init");

	if (!md_service::LoadInsList())
	{
		Log2(LOG_ERROR,"mdservice LoadInsList failed");
		return -1;
	}
	
	Log2(LOG_INFO,"mdservice LoadInsList success");
			
	boost::asio::io_context ioc;
	boost::asio::signal_set signals_(ioc);

	signals_.add(SIGINT);
	signals_.add(SIGTERM);
	#if defined(SIGQUIT)
		signals_.add(SIGQUIT);
	#endif 

	signals_.async_wait(
			[&ioc](boost::system::error_code, int sig)
		{
			ioc.stop();
			Log2(LOG_INFO,"mdservice got sig %d",sig);
			Log2(LOG_INFO,"mdservice exit");
		});
		
	if (!md_service::Init(ioc))
	{
		Log2(LOG_INFO,"mdservice inited fail");
		return -1;
	}
	
	Log2(LOG_INFO,"mdservice inited succss");

	ioc.run();	

	return 0;
}

