/////////////////////////////////////////////////////////////////////////
///@file numset.h
///@brief	数值处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef _WIN32
#ifndef NAN
static const unsigned long __nan[2] = {0xffffffff, 0x7fffffff};
#define NAN (*(const float *) __nan)
#endif
#endif

bool IsZero(double x);

bool IsZero(long x);

bool IsZero(long long x);

bool IsValid(double x);

bool IsValid(long x);

bool IsValid(long long x);

bool IsValid(int x);

void SetInvalid(double* x);

void SetInvalid(long* x);

void SetInvalid(long long* x);