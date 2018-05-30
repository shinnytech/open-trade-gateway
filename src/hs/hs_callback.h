#include "tool.h"
#define  MSGCENTER_FUNC_HEART		 620000                                           //消息中心心跳功能号
#define  MSGCENTER_FUNC_REG			 620001                                           //消息中心订阅功能号
#define  MSGCENTER_FUNC_REG_CANCLE   620002                                           //消息中心取消订阅功能号
#define  MSGCENTER_FUNC_SENDED		 620003                                           //消息中心主推功能号
#define  ISSUE_TYPE_REALTIME_SECU     33101                                           //订阅期货委托成交回报

namespace trader_dll
{

class TraderHs;

class HsCallback
    : public CCallbackInterface
{
public:
    // 因为CCallbackInterface的最终纯虚基类是IKnown，所以需要实现一下这3个方法
    unsigned long  FUNCTION_CALL_MODE QueryInterface(const char* iid, IKnown** ppv)
    {
        return 0;
    }
    unsigned long  FUNCTION_CALL_MODE AddRef()
    {
        return 0;
    }
    unsigned long  FUNCTION_CALL_MODE Release()
    {
        return 0;
    }

    // 各种事件发生时的回调方法，实际使用时可以根据需要来选择实现，对于不需要的事件回调方法，可直接return
    // Reserved?为保留方法，为以后扩展做准备，实现时可直接return或return 0。
    void FUNCTION_CALL_MODE OnConnect(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnSafeConnect(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnRegister(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnClose(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnSent(CConnectionInterface* lpConnection, int hSend, void* reserved1, void* reserved2, int nQueuingData);
    void FUNCTION_CALL_MODE Reserved1(void* a, void* b, void* c, void* d) {};
    void FUNCTION_CALL_MODE Reserved2(void* a, void* b, void* c, void* d) {};
    int  FUNCTION_CALL_MODE Reserved3()
    {
        return 0;
    }
    void FUNCTION_CALL_MODE Reserved4() {};
    void FUNCTION_CALL_MODE Reserved5() {};
    void FUNCTION_CALL_MODE Reserved6() {};
    void FUNCTION_CALL_MODE Reserved7() {}
    void FUNCTION_CALL_MODE OnReceivedBiz(CConnectionInterface* lpConnection, int hSend, const void* lpUnPackerOrStr, int nResult);
    void FUNCTION_CALL_MODE OnReceivedBizEx(CConnectionInterface* lpConnection, int hSend, LPRET_DATA lpRetData, const void* lpUnpackerOrStr, int nResult);
    void FUNCTION_CALL_MODE OnReceivedBizMsg(CConnectionInterface* lpConnection, int hSend, IBizMessage* lpMsg);

public:
    //331100 登入
    void OnResponse_331100(IF2UnPacker* lpUnPacker);
    void OnResponse_338301(IF2UnPacker* lpUnPacker);

    trader_dll::TraderHs* m_trader;
};
}
