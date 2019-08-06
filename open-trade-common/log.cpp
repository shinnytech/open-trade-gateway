/////////////////////////////////////////////////////////////////////////
///@file log.cpp
///@brief	日志操作
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "log.h"
#include "rapid_serialize.h"

#include <stdarg.h>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iostream>
#include <vector>

#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/algorithm/string.hpp>

using namespace std;

class LogContextImp:public LogContext
{
public:
	LogContextImp()
		:m_local_host_name("")
		,m_log_file_name("")		
	{
		char hostname[128];
		if (gethostname(hostname,sizeof(hostname)))
		{
			m_local_host_name = "unknown";
		}
		else
		{
			m_local_host_name = hostname;
		}
	}

	virtual ~LogContextImp()
	{
	}

	virtual LogContext& WithField(const std::string& key, bool value)
	{
		boolMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, char value)
	{
		charMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, unsigned char value)
	{
		ucharMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, int value)
	{
		intMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, unsigned int value)
	{
		uintMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, short value)
	{
		shortMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, unsigned short value)
	{
		ushortMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, long value)
	{
		longMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, unsigned long value)
	{
		ulongMap[key] = value;
		return *this;
	}
		
	virtual LogContext& WithField(const std::string& key, float value)
	{
		floatMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, double value)
	{
		doubleMap[key] = value;
		return *this;
	}
		
	virtual LogContext& WithField(const std::string& key, const std::string& value)
	{
		stringMap[key] = value;
		return *this;
	}

	virtual LogContext& WithField(const std::string& key, const char* value)
	{
		stringMap[key] = value;
		return *this;
	}
	
	virtual LogContext& WithPack(const std::string& key, const std::string& json_str)
	{
		packMap[key] = json_str;
		return *this;
	}

	virtual LogContext& WithPack(const std::string& key, const char* json_str)
	{
		packMap[key] = json_str;
		return *this;
	}
		
	bool FromString(const std::string& jsonStr,rapidjson::Document& doc)
	{
		rapidjson::StringStream buffer(jsonStr.c_str());
		typedef rapidjson::EncodedInputStream<rapidjson::UTF8<>, rapidjson::StringStream> InputStream;
		InputStream os(buffer);
		doc.ParseStream<rapidjson::kParseNanAndInfFlag,rapidjson::UTF8<> >(os);
		if (doc.HasParseError())
		{			
			return false;
		}
		return true;
	}

	bool ToString(rapidjson::Document& rootDoc,std::string& jsonStr)
	{
		rapidjson::StringBuffer buffer(0,2048);
		typedef rapidjson::EncodedOutputStream<rapidjson::UTF8<>,rapidjson::StringBuffer> OutputStream;
		OutputStream os(buffer, false);
		rapidjson::Writer<OutputStream
			,rapidjson::UTF8<>
			,rapidjson::UTF8<>
			,rapidjson::CrtAllocator
			,rapidjson::kWriteNanAndInfFlag> writer(os);
		rootDoc.Accept(writer);
		jsonStr = std::string(buffer.GetString());
		return true;
	}

	virtual void Log(LogLevel level, const std::string& msg)
	{
		rapidjson::Document rootDoc;

		rapidjson::Pointer("/time").Set(rootDoc, CurrentDateTimeStr().c_str());
		rapidjson::Pointer("/level").Set(rootDoc, Level2String(level).c_str());
		int pid = (int)getpid();
		rapidjson::Pointer("/pid").Set(rootDoc,pid);
		std::stringstream ss;
		ss << std::this_thread::get_id();		
		rapidjson::Pointer("/tid").Set(rootDoc,ss.str().c_str());
		rapidjson::Pointer("/node").Set(rootDoc,m_local_host_name.c_str());
		rapidjson::Pointer("/msg").Set(rootDoc,msg.c_str());		

		for (auto& kv : boolMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}

		for (auto& kv : charMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}

		for (auto& kv : ucharMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc, kv.second);
		}

