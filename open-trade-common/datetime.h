/////////////////////////////////////////////////////////////////////////
///@file datetime.h
///@brief	日期时间处理工具
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <time.h>
#include <string>

typedef int BOOL;
#define TRUE 1
#define FALSE 0

enum DateTimeField 
{
    DTField_Year = 0
    , DTField_Month = 1
    , DTField_Day = 2
    , DTField_Hour = 3
    , DTField_Minute = 4
    , DTField_Second = 5
    , DTField_MicroSecond = 6
};

struct Date 
{
	Date()
		:year(0)
		,month(0)
		,day(0)
	{
	}

    long year;
    long month;
    long day;
};

struct Time 
{
	Time()
		:hour(0)
		, minute(0)
		, second(0)
		, microsecond(0)
	{
	}

    long hour;
    long minute;
    long second;
    long microsecond;
};

struct DateTime 
{
    struct Date date;
    struct Time time;
};

struct TimeDelta 
{
	TimeDelta()
		:days(0)
		, seconds(0)
		, microseconds(0)
	{
	}

    long days;
    long seconds;
    long microseconds;
};

#define INVALIDDATETIME {0,0,0,0,0,0,0}

BOOL SetDate(struct Date* date, long year, long month, long day);
BOOL SetTime(struct Time* time, long hour, long minute, long second, long microsecond);

BOOL LongToDate(long orid, struct Date* date);
BOOL LongLongToTime(long long orid, struct Time* time);
BOOL DateToLong(const struct Date* date, long* n);
BOOL TimeToLongLong(const struct Time* time, long long* n);
BOOL DateTimeToLongLong(const struct DateTime* datetime, long long* n);

/*
	@brief: 返回日期对应的周日
	@retval: 星期1 = 0，星期2=1，...星期日=6. 如果日期为非法值，返回-1
*/
long GetDateWeek(const struct Date* date);

/*
	计算从fromday到today一共有多少个工作日
*/
long GetWorkdayOffset(const struct Date* fromday, const struct Date* today);

/*
	按照工作日移动日期, 跳过周六和周日, 正数右移，负数左移
*/
BOOL MoveDateByWorkday(struct Date* date, long workdays);

BOOL AdjustDate(struct Date* date, const struct TimeDelta* delta);
BOOL SubDateTime(const struct DateTime* from, const struct DateTime* to, struct TimeDelta* delta);
BOOL SubTime(const struct Time* from, const struct Time* to, struct TimeDelta* delta);
long TotalHours(const struct TimeDelta* delta);
long TotalMinutes(const struct TimeDelta* delta);
long TotalSeconds(const struct TimeDelta* delta);
long long TotalMicroSeconds(const struct TimeDelta* delta);

long CmpDate(const struct Date* date1, const struct Date* date2);
long CmpTime(const struct Time* time1, const struct Time* time2);
long CmpDateTime(const struct DateTime* datetime1, const struct DateTime* datetime2);
long CmpDateTimeField(const struct DateTime* datetime1, const struct DateTime* datetime2, enum DateTimeField field);

BOOL SetDateTime(struct DateTime* datetime, long year, long month, long day, long hour, long minute, long second, long microsecond);
void SetDateTimeNow( struct DateTime* datetime );
void SetDateTimeInvalid( struct DateTime* datetime );
void SetTimeInvalid(struct Time* time);

BOOL AlignTime(struct Time* target, long hour, long minute, long second, long microsecond);
BOOL AlignDateByDays(struct Date* target, long days);
BOOL AlignDateToWeek(struct Date* target);
BOOL AlignDateToMonth(struct Date* target);
BOOL AlignDateToYear(struct Date* target);
BOOL AdjustDateTime( struct DateTime* datetime, const struct TimeDelta* delta );
BOOL CreateTimeDelta(long days, long seconds, long long microseconds, struct TimeDelta* timedelta );

BOOL IsDateTimeValid(const struct DateTime* datetime);
BOOL IsTimeValid(const struct Time* time);

void DateTimeToString(char* buf, long buflen, const struct DateTime* dt);
void SetDateTimeMax( struct DateTime* datetime );
void SetDateTimeFromTimeT(struct DateTime* datetime, time_t* src);
const char* DateTimeGetString(const struct DateTime* v);
void SetDateTimeFromEpochNano(struct DateTime* datetime, long long epoch_nano);
long long DateTimeToEpochNano(const struct DateTime* dt);

void GetTimeFromString(const std::string& str,Time& time);

int DateTimeToEpochSeconds(const DateTime& dt);

void SetDateTimeFromEpochSeconds(struct DateTime* datetime,int epochSeconds);