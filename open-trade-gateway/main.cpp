/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "trade_server.h"
#include "config.h"

#include <iostream>
#include <string>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/process.hpp>

int main(int argc, char* argv[])
{
	try
	{
		if (!LogInit())
		{			
			return -1;
		}

		Log(LOG_INFO,NULL,"trade_server init");

		//加载配置文件
		if (!LoadConfig())

		{
			Log(LOG_WARNING,NULL,"trade_server load config failed!");
			LogCleanup();
			return -1;
		}

		//启动md server
		boost::process::child md_child(boost::process::search_path("open-trade-mdservice"));
		
		boost::asio::io_context ios;
		
		trade_server s(ios,g_config.port);
		if (!s.init())
		{
			md_child.terminate();
			Log(LOG_INFO, NULL, "trade_server init fail!");
			LogCleanup();
			return -1;
		}
		
		std::atomic_bool flag;
		flag.store(true);

		boost::asio::signal_set signals_(ios);
		signals_.add(SIGINT);
		signals_.add(SIGTERM);
		#if defined(SIGQUIT)
			signals_.add(SIGQUIT);
		#endif 
		signals_.async_wait(
			[&s,&ios,&md_child,&flag](boost::system::error_code, int sig)
		{						
			md_child.terminate();
			s.stop();	
			flag.store(false);
			ios.stop();
			Log(LOG_INFO, NULL, "trade_server got sig %d", sig);
			Log(LOG_INFO, NULL, "trade_server exit");
			LogCleanup();
		});
		
		while (flag.load())
		{
			try
			{
				ios.run();
				break;
			}
			catch(std::exception& ex)
			{
				Log(LOG_ERROR, NULL, "ios run exception:%s"
					,ex.what());
			}
		}

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "exception: " << e.what() << std::endl;
	}
}