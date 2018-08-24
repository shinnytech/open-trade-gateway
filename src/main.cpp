/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "trade_server.h"
#include "config.h"

int main() {
    openlog("open-trade-gateway", LOG_CONS|LOG_NDELAY|LOG_PID, LOG_USER);
    syslog(LOG_NOTICE, "server init");
    if (!LoadConfig()) {
        return 0;
    }
    TraderServer t;
    t.Run();
    syslog(LOG_NOTICE, "server exit");
    closelog();
    return 0;
}

