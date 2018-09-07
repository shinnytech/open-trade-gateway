/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <signal.h>
#include "log.h"
#include "trade_server.h"
#include "md_service.h"
#include "config.h"

static int interrupted = 0;

void sigint_handler(int sig)
{
    Log(LOG_INFO, NULL, "server got sig %d", sig);
	md_service::Stop();
	trade_server::Stop();
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
    //加载合约文件, 连接行情服务
    if (!md_service::Init())
        return -1;
    if (!trade_server::Init())
        return -1;
    //提供交易服务
    trade_server::Run();
    //服务结束
    md_service::CleanUp();
    trade_server::CleanUp();
    Log(LOG_INFO, NULL, "server exit");
    LogCleanup();
    return 0;
}

