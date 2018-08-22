/////////////////////////////////////////////////////////////////////////
///@file cpp_logger.cpp
///@brief	高速带校验log机制
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "cpp_logger.h"
#include <cassert>
#include <deque>
#include <algorithm>
#include <stdint.h>
#include <shlwapi.h>
#include <WinSock2.h>


using namespace std;

deque<LogEntry>* LogQueue = NULL;
CRITICAL_SECTION QueueLock;
HANDLE LogThread = INVALID_HANDLE_VALUE;
HANDLE LogEvent = INVALID_HANDLE_VALUE;
vector<OutputStream> Outputs;


char* fast_subdtoa(char* str, double value, int prec) {
	assert(prec != 0);
	assert(value >= 0);
	assert(value < 1);
	static double powers_of_10[] = {
		1,
		10,
		100,
		1000,
		10000,
		100000,
		1000000,
		10000000,
		100000000,
		1000000000
	};
	// prec will be 1-7
	prec &= 0x7;

	double tmp = value * powers_of_10[prec];
	unsigned int frac = (unsigned int)(tmp);

	int count = prec;
	str[count] = 0;
	do {
		str[--count] = (char)(48 + (frac % 10));
	} while (frac /= 10);
	while (count > 0) str[--count] = '0';
	return str + prec;
}

char* fast_strcpy(char* dst, const char* src) {
	while (*src) {
		*dst++ = *src++;
	}
	*dst = 0;
	return dst;
}

char* fast_ui2xa(char* dst, unsigned int value) {
	static uint16_t table[] = {
		12336, 12592, 12848, 13104, 13360, 13616, 13872, 14128, 14384, 14640, 16688, 16944, 17200, 17456, 17712, 17968,
		12337, 12593, 12849, 13105, 13361, 13617, 13873, 14129, 14385, 14641, 16689, 16945, 17201, 17457, 17713, 17969,
		12338, 12594, 12850, 13106, 13362, 13618, 13874, 14130, 14386, 14642, 16690, 16946, 17202, 17458, 17714, 17970,
		12339, 12595, 12851, 13107, 13363, 13619, 13875, 14131, 14387, 14643, 16691, 16947, 17203, 17459, 17715, 17971,
		12340, 12596, 12852, 13108, 13364, 13620, 13876, 14132, 14388, 14644, 16692, 16948, 17204, 17460, 17716, 17972,
		12341, 12597, 12853, 13109, 13365, 13621, 13877, 14133, 14389, 14645, 16693, 16949, 17205, 17461, 17717, 17973,
		12342, 12598, 12854, 13110, 13366, 13622, 13878, 14134, 14390, 14646, 16694, 16950, 17206, 17462, 17718, 17974,
		12343, 12599, 12855, 13111, 13367, 13623, 13879, 14135, 14391, 14647, 16695, 16951, 17207, 17463, 17719, 17975,
		12344, 12600, 12856, 13112, 13368, 13624, 13880, 14136, 14392, 14648, 16696, 16952, 17208, 17464, 17720, 17976,
		12345, 12601, 12857, 13113, 13369, 13625, 13881, 14137, 14393, 14649, 16697, 16953, 17209, 17465, 17721, 17977,
		12353, 12609, 12865, 13121, 13377, 13633, 13889, 14145, 14401, 14657, 16705, 16961, 17217, 17473, 17729, 17985,
		12354, 12610, 12866, 13122, 13378, 13634, 13890, 14146, 14402, 14658, 16706, 16962, 17218, 17474, 17730, 17986,
		12355, 12611, 12867, 13123, 13379, 13635, 13891, 14147, 14403, 14659, 16707, 16963, 17219, 17475, 17731, 17987,
		12356, 12612, 12868, 13124, 13380, 13636, 13892, 14148, 14404, 14660, 16708, 16964, 17220, 17476, 17732, 17988,
		12357, 12613, 12869, 13125, 13381, 13637, 13893, 14149, 14405, 14661, 16709, 16965, 17221, 17477, 17733, 17989,
		12358, 12614, 12870, 13126, 13382, 13638, 13894, 14150, 14406, 14662, 16710, 16966, 17222, 17478, 17734, 17990,
	};
	uint16_t* cptr = (uint16_t*)(dst + 6);
	uint8_t* vptr = (uint8_t*)&value;
	*cptr-- = table[*vptr++];
	*cptr-- = table[*vptr++];
	*cptr-- = table[*vptr++];
	*cptr-- = table[*vptr++];
	dst += 8;
	*dst = 0;
	return dst;
}

