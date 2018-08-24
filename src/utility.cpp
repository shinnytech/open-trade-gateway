/////////////////////////////////////////////////////////////////////////
///@file utility.cpp
///@brief	工具函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "utility.h"
#include <cstring>

std::string GenerateUniqFileName()
{
    static char base[] = "/tmp/myfileXXXXXX";
    char fname[1024];
    strcpy(fname, base);
    mktemp(fname);
    return fname;
}