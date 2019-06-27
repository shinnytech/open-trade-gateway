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
		Log.WithField("msg", "trade server init").WithField("key", "gateway").Write(LOG_INFO);

		//加载配置文件
		if (!LoadConfig())
		{
			Log.WithField("msg", "trade_server load config failed!").WithField("key", "gateway").Write(LOG_WARNING);
			return -1;
		}

		//启动md server
		boost::process::child md_child(boost::process::search_path("open-trade-mdservice"));
		
		boost::asio::io_context ios;
		
		trade_server s(ios,g_config.port);
		if (!s.init())
		{
			md_child.terminate();
			md_child.wait();
			Log.WithField("msg", "trade_server init fail!").WithField("key", "gateway").Write(LOG_INFO);
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
			Log.WithField("key", "gateway")(LOG_INFO, nullptr
				,"msg=trade_server got sig %d;", sig);
						
			s.stop();	
			flag.store(false);
			ios.stop();

			md_child.terminate();
			md_child.wait();
			
			Log.WithField("msg", "trade_server exit").WithField("key", "gateway").Write(LOG_INFO);
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
				Log.WithField("msg", "ios run exception").WithField("errmsg", ex.what()).WithField("key", "gateway").Write(LOG_ERROR);
			}
		}

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "exception: " << e.what() << std::endl;
	}
}