/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "ms_config.h"
#include "master_server.h"

#include <iostream>
#include <string>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/process.hpp>

int main(int argc, char* argv[])
{
	try
	{
		LogMs.WithField("msg", "open trade gateway master init").WithField("key", "gatewayms").Write(LOG_INFO);

		//加载配置文件
		if (!LoadMasterConfig())
		{
			LogMs.WithField("msg", "open trade gateway master load config failed!").WithField("key", "gatewayms").Write(LOG_WARNING);			
			return -1;
		}

		boost::asio::io_context ios;

		master_server s(ios,g_masterConfig);
		if (!s.init())
		{
			LogMs.WithField("msg", "open trade gateway master init fail!").WithField("key", "gatewayms").Write(LOG_INFO);
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
			[&s, &ios,&flag](boost::system::error_code,int sig)
		{
			s.stop();
			flag.store(false);
			ios.stop();

			LogMs.WithField("key", "gatewayms")(LOG_INFO,nullptr
				,"msg=open trade gateway master got sig %d;"
				, sig);

			LogMs.WithField("msg", "open trade gateway master exit").WithField("key", "gatewayms").Write(LOG_INFO);
		});
		
		while (flag.load())
		{
			try
			{
				ios.run();
				break;
			}
			catch (std::exception& ex)
			{
				LogMs.WithField("msg", "open trade gateway master ios run exception").WithField("errmsg", ex.what()).WithField("key", "gatewayms").Write(LOG_ERROR);
			}
		}

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "exception: " << e.what() << std::endl;
		return -1;
	}
}