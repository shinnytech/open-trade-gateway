/////////////////////////////////////////////////////////////////////////
///@file encoding.h
///@brief	字符编码处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <Windows.h>

std::string GBKToUTF8(const char* strGBK);
std::string UTF8ToGBK(const char* strUTF8);
std::string WideToUtf8(const wchar_t* wstr);
std::wstring Utf8ToWide(const char* str_utf8);

