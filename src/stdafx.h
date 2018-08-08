// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once
#pragma warning(disable:4800)
#pragma warning(disable:4828)
#pragma warning(disable:4482)
#pragma warning(disable:28159)  //建议使用GetTickCount64, 但XP不支持

#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers
#endif


// 如果要为以前的 Windows 平台生成应用程序，请包括 WinSDKVer.h，并将
// 将 _WIN32_WINNT 宏设置为要支持的平台，然后再包括 SDKDDKVer.h。

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_VISTA     // Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#include <SDKDDKVer.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

// a small number of common controls only work correctly for UNICODE builds.
// However, if you need non-UNICODE build and are only using "standard" Windows
// controls (e.g. no List Views or Tree Views, etc.) it's ok to remove the #ifdef
//#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
//#endif

#include <vector>
#include <string>
#include <map>
#include <set>
#include <list>
#include <array>
#include <queue>
#include <functional>
#include <memory>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <typeinfo>
#include <tuple>
#include <cmath>
#include <utility>
#include <concurrent_queue.h>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <atlstr.h>
#include <atlwin.h>
#include "cpp_logger.h"

#undef min
#undef max

#ifdef _DEBUG
#define DASSERT(_Expression) if(!(_Expression)) DebugBreak();
#else
#define DASSERT(_Expression) ((void)0)
#endif
