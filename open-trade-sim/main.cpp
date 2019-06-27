/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "config.h"
#include "log.h"
#include "tradersim.h"
#include "ins_list.h"

#include <iostream>
#include <string>
#include <fstream>
#include <atomic>

#include <boost/asio.hpp>

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			return -1;
		}

		std::string key = argv[1];
		
		Log.WithField("msg", "trade sim init").WithField("key", key.c_str()).Write(LOG_INFO);

		//加载配置文件
		if (!LoadConfig())
		{
			Log.WithField("msg", "trade sim load config failed!").WithField("key", key.c_str()).Write(LOG_WARNING);
			
			return -1;
		}

		boost::asio::io_context ioc;

		std::atomic_bool flag;
		flag.store(true);

		boost::asio::signal_set signals_(ioc);

		signals_.add(SIGINT);
		signals_.add(SIGTERM);
#if defined(SIGQUIT)
		signals_.add(SIGQUIT);
#endif

		tradersim tradeSim(ioc,key);
		tradeSim.Start();
		signals_.async_wait(
			[&ioc, &tradeSim, &key, &flag](boost::system::error_code, int sig)
		{
			tradeSim.Stop();
			flag.store(false);
			ioc.stop();
			
			Log.WithField("key", sig)(LOG_INFO,nullptr
				,"msg=trade sim got sig %d;"
				, key.c_str());

			Log.WithField("msg", "trade sim exit").WithField("key", key.c_str()).Write(LOG_INFO);
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
				Log.WithField("msg", "trade sim ioc run exception").WithField("errmsg", ex.what()).WithField("key", key.c_str()).Write(LOG_ERROR);
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "trade sim exception: " << e.what() << std::endl;
	}	
}
