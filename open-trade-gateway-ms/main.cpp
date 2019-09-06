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
		LogMs().WithField("fun","main")
			.WithField("key","gatewayms")
			.Log(LOG_INFO,"open trade gateway master init");

		//加载BrokerList
		if (!LoadBrokerList())
		{
			LogMs().WithField("fun", "main")
				.WithField("key", "gatewayms")
				.Log(LOG_WARNING, "open trade gateway master load broker list failed!");
			return -1;
		}
				
		//加载负载均衡服务配置文件
		if (!LoadMasterConfig())
		{
			LogMs().WithField("fun","main")
				.WithField("key","gatewayms")
				.Log(LOG_WARNING,"open trade gateway master load config failed!");		
			return -1;
		}

		boost::asio::io_context ios;

		master_server s(ios,g_masterConfig);
		if (!s.init())
		{
			LogMs().WithField("fun","main")
				.WithField("key","gatewayms")
				.Log(LOG_INFO,"open trade gateway master init fail!");			
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

			LogMs().WithField("fun","main")
				.WithField("key","gatewayms")
				.WithField("sig",sig)
				.Log(LOG_INFO,"open trade gateway master got sig");

			LogMs().WithField("fun","main")
				.WithField("key","gatewayms")				
				.Log(LOG_INFO,"open trade gateway master exit");			
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
				LogMs().WithField("fun","main")
					.WithField("key","gatewayms")
					.WithField("errmsg",ex.what())
					.Log(LOG_INFO,"open trade gateway master ios run exception");
			}
		}

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "exception:" << e.what() << std::endl;
		return -1;
	}
}