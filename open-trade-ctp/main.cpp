/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "config.h"
#include "log.h"
#include "tradectp.h"
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
		
		//加载配置文件
		if (!LoadConfig())
		{
			Log(LOG_WARNING,nullptr
				,"msg=trade ctp load config failed!;key=%s"
				,key.c_str());
			return -1;
		}

		//加载合约映射
		if (!GenInstrumentExchangeIdMap())
		{
			Log(LOG_WARNING, nullptr
				, "msg=trade ctp load instrument exchange id map failed!;key=%s"
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

		traderctp tradeCtp(ioc, key);
		tradeCtp.Start();
		signals_.async_wait(
			[&ioc, &tradeCtp, &key, &flag](boost::system::error_code, int sig)
		{
			tradeCtp.Stop();
			flag.store(false);
			ioc.stop();
			Log(LOG_INFO,nullptr
				,"msg=trade ctp got sig %d;key=%s"
				,sig
				,key.c_str());

			Log(LOG_INFO,nullptr
				,"msg=trade ctp exit;key=%s"
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
				Log(LOG_ERROR,nullptr
					,"trade ctp ioc run exception,%s;key=%s"					
					, ex.what()
					, key.c_str());
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "trade ctp "<< argv[1] <<" exception: " << e.what() << std::endl;
	}
}
