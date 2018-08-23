/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "trade_server.h"
#include "config.h"

int main() {
    if (!LoadConfig()) {
        puts("error loading config files");
        return 0;
    }
    TraderServer t;
    t.Run();
    return 0;
}