char* fast_json(char* dst, const char* src) {
	for (; *src; src++) {
		switch (*src) {
		case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
		case '"': *dst++ = '\\'; *dst++ = '"'; break;
		case '/': *dst++ = '\\'; *dst++ = '/'; break;
		case '\b': *dst++ = '\\'; *dst++ = 'b'; break;
		case '\f': *dst++ = '\\'; *dst++ = 'f'; break;
		case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
		case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
		case '\t': *dst++ = '\\'; *dst++ = 't'; break;
		default: *dst++ = *src; break;
		}
	}
	*dst = 0;
	return dst;
}

class TimeHelper {
private:
	ULARGE_INTEGER BaseLineFileTime;
	long long BaseLineTimestamp;
	long long TimestampFrequency;
	std::string LastTimeString;
	long long LastTimestamp;
	long long NextTimestamp;
	TimeHelper() {
		LARGE_INTEGER feq;
		SYSTEMTIME currentSystemTime;
		FILETIME currentFileTime;
		QueryPerformanceFrequency(&feq);
		TimestampFrequency = feq.QuadPart;
		BaseLineTimestamp = GetCurrentTimestamp();
		GetSystemTime(&currentSystemTime);
		SystemTimeToFileTime(&currentSystemTime, &currentFileTime);
		BaseLineFileTime.LowPart = currentFileTime.dwLowDateTime;
		BaseLineFileTime.HighPart = currentFileTime.dwHighDateTime;
		BaseLineFileTime.QuadPart += 28800 * 10000000LL;
		// align baseline to second
		BaseLineTimestamp -= (long long)((BaseLineFileTime.QuadPart % 10000000) / 10000000.0 * TimestampFrequency);
		BaseLineFileTime.QuadPart -= BaseLineFileTime.QuadPart % 10000000;
		RecalcLastTimeString(BaseLineTimestamp);
	}
	TimeHelper(TimeHelper const&);					// Don't Implement
	void operator=(TimeHelper const&);				// Don't implement
	void RecalcLastTimeString(long long timestamp) {
		char time[128];
		ULARGE_INTEGER currentTime;
		SYSTEMTIME currentSystemTime;
		FILETIME currentFileTime;
		long long seconds = (timestamp - BaseLineTimestamp) / TimestampFrequency;
		LastTimestamp = BaseLineTimestamp + seconds * TimestampFrequency;
		NextTimestamp = LastTimestamp + TimestampFrequency;
		assert(timestamp >= LastTimestamp && timestamp < NextTimestamp);
		currentTime.QuadPart = BaseLineFileTime.QuadPart + seconds * 10000000LL;
		currentFileTime.dwLowDateTime = currentTime.LowPart;
		currentFileTime.dwHighDateTime = currentTime.HighPart;
		FileTimeToSystemTime(&currentFileTime, &currentSystemTime);
		sprintf(time, "%04d/%02d/%02d-%02d:%02d:%02d."
			, currentSystemTime.wYear
			, currentSystemTime.wMonth
			, currentSystemTime.wDay
			, currentSystemTime.wHour
			, currentSystemTime.wMinute
			, currentSystemTime.wSecond
			);
		LastTimeString = time;
	}
public:
	~TimeHelper() {
	}
	static TimeHelper& GetInstance() {
		static TimeHelper instance; // Guaranteed to be destroyed.
		// Instantiated on first use.
		return instance;
	}
	char* GetTimeString(long long timestamp, char* str) {
		if (timestamp < LastTimestamp || timestamp >= NextTimestamp) RecalcLastTimeString(timestamp);
		assert(NextTimestamp == LastTimestamp + TimestampFrequency);
		assert(timestamp >= LastTimestamp && timestamp < NextTimestamp);
		str = fast_strcpy(str, LastTimeString.c_str());
		return fast_subdtoa(str, (timestamp - LastTimestamp) / (double)TimestampFrequency, 6);
	}
	inline long long GetCurrentTimestamp() {
		LARGE_INTEGER result;
		QueryPerformanceCounter(&result);
		return result.QuadPart;
	}
};


CppLogger::CppLogger()
	: Context("") {
	Inited = new bool(false);
	LogQueue = new deque<LogEntry>();
	InitializeCriticalSection(&QueueLock);
}

CppLogger::CppLogger(const std::string& context)
	: Inited(NULL) {
	Context = '[' + context + ']';
}

CppLogger::CppLogger(CppLogger& parant, const std::string& context)
	: Inited(NULL) {
	Context = parant.Context + '[' + context + ']';
}

