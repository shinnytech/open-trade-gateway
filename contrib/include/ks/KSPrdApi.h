/////////////////////////////////////////////////////////////////////////
///@system 新一代交易系统
///@company SunGard China
///@file KSPrdApi.h
///@brief 定义了客户端主数据业务接口
/////////////////////////////////////////////////////////////////////////

#ifndef __KSPRDAPI_H_INCLUDED__
#define __KSPRDAPI_H_INCLUDED__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KSUserApiStructEx.h"
#include "KSVocApiStruct.h"

#if defined(ISLIB) && defined(WIN32) && !defined(KSTRADEAPI_STATIC_LIB)

#ifdef LIB_TRADER_API_EXPORT
#define TRADER_PRDAPI_EXPORT __declspec(dllexport)
#else
#define TRADER_PRDAPI_EXPORT __declspec(dllimport)
#endif
#else
#ifdef WIN32
#define TRADER_PRDAPI_EXPORT 
#else
#define TRADER_PRDAPI_EXPORT __attribute__((visibility("default")))
#endif

#endif

namespace KingstarAPI
{
	class CKSPrdSpi
	{
	public:
		///主数据业务订阅通知
		virtual void OnRtnForSubscribed(CKSPrimeDataBusinessField *pPrimeDataBusiness) {};
		///主数据归档中
		virtual void OnRtnArchiving() {};
		///主数据细节归档
		virtual void OnRtnDetailArchived(KS_TABLEID_TYPE nTableID) {};
		///主数据归档完成
		virtual void OnRtnArchived(TThostFtdcDescriptionType availabeDescription,TThostFtdcDateType availabeTradingDay) {};// 高嵩 增加可用描述 2016年2月1日
	};

	class CKSPrdApi
	{
	public:
		///定制数据业务
		virtual int ReqSubPrimeData(CKSSubPrimeDataBusinessField *pSubPrimeDataBusiness) = 0;
		///填充数据业务
		virtual void ReqFillPrimeData(void) = 0;
		///销毁数据表
		virtual void DestroyPrimeDataDetail(void) = 0;
		// ResetDBEnviroment
		virtual void ResetDBEnviroment() = 0;

	protected:
		~CKSPrdApi(){};
	};

}	// end of namespace KingstarAPI
#endif