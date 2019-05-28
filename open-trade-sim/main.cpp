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
		
		Log(LOG_INFO,nullptr
			,"msg=trade sim init;key=%s"
			,key.c_str());

		//加载配置文件
		if (!LoadConfig())
		{
			Log(LOG_WARNING, nullptr
				,"msg=trade sim load config failed!;key=%s"
				, key.c_str());
			
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
			
			Log(LOG_INFO,nullptr
				,"msg=trade sim got sig %d;key=%s"
				, sig
				, key.c_str());

			Log(LOG_INFO, nullptr
				,"msg=trade sim exit;key=%s"
				,key.c_str());
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
					,"msg=trade sim ioc run exception;errmsg=%s;key=%s"
					, ex.what()
					, key.c_str());
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "trade sim exception: " << e.what() << std::endl;
	}	
}