CppLogger::~CppLogger() {
	if (Inited) {
		assert(LogQueue);
		assert(*Inited == false);
		DeleteCriticalSection(&QueueLock);
		delete Inited;
		delete LogQueue;
		LogQueue = NULL;
	}
}

CppLogger& CppLogger::GetRootLogger() {
	static CppLogger root; // Guaranteed to be destroyed.
	// Instantiated on first use.
	return root;
}

bool CppLogger::Init(const struct OutputFile* outputs) {
	// Only root logger can be inited
	assert(Inited != NULL);
	assert(LogQueue != NULL);
	assert(*Inited == false);
	assert(Outputs.empty());
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) return false;
	for (; outputs->Dir != NULL || outputs->File != NULL || outputs->Filter != NULL; outputs++) {
		char LogPath[MAX_PATH];
		if (outputs->File && outputs->File[0] != 0) {
			PathCombineA(LogPath, outputs->Dir, outputs->File);
		}
		else {
			char LogName[MAX_PATH];
			TimeHelper::GetInstance().GetTimeString(TimeHelper::GetInstance().GetCurrentTimestamp(), LogName);
			for (int i = 0;; i++) {
				if (LogName[i] == 0) break;
				if (LogName[i] == '/') LogName[i] = '.';
				else if (LogName[i] == ':') LogName[i] = '.';
			}
			sprintf(LogName + strlen(LogName), ".%d.log", GetCurrentProcessId());
			PathCombineA(LogPath, outputs->Dir, LogName);
		}

		OutputStream out;
		ZeroMemory(&out.CompressStream, sizeof(out.CompressStream));
		out.Level = outputs->Level;
		out.LogFileName = LogPath;
		out.LastCRC32 = 0xCDD1099E;
		out.Compress = outputs->Compress;
		if (outputs->Filter && outputs->Filter[0] != 0) {
			out.UseFilter = true;
			out.Filter = regex(outputs->Filter, regex_constants::optimize);
		}
		else {
			out.UseFilter = false;
		}
		if (out.LogFileName == "DebugOutput") {
			out.FileHandle = INVALID_HANDLE_VALUE;
			out.Socket = INVALID_SOCKET;
		}
		else {
			out.FileHandle = CreateFileA(out.LogFileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			out.Socket = INVALID_SOCKET;
			if (out.FileHandle == INVALID_HANDLE_VALUE) goto HandleCleanUp;
		}
		if (out.Compress == true) {
			if (deflateInit(&out.CompressStream, Z_BEST_SPEED) != Z_OK) {
				if (out.FileHandle != INVALID_HANDLE_VALUE) CloseHandle(out.FileHandle);
				if (out.Socket != INVALID_SOCKET) closesocket(out.Socket);
				goto HandleCleanUp;
			}
			// write magic number
			DWORD magic = 0xC0391093;
			DWORD bytesWritten;
			if (out.FileHandle != INVALID_HANDLE_VALUE) WriteFile(out.FileHandle, &magic, sizeof(magic), &bytesWritten, NULL);
		}
		Outputs.push_back(out);
	}
	LogEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (LogEvent == INVALID_HANDLE_VALUE) goto HandleCleanUp;
	LogThread = CreateThread(NULL, 0, LoggerRoutine, NULL, 0, NULL);
	if (LogThread == NULL) goto EventHandleCleanUp;
	*Inited = true;
	goto Exit;

EventHandleCleanUp:
	CloseHandle(LogEvent);
HandleCleanUp:
	while (Outputs.empty() == false) {
		if (Outputs.back().FileHandle != INVALID_HANDLE_VALUE) CloseHandle(Outputs.back().FileHandle);
		if (Outputs.back().Socket != INVALID_SOCKET) closesocket(Outputs.back().Socket);
		if (Outputs.back().Compress == true) deflateEnd(&Outputs.back().CompressStream);
		Outputs.pop_back();
	}
NetworkCleanUp:
	WSACleanup();

Exit:
	return *Inited;
}

void CppLogger::Exit() {
	assert(Inited != NULL);
	if (*Inited == false) return;
	LogEntry exitEntry(LogLevel::Log_Fatal, -1, 0, "");
	EnterCriticalSection(&QueueLock);
	LogQueue->push_back(exitEntry);
	LeaveCriticalSection(&QueueLock);
	SetEvent(LogEvent);
	WaitForSingleObject(LogThread, INFINITE);
	CloseHandle(LogThread);
	CloseHandle(LogEvent);
	while (Outputs.empty() == false) {
		if (Outputs.back().FileHandle != INVALID_HANDLE_VALUE) CloseHandle(Outputs.back().FileHandle);
		if (Outputs.back().Socket != INVALID_SOCKET) closesocket(Outputs.back().Socket);
		if (Outputs.back().Compress == true) deflateEnd(&Outputs.back().CompressStream);
		Outputs.pop_back();
	}
	LogThread = INVALID_HANDLE_VALUE;
	LogEvent = INVALID_HANDLE_VALUE;
	WSACleanup();
	*Inited = false;
}

