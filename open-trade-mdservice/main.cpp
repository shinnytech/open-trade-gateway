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
	Log().WithField("fun","main")
		.WithField("key","mdservice")
		.Log(LOG_INFO,"mdservice init!");
	
	boost::asio::io_context ioc;

	mdservice s(ioc);
	if (!s.init())
	{
		Log().WithField("fun","main")
			.WithField("key","mdservice")
			.Log(LOG_INFO,"mdservice init fail!");
		return -1;
	}
	else
	{
		Log().WithField("fun","main")
			.WithField("key","mdservice")
			.Log(LOG_INFO,"mdservice init success!");	
	}

	std::atomic_bool flag;
	flag.store(true);

	boost::asio::signal_set signals_(ioc);
	signals_.add(SIGINT);
	signals_.add(SIGTERM);
	#if defined(SIGQUIT)
		signals_.add(SIGQUIT);
	#endif 

	signals_.async_wait(
			[&s, &ioc, &flag](boost::system::error_code, int sig)
		{
			Log().WithField("fun","main")
			.WithField("key","mdservice")
			.WithField("sig",sig)
			.Log(LOG_INFO,"mdservice got sig");
		
			s.stop();

			flag.store(false);
			ioc.stop();			

			Log().WithField("fun","main")
				.WithField("key","mdservice")				
				.Log(LOG_INFO,"mdservice exit");			
		});
	
	while (flag.load())
	{
		try
		{
			ioc.run();
			break;
		}
		catch (std::exception& ex)
		{
			Log().WithField("fun","main")
				.WithField("key","mdservice")
				.WithField("errmsg",ex.what())
				.Log(LOG_INFO,"mdservice ios run exception");		
		}
	}	

	return 0;
}

