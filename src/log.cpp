#include "stdafx.h"
#include "log.h"

#include <stdarg.h>

struct LogContext{
    FILE* m_log_file;
    std::mutex m_log_file_mutex;
} log_context;

const char* Level2String(LogLevel level)
{
    switch (level) {
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
    fprintf(log_context.m_log_file, "{\"time\": \"%s\", \"level\": \"%s\", \"msg\": \"", datetime_str, level_str);
    va_list arglist;
    va_start(arglist, message_fmt);
    vfprintf(log_context.m_log_file, message_fmt, arglist);
    va_end(arglist);
    if (pack_str){
        fprintf(log_context.m_log_file, "\", \"pack\": %s}\n", pack_str);
    } else {
        fprintf(log_context.m_log_file, "\"}\n");
    }
    fflush(log_context.m_log_file);
}

bool LogInit()
{
    log_context.m_log_file = fopen("/var/log/open-trade-gateway/log", "a+t");
    if (!log_context.m_log_file){
        printf("can't open log file: /var/log/open-trade-gateway/log");
        return false;
    }
    return true;
}

void LogCleanup()
{
    fclose(log_context.m_log_file);
}
