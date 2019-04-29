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
		std::string logFileName = argv[1];
		
		//加载配置文件
		if (!LoadConfig())
		{
			Log(LOG_WARNING,"msg=trade ctp %s load config failed!"
				, logFileName.c_str());
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

		traderctp tradeCtp(ioc, logFileName);
		tradeCtp.Start();
		signals_.async_wait(
			[&ioc, &tradeCtp, &logFileName, &flag](boost::system::error_code, int sig)
		{
			tradeCtp.Stop();
			flag.store(false);
			ioc.stop();
			Log(LOG_INFO,"msg=trade ctp %s got sig %d", logFileName.c_str(), sig);
			Log(LOG_INFO,"msg=trade ctp %s exit", logFileName.c_str());
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
				Log(LOG_ERROR,"msg=trade ctp %s ioc run exception,%s"
					, logFileName.c_str()
					, ex.what());
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "trade ctp "<< argv[1] <<" exception: " << e.what() << std::endl;
	}
}
