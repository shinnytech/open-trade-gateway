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
	Log(LOG_INFO,nullptr
		,"msg=mdservice init;key=mdservice");

	if (!md_service::LoadInsList())
	{
		Log(LOG_ERROR, nullptr
			,"mdservice LoadInsList failed;key=mdservice");
		return -1;
	}
	
	Log(LOG_INFO, nullptr
		,"mdservice LoadInsList success;key=mdservice");
			
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
			Log(LOG_INFO, nullptr
				,"mdservice got sig %d;key=mdservice",sig);

			Log(LOG_INFO, nullptr
				,"mdservice exit;key=mdservice");
		});
		
	if (!md_service::Init(ioc))
	{
		Log(LOG_INFO,nullptr
			,"mdservice inited fail;key=mdservice");
		return -1;
	}
	
	Log(LOG_INFO,nullptr
		,"mdservice inited succss;key=mdservice");

	ioc.run();	

	return 0;
}

