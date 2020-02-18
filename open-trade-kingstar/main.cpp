#include "config.h"
#include "log.h"
#include "ins_list.h"
#include "tradeks.h"

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
		
		//加载配置文件
		if (!LoadConfig())
		{
			Log().WithField("fun","main")
				.WithField("key",key)
				.Log(LOG_WARNING,"trade kingstar load config failed!");			
			return -1;
		}

		//加载合约映射
		if (!GenInstrumentExchangeIdMap())
		{
			Log().WithField("fun","main")
				.WithField("key",key)
				.Log(LOG_WARNING,"trade kingstar load instrument exchange id map failed!");
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

		traderctp tradeKs(ioc, key);
		tradeKs.Start();
		signals_.async_wait(
			[&ioc,&tradeKs,&key,&flag](boost::system::error_code, int sig)
		{
			tradeKs.Stop();
			flag.store(false);
			ioc.stop();

			Log().WithField("fun","main")
				.WithField("key",key)
				.WithField("sig",sig)
				.Log(LOG_INFO,"trade kingstar got sig");			

			Log().WithField("fun","main")
				.WithField("key",key)				
				.Log(LOG_INFO,"trade kingstar exit");			
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
					.WithField("key",key)
					.WithField("errmsg",ex.what())
					.Log(LOG_ERROR,"trade kingstar ioc run exception");			
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "trade kingstar "<< argv[1] <<" exception:" << e.what() << std::endl;
	}
}
