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
		LogMs(LOG_INFO,nullptr
			,"msg=open trade gateway master init;key=gatewayms");

		//加载配置文件
		if (!LoadMasterConfig())
		{
			LogMs(LOG_WARNING,nullptr
				,"msg=open trade gateway master load config failed!;key=gatewayms");			
			return -1;
		}

		boost::asio::io_context ios;

		master_server s(ios,g_masterConfig);
		if (!s.init())
		{
			LogMs(LOG_INFO,nullptr
				,"msg=open trade gateway master init fail!;key=gatewayms");
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

			LogMs(LOG_INFO,nullptr
				, "msg=open trade gateway master got sig %d;key=gatewayms"
				, sig);

			LogMs(LOG_INFO,nullptr
				, "msg=open trade gateway master exit;key=gatewayms");
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
				LogMs(LOG_ERROR,nullptr
					, "msg=open trade gateway master ios run exception;errmsg=%s;key=gatewayms"
					, ex.what());
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