int CppLogger::GetLogFileCount() {
	return Outputs.size();
}

const std::string& CppLogger::GetLogFile(int index) {
	return Outputs[index].LogFileName;
}

const char* CppLogger::Level2String(LogLevel level) {
	switch (level) {
	case LogLevel::Log_Debug:
		return "  DEBUG";
	case LogLevel::Log_Info:
		return "   INFO";
	case LogLevel::Log_Notice:
		return " NOTICE";
	case LogLevel::Log_Warning:
		return "WARNING";
	case LogLevel::Log_Error:
		return "  ERROR";
	case LogLevel::Log_Fatal:
		return "  FATAL";
	default:
		return "UNKNOWN";
	}
}

char CppLogger::Level2Syslog(LogLevel level) {
	switch (level) {
	case LogLevel::Log_Debug:
		return '7';
	case LogLevel::Log_Info:
		return '6';
	case LogLevel::Log_Notice:
		return '5';
	case LogLevel::Log_Warning:
		return '4';
	case LogLevel::Log_Error:
		return '3';
	case LogLevel::Log_Fatal:
		return '2';
	default:
		return '1';
	}
}

void CppLogger::LogMsg(LogLevel level, const std::string& context, bool nofmt, const char* fmt, va_list arglist) {
	assert(this == &CppLogger::GetRootLogger());
	char msg[8192];
	char* index = msg;
	index = fast_strcpy(msg, context.c_str());
	*index++ = ' ';
	if (nofmt) strncpy_s(index, sizeof(msg)-(index - msg), fmt, _TRUNCATE);
	else _vsnprintf_s(index, sizeof(msg)-(index - msg), _TRUNCATE, fmt, arglist);
	EnterCriticalSection(&QueueLock);
	LogQueue->emplace_back(LogEntry(level, TimeHelper::GetInstance().GetCurrentTimestamp(), GetCurrentThreadId(), msg));
	LeaveCriticalSection(&QueueLock);
	if (LogEvent != INVALID_HANDLE_VALUE) SetEvent(LogEvent);
}

