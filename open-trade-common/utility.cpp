/////////////////////////////////////////////////////////////////////////
///@file utility.cpp
///@brief	工具函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "utility.h"

#include <string>
#include <chrono>
#include <cstring>
#include <ctime>
#include <stdio.h>

#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

std::string GenerateUniqFileName()
{	
	static char base[] = "/tmp/myfileXXXXXX";
	char fname[1024];
	strcpy(fname, base);
	mkstemp(fname);
	return fname;
}

int GetLocalEpochSecond()
{
	auto now = std::chrono::high_resolution_clock::now();
	std::chrono::seconds mi = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
	return mi.count();
}

long long GetLocalEpochNano()
{
    auto now = std::chrono::high_resolution_clock::now();
	std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    return ns.count();
}

long long GetLocalEpochMilli()
{
	auto now = std::chrono::high_resolution_clock::now();
	std::chrono::milliseconds mi = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	return mi.count();
}

std::string GuessTradingDay()
{
	std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm* t = std::localtime(&now);
	if (t->tm_hour >= 16)
	{
		//Friday, Saturday, Sunday
		if (t->tm_wday == 5)
		{
			now += 3600 * 24 * 3;
		}
		else if (t->tm_wday == 6)
		{
			now += 3600 * 24 * 2;
		}
		else
		{
			now += 3600 * 24 * 1;
		}
	}
	else
	{
		if (t->tm_wday == 6)
		{
			now += 3600 * 24 * 2;
		}
		else if (t->tm_wday == 0)
		{
			now += 3600 * 24 * 1;
		}
	}
	t = std::localtime(&now);
	char buf[16];
	snprintf(buf, 16, "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
	return std::string(buf);
}

void SplitString(const std::string& str
	, std::vector<std::string>& vecs, int len)
{
	std::string strLeave = str;
	int i = 0;
	vecs.clear();
	while (strLeave.length() > len)
	{
		std::string strMsg = strLeave.substr(i*len,len);
		vecs.push_back(strMsg);
		strLeave = strLeave.substr((i + 1)*len);
	}
	if (strLeave.length() > 0)
	{
		vecs.push_back(strLeave);
	}
}

std::string base64_decode(const std::string &in)
{
	std::string out;
	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

	int val = 0, valb = -8;
	for (const char& c : in)
	{
		if (T[c] == -1) break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0)
		{
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

void CutDigital(std::string& instId)
{
	if (instId.empty())
	{
		return;
	}
	
	unsigned int i = 0;
	bool flag = false;
	while (i < instId.length())
	{
		if ('0' <= instId.at(i) && instId.at(i) <= '9')
		{			
			flag = true;
			break;
		}
		i++;
	}

	if (flag)
	{
		instId = instId.substr(0, i);
	}	
}

void CutDigital_Ex(std::string& instId)
{
	std::vector<std::string> vecs;
	boost::split(vecs,instId,boost::is_any_of(" "),boost::token_compress_on);
	if (vecs.size() != 2)
	{
		CutDigital(instId);
		return;
	}

	instId = vecs[1];
	vecs.clear();
	boost::split(vecs,instId,boost::is_any_of("&"),boost::token_compress_on);
	if (vecs.size() != 2)
	{
		CutDigital(instId);
		return;
	}

	instId= vecs[0];
	CutDigital(instId);
}

std::string GenerateGuid()
{
	std::string result;
	result.reserve(36);	
	boost::uuids::uuid u = boost::uuids::random_generator()();
	std::size_t i = 0;
	for (boost::uuids::uuid::const_iterator it_data = u.begin()
		; it_data != u.end()
		; ++it_data, ++i)
	{
		const size_t hi = ((*it_data) >> 4) & 0x0F;
		result += boost::uuids::detail::to_char(hi);
		const size_t lo = (*it_data) & 0x0F;
		result += boost::uuids::detail::to_char(lo);
	}
	return result;	
}