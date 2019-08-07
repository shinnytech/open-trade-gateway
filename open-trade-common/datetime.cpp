/////////////////////////////////////////////////////////////////////////
///@file datetime.cpp
///@brief	日期时间处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "datetime.h"
#include <stdlib.h>
#include <stdio.h>
#include <cassert>
#include <algorithm>
#include <vector>

#include <boost/algorithm/string.hpp>


static const long _DAYS_IN_MONTH[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const long _DAYS_BEFORE_MONTH[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static const long MINYEAR = 1;
static const long MAXYEAR = 9999;
static const long _DI400Y = 146097;//    # number of days in 400 years
static const long _DI100Y = 36524;//    #    "    "   "   " 100   "
static const long _DI4Y   = 1461;//      #    "    "   "   "   4   "

void divmod_long(long x, long y, long* divv, long* mod)
{
    *mod = x % y;
    *divv = x / y;
    if (*mod < 0) {
        (*mod) += y;
        (*divv)--;
    }
}
void divmod_longlong(long long x, long long y, long long* div, long long* mod)
{
    *mod = x % y;
    *div = x / y;
    if (*mod < 0) {
        (*mod) += y;
        (*div)--;
    }
}
BOOL _is_leap(long year)
{
    //"year -> 1 if leap year, else 0."
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

long _days_before_year(long year)
{
    //"year -> number of days before January 1st of year."
    long y = year - 1;
    return y * 365 + y / 4 - y / 100 + y / 400;
}

long _days_in_month(long year, long month)
{
    //"year, month -> number of days in that month in that year."
    assert (1 <= month && month <= 12);
    if (month == 2 && _is_leap(year)) {
        return 29;
    }
    return _DAYS_IN_MONTH[month];
}

long _days_before_month(long year, long month)
{
    //"year, month -> number of days in year preceeding first day of month."
    assert (1 <= month && month <= 12);
    return _DAYS_BEFORE_MONTH[month] + (month > 2 && _is_leap(year) ? 1 : 0);
}

long _ymd2ord(long year, long month, long day)
{
    //"year, month, day -> ordinal, considering 01-Jan-0001 as day 1."
    long dim;
    assert (1 <= month && month <= 12);
    dim = _days_in_month(year, month);
    assert (1 <= day && day <= dim);
    return (_days_before_year(year) +
            _days_before_month(year, month) +
            day);
}


//# A 4-year cycle has an extra leap day over what we'd get from pasting
//# together 4 single years.
//	assert _DI4Y == 4 * 365 + 1
//
//# Similarly, a 400-year cycle has an extra leap day over what we'd get from
//# pasting together 4 100-year cycles.
//	assert _DI400Y == 4 * _DI100Y + 1
//
//# OTOH, a 100-year cycle has one fewer leap day than we'd get from
//# pasting together 25 4-year cycles.
//	assert _DI100Y == 25 * _DI4Y - 1
void _ord2ymd( long n, long* year, long* month, long* day )
{
    /*
    "ordinal -> (year, month, day), considering 01-Jan-0001 as day 1."

    # n is a 1-based index, starting at 1-Jan-1.  The pattern of leap years
    # repeats exactly every 400 years.  The basic strategy is to find the
    # closest 400-year boundary at or before n, then work with the offset
    # from that boundary to n.  Life is much clearer if we subtract 1 from
    # n first -- then the values of n at 400-year boundaries are exactly
    # those divisible by _DI400Y:
    #
    #     D  M   Y            n              n-1
    #     -- --- ----        ----------     ----------------
    #     31 Dec -400        -_DI400Y       -_DI400Y -1
    #      1 Jan -399         -_DI400Y +1   -_DI400Y      400-year boundary
    #     ...
    #     30 Dec  000        -1             -2
    #     31 Dec  000         0             -1
    #      1 Jan  001         1              0            400-year boundary
    #      2 Jan  001         2              1
    #      3 Jan  001         3              2
    #     ...
    #     31 Dec  400         _DI400Y        _DI400Y -1
    #      1 Jan  401         _DI400Y +1     _DI400Y      400-year boundary
    */
    long n400 = 0;
    long n100 = 0;
    long n4 = 0;
    long n1 = 0;
    long preceding = 0;
    BOOL leapyear = FALSE;
    n--;
    n400 = n / _DI400Y;
    n = n % _DI400Y;
    *year = n400 * 400 + 1;
    /*
    # Now n is the (non-negative) offset, in days, from January 1 of year, to
    # the desired date.  Now compute how many 100-year cycles precede n.
    # Note that it's possible for n100 to equal 4!  In that case 4 full
    # 100-year cycles precede the desired day, which implies the desired
    # day is December 31 at the end of a 400-year cycle.
    */
    n100 = n / _DI100Y;
    n = n % _DI100Y;
    //# Now compute how many 4-year cycles precede it.
    n4 = n / _DI4Y;
    n = n % _DI4Y;
    /*
    # And now how many single years.  Again n1 can be 4, and again meaning
    # that the desired day is December 31 at the end of the 4-year cycle.
    */
    n1 = n / 365;
    n = n % 365;
    *year += n100 * 100 + n4 * 4 + n1;
    if (n1 == 4 || n100 == 4) {
        assert (n == 0);
        (*year)--;
        *month = 12;
        *day = 31;
        return;
    }
    /*
    # Now the year is correct, and n is the offset from January 1.  We find
    # the month via an estimate that's either exact or one too large.
    */
    leapyear = (n1 == 3 && (n4 != 24 || n100 == 3));
    assert(leapyear == _is_leap(*year));
    *month = (n + 50) >> 5;
    preceding = _DAYS_BEFORE_MONTH[*month] + (*month > 2 && leapyear);
    if (preceding > n) {
        //# estimate is too large
        *month -= 1;
        preceding -= _DAYS_IN_MONTH[*month] + (*month == 2 && leapyear);
    }
    n -= preceding;
    assert (0 <= n && n < _days_in_month(*year, *month));
    /*
    # Now the year and month are correct, and n is the offset from the
    # start of that month:  we're done!
    */
    (*day) = n + 1;
}

BOOL _check_date_fields(long year, long month, long day)
{
    long dim;
    if (year < MINYEAR || year > MAXYEAR) {
        return FALSE;
    }
    if (month < 1 || month > 12) {
        return FALSE;
    }
    dim = _days_in_month(year, month);
    if ( day < 1 || day > dim) {
        return FALSE;
    }
    return TRUE;
}

BOOL _check_time_fields(long hour, long minute, long second, long microsecond)
{
    if (hour < 0 || hour > 23) {
        return FALSE;
    }
    if (minute < 0 || minute > 59) {
        return FALSE;
    }
    if (second < 0 || second > 59) {
        return FALSE;
    }
    if (microsecond < 0 || microsecond > 999999) {
        return FALSE;
    }
    return TRUE;
}

BOOL SetDate( struct Date* date, long year, long month, long day)
{
    if (!_check_date_fields(year, month, day) || !date) {
        return FALSE;
    }
    date->year = year;
    date->month = month;
    date->day = day;
    return TRUE;
}
BOOL SetTime( struct Time* targettime, long hour, long minute, long second, long microsecond)
{
    if (!_check_time_fields(hour, minute, second, microsecond) || !targettime) {
        return FALSE;
    }
    targettime->hour = hour;
    targettime->minute = minute;
    targettime->second = second;
    targettime->microsecond = microsecond;
    return TRUE;
}

BOOL SetTime2( struct Time* time, long second, long microsecond)
{
    long hour = second / 3600;
    long rem = second % 3600;
    long minute = rem / 60;
    long sec = rem % 60;
    return SetTime(time, hour, minute, sec, microsecond);
}

BOOL LongToDate(long orid, struct Date* date)
{
    long year, month, day;
    _ord2ymd(orid, &year, &month, &day);
    return SetDate(date, year, month, day);
}

BOOL LongLongToTime(long long orid, struct Time* time)
{
    time->microsecond = orid % 1000000;
    orid /= 1000000;
    time->second = orid % 60;
    orid /= 60;
    time->minute = orid % 60;
    orid /= 60;
    time->hour = (long)orid;
    if (!_check_time_fields(time->hour, time->minute, time->second, time->microsecond)) {
        return FALSE;
    }
    return TRUE;
}

BOOL DateToLong(const struct Date* date, long* n)
{
    if (!_check_date_fields(date->year, date->month, date->day)) {
        return FALSE;
    }
    *n = _ymd2ord(date->year, date->month, date->day);
    return TRUE;
}

BOOL TimeToLongLong(const struct Time* time, long long* n)
{
    *n = (((time->hour * 60 + time->minute) * 60) + time->second);
    *n = (*n) * 1000000 + time->microsecond;
    if (!_check_time_fields(time->hour, time->minute, time->second, time->microsecond)) {
        return FALSE;
    }
    return TRUE;
}

BOOL CreateTimeDelta(long days, long seconds, long long microseconds, struct TimeDelta* timedelta )
{
    if (!timedelta) {
        return FALSE;
    }
    if (microseconds != 0) {
        long long s;
        divmod_longlong(microseconds, 1000000, &s, &microseconds);
        seconds += (long)s;
    }
    if (seconds != 0) {
        long d;
        divmod_long(seconds, 24 * 3600, &d, &seconds);
        days += d;
    }
    timedelta->days = days;
    timedelta->seconds = seconds;
    timedelta->microseconds = (long)(microseconds);
    if (abs(timedelta->days) > 999999999) {
        return FALSE;
    }
    return TRUE;
}

BOOL AdjustDate( struct Date* date, const struct TimeDelta* delta )
{
    long ord;
    if (!DateToLong(date, &ord)) {
        return FALSE;
    }
    ord += delta->days;
    if (!LongToDate(ord, date)) {
        return FALSE;
    }
    return TRUE;
}

BOOL AddTimeDelta(struct TimeDelta* target, const struct TimeDelta* source)
{
    return CreateTimeDelta(
               target->days + source->days
               , target->seconds + source->seconds
               , target->microseconds + source->microseconds
               , target);
}

BOOL AdjustDateTime( struct DateTime* datetime, const struct TimeDelta* delta )
{
    long orddays;
    long long orduseconds;
    struct TimeDelta d2;
    if (!DateToLong(&datetime->date, &orddays)) {
        return FALSE;
    }
    if (!TimeToLongLong(&datetime->time, &orduseconds)) {
        return FALSE;
    }
    if (!CreateTimeDelta(orddays, 0, orduseconds, &d2)) {
        return FALSE;
    }
    if (!AddTimeDelta(&d2, delta)) {
        return FALSE;
    }
    if (!LongToDate(d2.days, &datetime->date)) {
        return FALSE;
    }
    return SetTime2(&datetime->time, d2.seconds, d2.microseconds);
}

BOOL AlignTime(struct Time* target, long hour, long minute, long second, long microsecond)
{
    long long ms = (long long)microsecond + ((((long long)hour * 60) + (long long)minute) * 60 + (long long)second) * 1000000;
    long long orign;
    if (!TimeToLongLong(target, &orign)) {
        return FALSE;
    }
    orign -= orign % ms;
    if (!LongLongToTime(orign, target)) {
        return FALSE;
    }
    return TRUE;
}

BOOL AlignDateByDays(struct Date* target, long days)
{
    //将日期向前调整到最接近的整N天
    long orign;
    if (!DateToLong(target, &orign)) {
        return FALSE;
    }
    orign -= orign % days;
    if (!LongToDate(orign, target)) {
        return FALSE;
    }
    return TRUE;
}

BOOL AlignDateToWeek(struct Date* target)
{
    //将日期向前调整到最接近的周一
    return TRUE;
}
BOOL AlignDateToMonth(struct Date* target)
{
    //将日期向前调整到最接近的月初
    return TRUE;
}
BOOL AlignDateToYear(struct Date* target)
{
    //将日期向前调整到最接近的年初
    return TRUE;
}


long CmpDateTimeGeneral(const long* s1, const long* s2, long count)
{
    long i;
    for (i = 0; i < count; i++) {
        if (*(s1 + i) > *(s2 + i)) {
            return 1;
        }
        if (*(s1 + i) < * (s2 + i)) {
            return -1;
        }
    }
    return 0;
}

long CmpDate( const struct Date* date1, const struct Date* date2 )
{
    return CmpDateTimeGeneral((long*)(date1), (long*)(date2), 3);
}

long CmpTime( const struct Time* time1, const struct Time* time2 )
{
    return CmpDateTimeGeneral((long*)(time1), (long*)(time2), 4);
}

long CmpDateTime( const struct DateTime* datetime1, const struct DateTime* datetime2 )
{
    return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 7);
}

long CmpDateTimeField( const struct DateTime* datetime1, const struct DateTime* datetime2, enum DateTimeField field )
{
    switch (field) {
        case DTField_Year:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 1);
        case DTField_Month:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 2);
        case DTField_Day:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 3);
        case DTField_Hour:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 4);
        case DTField_Minute:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 5);
        case DTField_Second:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 6);
        case DTField_MicroSecond:
            return CmpDateTimeGeneral((long*)(datetime1), (long*)(datetime2), 7);
        default: {
            assert (FALSE);
            return 0;
        }
    }
}