		for (auto& kv : intMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc, kv.second);
		}

		for (auto& kv : uintMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}

		for (auto& kv : shortMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc, kv.second);
		}

		for (auto& kv : ushortMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc, kv.second);
		}

		for (auto& kv : longMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}

		for (auto& kv : ulongMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}
		
		for (auto& kv : floatMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}

		for (auto& kv : doubleMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second);
		}
		
		for (auto& kv : stringMap)
		{
			std::string strKey = "/" + kv.first;
			rapidjson::Pointer(strKey.c_str()).Set(rootDoc,kv.second.c_str());
		}
					   		
		for (auto& kv : packMap)
		{			
			rapidjson::Document doc;
			rapidjson::Value packValue;
			if (FromString(kv.second,doc))
			{
				rapidjson::Document::AllocatorType& a = rootDoc.GetAllocator();
				packValue.CopyFrom(doc,a);
				std::string strKey = "/" + kv.first;
				rapidjson::Pointer(strKey.c_str()).Set(rootDoc,packValue);				
			}
		}
		
		std::string str;
		if (ToString(rootDoc, str))
		{
			str += "\n";			
			int log_file_fd = open(m_log_file_name.c_str()
				, O_WRONLY | O_APPEND | O_CREAT
				, S_IRUSR | S_IWUSR);
			if (log_file_fd == -1)
			{
				printf("can't open log file:%s", m_log_file_name.c_str());				
			}
			else
			{
				write(log_file_fd,str.c_str(),str.length());
				close(log_file_fd);
			}			
		}	
		
		boolMap.clear();
		charMap.clear();
		ucharMap.clear();
		intMap.clear();
		uintMap.clear();
		shortMap.clear();
		ushortMap.clear();
		longMap.clear();
		ulongMap.clear();	
		floatMap.clear();
		doubleMap.clear();		
		stringMap.clear();
		packMap.clear();
	}
protected:
	std::string m_local_host_name;

	std::string m_log_file_name;	

	std::map<std::string, bool> boolMap;

	std::map<std::string, char> charMap;

	std::map<std::string, unsigned char> ucharMap;

	std::map<std::string,int> intMap;

	std::map<std::string, unsigned int> uintMap;

	std::map<std::string, short> shortMap;

	std::map<std::string, unsigned short> ushortMap;

	std::map<std::string, long> longMap;

	std::map<std::string, unsigned long> ulongMap;
	
	std::map<std::string, float> floatMap;

	std::map<std::string, double> doubleMap;	

	std::map<std::string, std::string> stringMap;

	std::map<std::string, std::string> packMap;		
	
	std::string Level2String(LogLevel level)
	{
		std::string strLevel = "";
		switch (level)
		{
		case LogLevel::LOG_DEBUG:
			strLevel="debug";
			break;
		case LogLevel::LOG_INFO:
			strLevel = "info";
			break;
		case LogLevel::LOG_WARNING:
			strLevel = "warning";
			break;
		case LogLevel::LOG_ERROR:
			strLevel = "error";
			break;
		case LogLevel::LOG_FATAL:
			strLevel = "fatal";
			break;
		default:
			strLevel = "info";
		}
		return strLevel;
	}

	std::string CurrentDateTimeStr()
	{
		//2018-08-29T09:41:37.532100652+08:00
		char dtstr[128];
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
		std::size_t fractional_seconds = ns.count() % 1000000000LL;
		std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ns);
		std::time_t t = s.count();
		std::tm* tm = std::localtime(&t);
		sprintf(dtstr,"%04d-%02d-%02dT%02d:%02d:%02d.%09lu+08:00"
			,tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday
			,tm->tm_hour, tm->tm_min, tm->tm_sec
			,fractional_seconds);
		return dtstr;
	}
};

class GateWayLogContext :public LogContextImp
{
public:
	GateWayLogContext()
		:LogContextImp()
	{
		m_log_file_name = "/var/log/open-trade-gateway/open-trade-gateway.log";
	}
};

class GateWayMsLogContext :public LogContextImp
{
public:
	GateWayMsLogContext()
	{
		m_log_file_name = "/var/log/open-trade-gateway/open-trade-gateway-ms.log";
	}
};

LogContext& Log()
{
	static std::mutex log_file_mutex;
	static std::map<std::string,GateWayLogContext> loggerMap;
	std::lock_guard<std::mutex> lock(log_file_mutex);
	std::stringstream ss;
	ss << std::this_thread::get_id();	
	return loggerMap[ss.str()];
}

LogContext& LogMs()
{
	static std::mutex log_file_mutex;
	static std::map<std::string,GateWayMsLogContext> loggerMap;
	std::lock_guard<std::mutex> lock(log_file_mutex);
	std::stringstream ss;
	ss << std::this_thread::get_id();
	return loggerMap[ss.str()];
}