DWORD WINAPI CppLogger::LoggerRoutine(LPVOID lpParameter) {
	deque<LogEntry>* BackEndQueue = new deque<LogEntry>();
	DWORD bytesWritten;
	char rawMsg[9000];
	char netMsg[18000];
	char compressRaw[40000];
	char compressNet[40000];
	int lenRaw;
	int lenNet;
	int lenCompressNet;
	char* msg;
	int len;
	while (true) {
		WaitForSingleObject(LogEvent, INFINITE);
		bool isDebuging = IsDebuggerPresent() == TRUE;
		// swap queue
		assert(LogQueue);
		assert(BackEndQueue);
		assert(BackEndQueue->size() == 0);
		deque<LogEntry>* TempQueue = LogQueue;
		EnterCriticalSection(&QueueLock);
		LogQueue = BackEndQueue;
		LeaveCriticalSection(&QueueLock);
		BackEndQueue = TempQueue;
		for (unsigned int i = 0; i < BackEndQueue->size(); i++) {
			LogEntry& Entry = (*BackEndQueue)[i];
			// -1 for exit message
			if (Entry.Timestamp == -1) {
				delete BackEndQueue;
				return 0;
			}
			// reset netMsg & compressNet
			lenNet = lenCompressNet = -1;
			char* dst = TimeHelper::GetInstance().GetTimeString(Entry.Timestamp, rawMsg);
			*dst++ = ' ';
			*dst++ = '-';
			*dst++ = ' ';
			dst = fast_ui2xa(dst, Entry.ThreadId);
			*dst++ = ' ';
			*dst++ = '-';
			*dst++ = ' ';
			dst = fast_strcpy(dst, Level2String(Entry.Level));
			*dst++ = ' ';
			dst = fast_strcpy(dst, Entry.Msg.c_str());
			*dst++ = ' ';
			*dst++ = ':';
			char* crc = dst;
			dst += 8;
			*dst++ = '\r';
			*dst++ = '\n';
			*dst++ = '\0';
			lenRaw = dst - rawMsg - 1;
			for (auto ito = Outputs.begin(); ito != Outputs.end(); ++ito) {
				if (Entry.Level < ito->Level) continue;
				if (ito->FileHandle == INVALID_HANDLE_VALUE && ito->Socket == INVALID_SOCKET && isDebuging == false) continue;
				if (ito->UseFilter == true && regex_search(rawMsg, ito->Filter) == false) continue;
				if (ito->FileHandle != INVALID_HANDLE_VALUE) {
					ito->LastCRC32 = 0;
					*fast_ui2xa(crc, ito->LastCRC32) = '\r';
					len = lenRaw;
					msg = rawMsg;
				}
				else if (ito->Socket != INVALID_SOCKET) {
					if (lenNet == -1) {
						*crc = 0;
						dst = fast_strcpy(netMsg, "{ \"version\": \"1.1\", \"host\": \"");
						dst = fast_strcpy(dst, ito->Host.c_str());
						dst = fast_strcpy(dst, "\", \"level\": \"");
						*dst++ = Level2Syslog(Entry.Level);
						dst = fast_strcpy(dst, "\", \"short_message\": \"");
						dst = fast_json(dst, rawMsg+49);
						dst = fast_strcpy(dst, "\" }");
						lenNet = MultiByteToWideChar(CP_ACP, 0, netMsg, dst - netMsg, (LPWSTR)compressNet, sizeof(compressNet) / sizeof(WCHAR));
						lenNet = WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)compressNet, lenNet, netMsg, sizeof(netMsg), NULL, NULL);
					}
					len = lenNet;
					msg = netMsg;
				}
				else {
					*crc = '\r';
					*(crc + 1) = '\n';
					*(crc + 2) = 0;
					OutputDebugStringA(rawMsg);
					msg = NULL;
					len = 0;
				}
				if (ito->Compress == true && msg != NULL) {
					if (ito->FileHandle != INVALID_HANDLE_VALUE || (ito->Socket != INVALID_SOCKET && lenCompressNet == -1)) {
						assert(sizeof(compressRaw) == sizeof(compressNet));
						char *compressBuff = ito->FileHandle != INVALID_HANDLE_VALUE ? compressRaw : compressNet;
						ito->CompressStream.next_in = (unsigned char *)msg;
						ito->CompressStream.avail_in = len;
						ito->CompressStream.next_out = (unsigned char *)compressBuff;
						ito->CompressStream.avail_out = sizeof(compressRaw);
						deflate(&ito->CompressStream, Z_SYNC_FLUSH);
						assert(ito->CompressStream.avail_in == 0);
						assert(ito->CompressStream.avail_out != 0);
						msg = compressBuff;
						len = ito->CompressStream.next_out - (unsigned char *)compressBuff;
						if (ito->Socket != INVALID_SOCKET) lenCompressNet = len;
					}
					else {
						msg = compressNet;
						len = lenCompressNet;
					}
				}
				if (ito->FileHandle != INVALID_HANDLE_VALUE) WriteFile(ito->FileHandle, msg, len, &bytesWritten, NULL);
				else if (ito->Socket != INVALID_SOCKET) send(ito->Socket, msg, len, 0);
			}
		}
		BackEndQueue->clear();
	}
}

void CppLogger::Debug(const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(LogLevel::Log_Debug, fmt, arglist);
	va_end(arglist);
}

void CppLogger::Info(const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(LogLevel::Log_Info, fmt, arglist);
	va_end(arglist);
}

void CppLogger::Notice(const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(LogLevel::Log_Notice, fmt, arglist);
	va_end(arglist);
}

void CppLogger::Warning(const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(LogLevel::Log_Warning, fmt, arglist);
	va_end(arglist);
}

void CppLogger::Error(const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(LogLevel::Log_Error, fmt, arglist);
	va_end(arglist);
}

void CppLogger::Fatal(const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(LogLevel::Log_Fatal, fmt, arglist);
	va_end(arglist);
}

void CppLogger::Log(LogLevel level, const char* fmt, ...) {
	va_list arglist;
	va_start(arglist, fmt);
	LogV(level, fmt, arglist);
	va_end(arglist);
}

void CppLogger::LogV(LogLevel level, const char* fmt, va_list arglist) {
	CppLogger::GetRootLogger().LogMsg(level, Context, false, fmt, arglist);
}

void CppLogger::LogM(LogLevel level, const char* msg) {
	CppLogger::GetRootLogger().LogMsg(level, Context, true, msg, NULL);
}
