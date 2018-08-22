/////////////////////////////////////////////////////////////////////////
///@file encoding.cpp
///@brief	字符编码处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "encoding.h"
#include <windows.h>

std::string GBKToUTF8(const char* strGBK)
{
    int len = MultiByteToWideChar(936, 0, strGBK, -1, NULL, 0);
    wchar_t* wstr = new wchar_t[len + 1];
    memset(wstr, 0, len + 1);
    MultiByteToWideChar(936, 0, strGBK, -1, wstr, len);
    len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = new char[len + 1];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    std::string strTemp = str;
    if (wstr)
        delete[] wstr;
    if (str)
        delete[] str;
    return strTemp;
}

std::string UTF8ToGBK(const char* strUTF8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, NULL, 0);
    wchar_t* wszGBK = new wchar_t[len + 1];
    memset(wszGBK, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, strUTF8, -1, wszGBK, len);
    len = WideCharToMultiByte(936, 0, wszGBK, -1, NULL, 0, NULL, NULL);
    char* szGBK = new char[len + 1];
    memset(szGBK, 0, len + 1);
    WideCharToMultiByte(936, 0, wszGBK, -1, szGBK, len, NULL, NULL);
    std::string strTemp(szGBK);
    if (wszGBK)
        delete[] wszGBK;
    if (szGBK)
        delete[] szGBK;
    return strTemp;
}

std::string WideToUtf8(const wchar_t* wstr)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = new char[len + 1];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    std::string str_utf8 = str;
    delete[] str;
    return str_utf8;
}

std::wstring Utf8ToWide(const char* str_utf8)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, NULL, 0);
    wchar_t* wsz_wide = new wchar_t[len + 1];
    memset(wsz_wide, 0, len * 2 + 2);
    MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, wsz_wide, len);
    std::wstring str_wide = wsz_wide;
    delete[] wsz_wide;
    return str_wide;
}

std::wstring GbkToWide(const char* str_gbk)
{
    int len = MultiByteToWideChar(936, 0, str_gbk, -1, NULL, 0);
    wchar_t* wsz_wide = new wchar_t[len + 1];
    memset(wsz_wide, 0, len * 2 + 2);
    MultiByteToWideChar(936, 0, str_gbk, -1, wsz_wide, len);
    std::wstring str_wide = wsz_wide;
    delete[] wsz_wide;
    return str_wide;
}

std::string WStringToUtf8(const std::wstring src)
{
    return WideToUtf8(src.c_str());
}

std::wstring Utf8ToWString(const char* utf8_str)
{
    return Utf8ToWide(utf8_str);
}
