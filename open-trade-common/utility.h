/////////////////////////////////////////////////////////////////////////
///@file utility.h
///@brief	工具函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <vector>

std::string GenerateUniqFileName();

template<size_t N>
inline char* strcpy_x(char(&dest)[N], const char* src)
{
    return strncpy(dest, src, N - 1);
}

template<size_t N>
inline char* strcpy_x(char(&dest)[N], const std::string& src)
{
    return strncpy(dest, src.c_str(), N - 1);
}

int GetLocalEpochSecond();

long long GetLocalEpochNano();

long long GetLocalEpochMilli();

std::string GuessTradingDay();

void SplitString(const std::string& str
	, std::vector<std::string>& vecs,int len);

void CutDigital(std::string& instId);

void CutDigital_Ex(std::string& instId);

std::string base64_decode(const std::string &in);

std::string GenerateGuid();
