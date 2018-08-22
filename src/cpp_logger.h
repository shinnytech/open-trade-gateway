/////////////////////////////////////////////////////////////////////////
///@file cpp_logger.h
///@brief	高速带校验log机制
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <regex>
#include <zlib/zlib.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")

#ifdef _NLOGDEBUG
#define LOG_DEBUG(...)
#else
#define LOG_DEBUG(...)		CppLogger::GetRootLogger().Debug( __VA_ARGS__ )
#endif //_DEBUG
#define LOG_INFO(...)		CppLogger::GetRootLogger().Info( __VA_ARGS__ )
#define LOG_NOTICE(...)		CppLogger::GetRootLogger().Notice( __VA_ARGS__ )
#define LOG_WARNING(...)	CppLogger::GetRootLogger().Warning( __VA_ARGS__ )
#define LOG_ERROR(...)		CppLogger::GetRootLogger().Error( __VA_ARGS__ )
#define LOG_FATAL(...)		CppLogger::GetRootLogger().Fatal( __VA_ARGS__ )

typedef enum tagLogLevel { Log_Debug, Log_Info, Log_Notice, Log_Warning, Log_Error, Log_Fatal } LogLevel;

struct LogEntry {
	LogLevel Level;
	long long Timestamp;
	unsigned long ThreadId;
	std::string Msg;
	LogEntry(LogLevel level, long long timestamp, unsigned long threadId, const char* msg)
		: Level(level), Timestamp(timestamp), ThreadId(threadId), Msg(msg) {
	}
};

struct OutputStream {
	void* FileHandle;
	unsigned int Socket;
	std::string Host;
	LogLevel Level;
	std::string LogFileName;
	bool UseFilter;
	std::regex Filter;
	bool Compress;
	z_stream CompressStream;
	unsigned int LastCRC32;
};

struct OutputFile {
	LogLevel Level;
	const char* Dir;
	const char* File;
	const char* Filter;
	bool Compress;
};

char* fast_subdtoa(char* str, double value, int prec = 6);
char* fast_strcpy(char* dst, const char* src);
char* fast_ui2xa(char* dst, unsigned int value);
char* fast_json(char* dst, const char* src);

class CppLogger 
{
private:
	CppLogger();
	CppLogger(CppLogger const&);					// Don't Implement
	void operator=(CppLogger const&);				// Don't implement
	static const char* Level2String(LogLevel level);
	static char Level2Syslog(LogLevel level);
	void LogMsg(LogLevel level, const std::string& context, bool nofmt, const char* fmt, va_list arglist);
	static unsigned long __stdcall LoggerRoutine(void* lpParameter);
	bool* Inited;
	std::string Context;

public:
	CppLogger(const std::string& context);
	CppLogger(CppLogger& parant, const std::string& context);
	~CppLogger();
	static CppLogger& GetRootLogger();
	bool Init(const struct OutputFile* outputs);
	void Exit();
	int GetLogFileCount();
	const std::string& GetLogFile(int index);
	void Debug(const char* fmt, ...);
	void Info(const char* fmt, ...);
	void Notice(const char* fmt, ...);
	void Warning(const char* fmt, ...);
	void Error(const char* fmt, ...);
	void Fatal(const char* fmt, ...);
	void Log(LogLevel level, const char* fmt, ...);
	void LogV(LogLevel level, const char* fmt, va_list arglist);
	void LogM(LogLevel level, const char* msg);
};
