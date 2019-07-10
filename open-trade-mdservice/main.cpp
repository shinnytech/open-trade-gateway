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

	boost::asio::io_context ioc;

	mdservice s(ioc);
	if (!s.init())
	{
		Log(LOG_ERROR, nullptr
			, "msg=mdservice init fail!;key=mdservice");
		return -1;
	}
	else
	{
		Log(LOG_INFO, nullptr
			, "msg=mdservice init success;key=mdservice");
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
			Log(LOG_INFO, nullptr
				, "msg=mdservice got sig %d;key=mdservice", sig);

			s.stop();

			flag.store(false);
			ioc.stop();			

			Log(LOG_INFO, nullptr
				,"msg=mdservice exit;key=mdservice");
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
			Log(LOG_ERROR, nullptr
				, "msg=mdservice ios run exception;errmsg=%s;key=mdservice"
				, ex.what());
		}
	}	

	return 0;
}

