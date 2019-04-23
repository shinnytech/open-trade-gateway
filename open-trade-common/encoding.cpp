/////////////////////////////////////////////////////////////////////////
///@file encoding.cpp
///@brief	字符编码处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "iconvpp.h"
#include <string>

std::string GBKToUTF8(const char* strGBK)
{
    iconvpp::converter conv("UTF-8",   // output encoding
                            "GBK",  // input encoding
                            true,      // ignore errors (optional, default: fasle)
                            1024);     // buffer size   (optional, default: 1024)
    std::string output;
    conv.convert(strGBK, output);
    return output;
}

std::string UTF8ToGBK(const char* strUTF8)
{
    iconvpp::converter conv("GBK",   // output encoding
                            "UTF-8",  // input encoding
                            true,      // ignore errors (optional, default: fasle)
                            1024);     // buffer size   (optional, default: 1024)
    std::string output;
    conv.convert(strUTF8, output);
    return output;
}