void SetDateTimeNow( struct DateTime* datetime )
{
    time_t now = 0;
    time(&now);
    SetDateTimeFromTimeT(datetime, &now);
}

void SetDateTimeInvalid( struct DateTime* datetime )
{
    datetime->date.year = 0;
    datetime->date.month = 0;
    datetime->date.day = 0;
    datetime->time.hour = 0;
    datetime->time.minute = 0;
    datetime->time.second = 0;
    datetime->time.microsecond = 0;
}
void SetDateTimeMax( struct DateTime* datetime )
{
    datetime->date.year = 9999;
    datetime->date.month = 1;
    datetime->date.day = 1;
    datetime->time.hour = 0;
    datetime->time.minute = 0;
    datetime->time.second = 0;
    datetime->time.microsecond = 0;
}
BOOL SubDateTime( const struct DateTime* from, const struct DateTime* to, struct TimeDelta* delta )
{
    long days1, days2, secs1, secs2;
    if (!DateToLong(&from->date, &days1) || !DateToLong(&to->date, &days2)) {
        return FALSE;
    }
    secs1 = from->time.second + from->time.minute * 60 + from->time.hour * 3600;
    secs2 = to->time.second + to->time.minute * 60 + to->time.hour * 3600;
    if (!CreateTimeDelta(days2 - days1, secs2 - secs1, to->time.microsecond - from->time.microsecond, delta)) {
        return FALSE;
    }
    return TRUE;
}

