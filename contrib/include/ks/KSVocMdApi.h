/////////////////////////////////////////////////////////////////////////
///@system 新一代交易系统
///@company SunGard China
///@file KSVocMDApi.h
///@brief 定义了客户端客户定制行情接口
/////////////////////////////////////////////////////////////////////////

#ifndef __KSVOCMDAPI_H_INCLUDED_
#define __KSVOCMDAPI_H_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KSUserApiStructEx.h"
#include "KSVocApiStruct.h"

#if defined(ISLIB) && defined(WIN32) && !defined(KSMDAPI_STATIC_LIB)
#ifdef LIB_MD_API_EXPORT
#define TRADER_VOCMDAPI_EXPORT __declspec(dllexport)
#else
#define TRADER_VOCMDAPI_EXPORT __declspec(dllimport)
#endif
#else
#ifdef WIN32
#define TRADER_VOCMDAPI_EXPORT 
#else
#define TRADER_VOCMDAPI_EXPORT __attribute__((visibility("default")))
#endif

#endif

namespace KingstarAPI
{

	class CKSVocMdSpi
	{
	public:
		///订阅行情应答
		virtual void OnRspSubKSMarketData(CKSSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///取消订阅行情应答
		virtual void OnRspUnSubKSMarketData(CKSSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///订阅询价应答
		virtual void OnRspSubKSForQuoteRsp(CKSSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///取消订阅询价应答
		virtual void OnRspUnSubKSForQuoteRsp(CKSSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	};

	class TRADER_VOCMDAPI_EXPORT CKSVocMdApi
	{
	public:
		///订阅行情。
		///@param ppInstrumentID 合约
		///@param nCount 要订阅/退订行情的合约个数
		///@remark 
		virtual int SubscribeKSMarketData(CKSSpecificInstrumentField *ppInstrumentID[], int nCount) = 0;

		///退订行情。
		///@param ppInstrumentID 合约 
		///@param nCount 要订阅/退订行情的合约个数
		///@remark 
		virtual int UnSubscribeKSMarketData(CKSSpecificInstrumentField *ppInstrumentID[], int nCount) = 0;

		///订阅询价。
		///@param ppInstrumentID 合约ID  
		///@param nCount 要订阅/退订行情的合约个数
		///@remark 
		virtual int SubscribeKSForQuoteRsp(CKSSpecificInstrumentField *ppInstrumentID[], int nCount) = 0;

		///退订询价。
		///@param ppInstrumentID 合约ID  
		///@param nCount 要订阅/退订行情的合约个数
		///@remark 
		virtual int UnSubscribeKSForQuoteRsp(CKSSpecificInstrumentField *ppInstrumentID[], int nCount) = 0;

		///高华认证邮箱登录请求
		virtual int ReqKSEMailLogin(CKSReqEMailLoginField *pReqEMailLoginField, int nRequestID) = 0;

	protected:
		~CKSVocMdApi(){};
	};

}	// end of namespace KingstarAPI
#endif