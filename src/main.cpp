/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <signal.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>

#include "log.h"
#include "trade_server.h"
#include "md_service.h"
#include "config.h"

static int interrupted = 0;

void sigint_handler(int sig)
{
    Log(LOG_INFO, NULL, "server got sig %d", sig);
    Log(LOG_INFO, NULL, "server exit");
    LogCleanup();
    exit(0);
	// md_service::Stop();
	// trade_server::Stop();
}

int main() {
    LogInit();
    Log(LOG_INFO, NULL, "server init");
    //加载配置文件
    if (!LoadConfig()) {
        return -1;
    }
    signal(SIGTERM, sigint_handler);
    signal(SIGINT, sigint_handler);
    //加载合约信息
    if (!md_service::LoadInsList())
        return -1;
    //运行网络服务
    boost::asio::io_context ioc;
    if(md_service::Init(ioc)){
        Log(LOG_INFO, NULL, "md service inited");
        ioc.run();
    }else{
        trade_server::Init(ioc, boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(), g_config.port});
        Log(LOG_INFO, NULL, "trade server inited, host=%s, port=%d", g_config.host.c_str(), g_config.port);
        ioc.run();
    }
    //服务结束
    Log(LOG_INFO, NULL, "server exit");
    LogCleanup();
    return 0;
}