long TotalSeconds( const struct TimeDelta* delta )
{
    return delta->days * 86400 + delta->seconds;
}

long long TotalMicroSeconds( const struct TimeDelta* delta )
{
    return ((long long)delta->days * 86400 + (long long)delta->seconds) * 1000000 + (long long)delta->microseconds;
}

long TotalMinutes( const struct TimeDelta* delta )
{
    return (delta->days * 86400 + delta->seconds) / 60;
}

long TotalHours( const struct TimeDelta* delta )
{
    return (delta->days * 86400 + delta->seconds) / 3600;
}

BOOL IsDateTimeValid( const struct DateTime* datetime )
{
    return _check_date_fields(datetime->date.year, datetime->date.month, datetime->date.day)
           && _check_time_fields(datetime->time.hour, datetime->time.minute, datetime->time.second, datetime->time.microsecond);
}

void SetDateTimeFromTimeT(struct DateTime* datetime, time_t* src)
{
    struct tm* p;
    p = localtime(src);
    datetime->date.year = p->tm_year + 1900;
    datetime->date.month = p->tm_mon + 1;
    datetime->date.day = p->tm_mday;
    datetime->time.hour = p->tm_hour;
    datetime->time.minute = p->tm_min;
    datetime->time.second = p->tm_sec;
    datetime->time.microsecond = 0;
}

