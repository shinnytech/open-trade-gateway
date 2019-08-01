/////////////////////////////////////////////////////////////////////////
///@file log.h
///@brief	日志操作
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <thread>
#include <mutex>

typedef enum 
{
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
} LogLevel;

class LogContext
{
public:
	virtual LogContext& WithField(const std::string& key, bool value) = 0;

	virtual LogContext& WithField(const std::string& key, char value) = 0;

	virtual LogContext& WithField(const std::string& key, unsigned char value) = 0;

	virtual LogContext& WithField(const std::string& key, int value) = 0;

	virtual LogContext& WithField(const std::string& key, unsigned int value) = 0;

	virtual LogContext& WithField(const std::string& key, short value) = 0;

	virtual LogContext& WithField(const std::string& key, unsigned short value) = 0;

	virtual LogContext& WithField(const std::string& key, long value) = 0;

	virtual LogContext& WithField(const std::string& key, unsigned long value) = 0;
	
	virtual LogContext& WithField(const std::string& key, float value) = 0;

	virtual LogContext& WithField(const std::string& key, double value) = 0;
	
	virtual LogContext& WithField(const std::string& key,const std::string& value)=0;	

	virtual LogContext& WithField(const std::string& key, const char* value) = 0;

	virtual LogContext& WithPack(const std::string& key, const std::string& json_str) = 0;

	virtual LogContext& WithPack(const std::string& key, const char* json_str) = 0;

	virtual void Log(LogLevel level, const std::string& msg) = 0;
};

LogContext& Log();

LogContext& LogMs();


