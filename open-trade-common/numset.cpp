/////////////////////////////////////////////////////////////////////////
///@file numset.cpp
///@brief	数值处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "numset.h"
#include <float.h>
#include <limits.h>
#include <cmath>

bool IsZero(double x)
{
    return (x > -DBL_EPSILON && x < DBL_EPSILON);
}

bool IsZero(long x)
{
    return x == 0;
}

bool IsZero( long long x )
{
    return (x == (long long)0);
}

bool IsValid(double x)
{
    return (!std::isnan(x)) && (x < 1e20) && (x > -1e20);
}

bool IsValid(long x)
{
    return x != LONG_MAX;
}

bool IsValid(int x)
{
    return x != INT_MAX;
}

bool IsValid( long long x )
{
    return x != LLONG_MAX;
}

void SetInvalid( double* x )
{
    *x = NAN;
}

void SetInvalid(long* x)
{
    *x = LONG_MAX;
}

void SetInvalid(long long* x)
{
    *x = LLONG_MAX;
}