void SetDateTimeFromEpochNano(struct DateTime* datetime, long long epoch_nano)
{
    time_t seconds = epoch_nano / 1000000000;
    SetDateTimeFromTimeT(datetime, &seconds);
    datetime->time.microsecond = (epoch_nano % 1000000000ll) / 1000;
}

void SetDateTimeFromEpochSeconds(struct DateTime* datetime,int epochSeconds)
{
	time_t seconds = epochSeconds;
	SetDateTimeFromTimeT(datetime, &seconds);
	datetime->time.microsecond = 0;
}

void SetTimeInvalid(struct Time* time )
{
    time->hour = 0xFF;
    time->minute = 0xFF;
    time->second = 0xFF;
    time->microsecond = 0xFF;
}

BOOL IsTimeValid( const struct Time* time )
{
    return _check_time_fields(time->hour, time->minute, time->second, time->microsecond);
}

#ifdef _WIN32
#define snprintf _snprintf
#endif

void DateTimeToString( char* buf, long buflen, const struct DateTime* v )
{
    snprintf(buf, buflen, "%04ld%02ld%02ld%02ld%02ld%02ld%06ld"
             , v->date.year
             , v->date.month
             , v->date.day
             , v->time.hour
             , v->time.minute
             , v->time.second
             , v->time.microsecond);
}

BOOL SetDateTime( struct DateTime* datetime, long year, long month, long day, long hour, long minute, long second, long microsecond )
{
    assert(datetime);
    if (!datetime || !_check_date_fields(year, month, day) || !_check_time_fields(hour, minute, second, microsecond)) {
        return FALSE;
    }
    datetime->date.year = year;
    datetime->date.month = month;
    datetime->date.day = day;
    datetime->time.hour = hour;
    datetime->time.minute = minute;
    datetime->time.second = second;
    datetime->time.microsecond = microsecond;
    return TRUE;
}

long GetDateWeek( const struct Date* date )
{
    long c;
    if (DateToLong(date, &c) == FALSE) {
        return -1;
    }
    return (c + 6) % 7;
}

