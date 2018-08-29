#pragma once

typedef enum {
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
} LogLevel;

bool LogInit();
void Log(LogLevel level, const char* pack_str, const char* message_fmt, ...);
void LogCleanup();
