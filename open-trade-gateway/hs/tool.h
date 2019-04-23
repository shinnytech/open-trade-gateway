/***************************************************************************************************************
         说在使用前的话
// 此C++ Demo使用异步发送接收模式
//Demo连接的是恒生期货的仿真环境，如果要对接券商环境，需要修改配置文件的ip端口，以及license文件，用户名密码
//不同环境的验证信息可能不一样，更换连接环境时，包头字段设置需要确认
//接口字段说明请参考接口文档"恒生统一接入平台_周边接口规范(期货).xls"
//T2函数技术说明参考开发文档“T2SDK 外部版开发指南.docx"
//如有t2技术疑问可联系UFX支持组张亚辉(大金融群 恒生-张亚辉 qq 1007526923)
//demo仅供参考
******************************************************************************************************************/

#ifndef TOOL_H
#define TOOL_H

#ifdef WIN32
#include <Windows.h>
#include <direct.h>
#include <Iphlpapi.h>
#include <process.h>
#endif

#ifdef LINUX
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <dlfcn.h>
#include <assert.h>
#include <poll.h>   // 2011-6-30 add ,for poll
#include <unistd.h>
#include <netdb.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <pwd.h>

struct hostent* gethostbyname(const char* name);

#define  Sleep(x) usleep((x)*1000)
#endif

#include <iostream>
#include <string>
#include <fstream>
#include <math.h>
#include <queue>
#include <map>
#include <winsock.h>
#include "t2sdk_interface.h"


using namespace std;

extern "C" typedef void (*TDFUNC)(void*);

/////////////////////////////////////////////////Function///////////////////////////////////////////////
void ShowPacket(IF2UnPacker* lpUnPacker);
void system_pause(void);
void ShowMessage(const char* Msg);
bool FileExist(char* filename);
bool SaveLog(char* filename, char* strMag);
string NewClientName(const char* srvtype, const char* mac);
bool GetIpAddressByUrl(char* ip, const char* inurl);
char* GetDateByString();
char* GetTimeByString();
int GetTimeByInt();
timeval CurrentTimeTag();
timespec CurrentNTimeTag();

char* hs_strncpy(char* dest, const char* src, size_t size);
char* strtok_t(char* instr, char* delimit, char** saveptr);

/////////////////////////////////////////////////CDate///////////////////////////////////////////////
class CDate
{
public:
    CDate();
    CDate(int year, int month, int day);
    CDate(int date);
    static bool isLeapYear(int year);
    static int GetDaysOfMonth(int year, int month);
    CDate PrevDay();
    CDate PrevWorkingDay();
    bool IsWorkingDay();
    int GetDate();
    void GetTime(char* time, char splitchar);
    int  GetMilliseconds()
    {
        return m_Milliseconds;
    }
    int  GetTimeStamp();
private:
    int m_iyear;
    int m_imonth;
    int m_iday;
    int m_ihour;
    int m_iMinutes;
    int m_iSeconds;
    int m_Milliseconds;
};

/////////////////////////////////////////////////ManualLock///////////////////////////////////////////////
#ifdef WIN32

class ManualLock
{
    CRITICAL_SECTION cs;
public:
    ManualLock()
    {
        InitializeCriticalSection(&cs);
    }
    inline void Lock()
    {
        EnterCriticalSection(&cs);
    }
    inline void UnLock()
    {
        LeaveCriticalSection(&cs);
    }
    ~ManualLock()
    {
        DeleteCriticalSection(&cs);
    }
};
#endif

#ifdef LINUX
class ManualLock
{
    pthread_mutex_t lock;
public:
    ManualLock()
    {
        pthread_mutex_init(&lock, NULL);
    }
    void Lock()
    {
        pthread_mutex_lock(&lock);
    }
    void UnLock()
    {
        pthread_mutex_unlock(&lock);
    }
    ~ManualLock()
    {
        pthread_mutex_destroy(&lock);
    }
};
#endif

////////////////////////////////////////////////OperateSystem////////////////////////////////////////////////
class OperateSystem
{
public:
    static int  getCurrentProcessId();
    static void getComputerName(char* username, int buflen);
    static void getUserName(char* username, int buflen);
};

////////////////////////////////////////////////CThreadSafeValue////////////////////////////////////////////////
class CThreadSafeValue
{
    long volatile value;
#ifdef LINUX
    pthread_mutex_t mutex;
#endif
public:
    CThreadSafeValue(long val);
    long GetValue();
    long Increase();
    long Decrease();
};
//
///////////////////////////////////////////////////CEvent///////////////////////////////////////////////
//class CEvent
//{
//#ifdef WIN32
//	HANDLE hEvent;
//#endif
//#ifdef LINUX
//	pthread_cond_t ptcond;
//	pthread_mutex_t mutex;
//	bool bSignaled;
//#endif
//public:
//	enum EventWaitState{ EVENT_SIGNALED, EVENT_TIMEOUT, EVENT_ERROR};
//	CEvent();
//	~CEvent();
//	void Notify();
//	void Reset();
//	CEvent::EventWaitState Wait(long timeout);
//};

/////////////////////////////////////////////////CThread///////////////////////////////////////////////
#ifdef WIN32
class CThread
{
    HANDLE hThread;
    TDFUNC tdfunc;
    void*  lpvoid;
    bool   isrunning;
    static unsigned int _stdcall Win32TdFunc(void* lpvoid);
public:
    CThread()
    {
        isrunning = false;
    }
    void Start(TDFUNC lpfunc, int stacksize, void* lpvoid);
    void Join();
    bool IsRunning()
    {
        return isrunning;
    }
};
#endif
#ifdef LINUX
class CThread
{
private:
    pthread_t threadid;
    TDFUNC tdfunc;
    void* lpvoid;
    bool isrunning;
    static void* LinuxTdFunc(void* lpvoid);
public:
    void Start(TDFUNC tdfunc, int stacksize, void* lpvoid);
    void Join();
    bool IsRunning()
    {
        return isrunning;
    }

};
#endif
//
//////////////////////////////////////////////////CMessageQueue////////////////////////////////////////////////
//template<typename T> class CMessageQueue
//{
//private:
//	CEvent evt;
//	ManualLock lock;
//	std::queue<T> lst;
//
//	T TryPop()
//	{
//		T ret = NULL;
//		lock.Lock();
//		if( lst.size() <= 0)
//		{
//			ret = NULL;
//		}
//		else
//		{
//			ret = lst.front();
//			lst.pop();
//		}
//		lock.UnLock();
//		return ret;
//	}
//
//public:
//	void Push(T item)
//	{
//		bool bNotify = false;
//		lock.Lock();
//		lst.push(item);
//		if( lst.size() == 1 )
//		{
//			bNotify = true;
//		}
//		lock.UnLock();
//		if( bNotify == true)
//		{
//			evt.Notify();
//		}
//	}
//
//	T Pop(int timeout)
//	{
//		T ret = TryPop();
//		if( ret == NULL && timeout != 0)
//		{
//			evt.Wait(timeout);
//			ret = TryPop();
//		}
//		return ret;
//	}
//};

////////////////////////////////////////////////CLogWriter////////////////////////////////////////////////
class CLogWriter
{
public:
    CLogWriter(const char* szFileName);
    virtual ~CLogWriter();

    void WriteLog(const char* szMsg);
protected:
    FILE* 					m_fp;
    ManualLock				m_lock;
};

#endif
