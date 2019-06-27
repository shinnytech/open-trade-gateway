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
	Log.WithField("msg", "mdservice init").WithField("key", "mdservice").Write(LOG_INFO);

	boost::asio::io_context ioc;

	mdservice s(ioc);
	if (!s.init())
	{
		LogMs.WithField("msg", "mdservice init fail!").WithField("key", "mdservice").Write(LOG_ERROR);
		return -1;
	}
	else
	{
		Log.WithField("msg", "mdservice init success").WithField("key", "mdservice").Write(LOG_INFO);
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
			Log.WithField("key", "mdservice")(LOG_INFO, nullptr
				,"msg=mdservice got sig %d;", sig);

			s.stop();

			flag.store(false);
			ioc.stop();			

			Log.WithField("msg", "mdservice exit").WithField("key", "mdservice").Write(LOG_INFO);
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
			LogMs.WithField("msg", "mdservice ios run exception").WithField("errmsg", ex.what()).WithField("key", "mdservice").Write(LOG_ERROR);
		}
	}	

	return 0;
}