long GetWorkdayOffset(const struct Date* fromday, const struct Date* today )
{
    long days1, days2, difday;
    long weeks, w1, w2;
    long dir = 1;
    if (CmpDate(fromday, today) > 0) {
        const struct Date* t = fromday;
        fromday = today;
        today = t;
        dir = -1;
    }
    if (!DateToLong(fromday, &days1) || !DateToLong(today, &days2)) {
        return FALSE;
    }
    difday = days2 - days1;
    weeks = difday / 7;
    difday -= weeks * 2;
    w1 = GetDateWeek(fromday);
    w2 = GetDateWeek(today);
    if (w2 > w1 && w2 >= 5) {
        if (w1 >= 5) {
            difday -= (w2 - w1);
        } else {
            difday -= (w2 - 4);
        }
    }
    if (w2 < w1) {
        if (w1 <= 5) {
            difday -= 2;
        } else if (w1 == 6) {
            difday -= 1;
        }
    }
    return difday * dir;
}

BOOL MoveDateByWorkday( struct Date* date, long workdays )
{
    long weeks;
    long leftdays;
    long weekday;
    long totaldays = 0;
    long dir = (workdays > 0);
    struct TimeDelta delta;
    if (workdays == 0) {
        return TRUE;
    }
    weekday = GetDateWeek(date);
    //先假装定位到所处周的周1或周5
    if (workdays > 0) {
        workdays += std::min(weekday, (long)4);
    } else {
        workdays -= std::min((11 - weekday) % 7, (long)4);
    }
    //整周的移动
    weeks = workdays / 5;
    leftdays = workdays % 5;
    //最后再调整
    totaldays = weeks * 7 + leftdays;
    if (dir) {
        totaldays -= weekday;
    } else {
        totaldays += ((11 - weekday) % 7);
    }
    CreateTimeDelta(totaldays, 0, 0, &delta);
    return AdjustDate(date, &delta);
}

BOOL SubTime( const struct Time* from, const struct Time* to, struct TimeDelta* delta )
{
    long secs1, secs2;
    secs1 = from->second + from->minute * 60 + from->hour * 3600;
    secs2 = to->second + to->minute * 60 + to->hour * 3600;
    if (!CreateTimeDelta(0, secs2 - secs1, to->microsecond - from->microsecond, delta)) {
        return FALSE;
    }
    return TRUE;
}

BOOL DateTimeToLongLong( const struct DateTime* datetime, long long* n )
{
    /*
    	16		4		5		5		6		6		20
    	YYYY	MM		DD		hh		mm		s		microsecond
    */
    *n = 0
         | ((long long)datetime->date.year << 46)
         | ((long long)datetime->date.month << 42)
         | ((long long)datetime->date.day << 37)
         | ((long long)datetime->time.hour << 32)
         | ((long long)datetime->time.minute << 26)
         | ((long long)datetime->time.second << 20)
         | ((long long)datetime->time.microsecond);
    return TRUE;
}

const char* DateTimeGetString(const struct DateTime* v)
{
    static char buf[10][128];
    static long c = 0;
    c = (c + 1) % 10;
    sprintf(buf[c], "%04ld/%02ld/%02ld-%02ld:%02ld:%02ld:%06ld"
            , v->date.year
            , v->date.month
            , v->date.day
            , v->time.hour
            , v->time.minute
            , v->time.second
            , v->time.microsecond);
    return buf[c];
}

long long DateTimeToEpochNano(const struct DateTime* dt)
{
    struct tm p;
    time_t t;
    long long nano;
    p.tm_year = dt->date.year - 1900;
    p.tm_mon = dt->date.month - 1;
    p.tm_mday = dt->date.day;
    p.tm_hour = dt->time.hour;
    p.tm_min = dt->time.minute;
    p.tm_sec = dt->time.second;
	p.tm_isdst = 0;
    t = mktime(&p);
    nano = (long long)t * 1000000000 + dt->time.microsecond * 1000;
    return nano;
}

int DateTimeToEpochSeconds(const DateTime& dt)
{
	struct tm p;
	time_t t;
	p.tm_year = dt.date.year - 1900;
	p.tm_mon = dt.date.month - 1;
	p.tm_mday = dt.date.day;
	p.tm_hour = dt.time.hour;
	p.tm_min = dt.time.minute;
	p.tm_sec = dt.time.second;
	p.tm_isdst = 0;
	t = mktime(&p);
	return t;
}

void GetTimeFromString(const std::string& str, Time& time)
{
	std::vector<std::string> hms;
	boost::algorithm::split(hms,str,boost::algorithm::is_any_of(":"));
	if (hms.size() != 3)
	{
		return;
	}

	time.hour = atoi(hms[0].c_str());
	time.minute= atoi(hms[1].c_str());
	time.second= atoi(hms[2].c_str());
	time.microsecond = 0;
}