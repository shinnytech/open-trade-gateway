/////////////////////////////////////////////////////////////////////////
///@file utility.cpp
///@brief	工具函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "Utility.h"
#include <filesystem>

std::string GenerateUniqFileName()
{
    static std::atomic_int sno = 1;
    std::experimental::filesystem::path p = std::experimental::filesystem::temp_directory_path();
    char file_name_buffer[1024];
    long process_id = GetCurrentProcessId();
    SYSTEMTIME systime;
    GetLocalTime(&systime);
    sprintf_s(file_name_buffer, sizeof(file_name_buffer) - 1, ("t%d%04d%02d%02d%02d%02d%02d%03d%08d")
              , process_id
              , systime.wYear, systime.wMonth, systime.wDay, systime.wMonth, systime.wMinute, systime.wSecond, systime.wMilliseconds
              , sno.fetch_add(1)
             );
    p /= std::string(file_name_buffer);
    return p.string();
}