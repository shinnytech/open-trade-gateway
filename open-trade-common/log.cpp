/////////////////////////////////////////////////////////////////////////
///@file log.cpp
///@brief	日志操作
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "log.h"
#include <stdarg.h>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iostream>

#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

struct LogContext
{
	std::mutex m_log_file_mutex;

	int m_log_file_fd;  	
} log_context;

const char* Level2String(LogLevel level)
{
    switch (level) 
	{
        case LogLevel::LOG_DEBUG:
            return "debug";
        case LogLevel::LOG_INFO:
            return "info";
        case LogLevel::LOG_WARNING:
            return "warning";
        case LogLevel::LOG_ERROR:
            return "error";
        case LogLevel::LOG_FATAL:
            return "fatal";
        default:
            return "info";
    }
}

const char* CurrentDateTimeStr()
{
    //2018-08-29T09:41:37.532100652+08:00
    static char dtstr[128];
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    std::size_t fractional_seconds = ns.count() % 1000000000LL;
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ns);
    std::time_t t = s.count();
    std::tm* tm = std::localtime(&t);
    sprintf(dtstr, "%04d-%02d-%02dT%02d:%02d:%02d.%09lld+08:00"
    , tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday
    , tm->tm_hour, tm->tm_min, tm->tm_sec
    , fractional_seconds);
    return dtstr;
}

void Log(LogLevel level, const char* pack_str, const char* message_fmt, ...)
{
	std::lock_guard<std::mutex> lock(log_context.m_log_file_mutex);
	
	
    const char* level_str = Level2String(level);
    const char* datetime_str = CurrentDateTimeStr();	

	std::stringstream ss;
	ss << "{\"time\": \"" << datetime_str
		<< "\", \"level\": \"" << level_str
		<< "\", \"msg\": \"";

	va_list arglist;
	char buf[1024 * 5];
	memset(buf, 0, sizeof(buf));
	va_start(arglist, message_fmt);
	vsnprintf(buf,1024*5,message_fmt,arglist);
	va_end(arglist);
	if (pack_str)
	{
		ss << buf << "\", \"pack\":"
			<< pack_str << "}\n";		
	}
	else
	{
		ss << buf << "\"}\n";
	}
	std::string str = ss.str();
	write(log_context.m_log_file_fd,str.c_str(),str.length());
}

bool LogInit()
{
	std::string logFileName = "/var/log/open-trade-gateway/open-trade-gateway.log";
    log_context.m_log_file_fd = open(logFileName.c_str(),O_WRONLY|O_APPEND| O_CREAT,S_IRUSR|S_IWUSR);
	if (log_context.m_log_file_fd == -1)
	{
        printf("can't open log file:%s",logFileName.c_str());
        return false;
    }
    return true;
}

void LogCleanup()
{
    close(log_context.m_log_file_fd);
}
