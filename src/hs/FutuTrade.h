#include "tool.h"
#define  MSGCENTER_FUNC_HEART		 620000                                           //消息中心心跳功能号
#define  MSGCENTER_FUNC_REG			 620001                                           //消息中心订阅功能号
#define  MSGCENTER_FUNC_REG_CANCLE   620002                                           //消息中心取消订阅功能号
#define  MSGCENTER_FUNC_SENDED		 620003                                           //消息中心主推功能号
#define  ISSUE_TYPE_REALTIME_SECU     33101                                           //订阅期货委托成交回报

class CFutuRequestMode;
// 自定义类CCallback，通过继承（实现）CCallbackInterface，来自定义各种事件（包括连接成功、
// 连接断开、发送完成、收到数据等）发生时的回调方法
class CTradeCallback : public CCallbackInterface
{
public:
    // 因为CCallbackInterface的最终纯虚基类是IKnown，所以需要实现一下这3个方法
    unsigned long  FUNCTION_CALL_MODE QueryInterface(const char* iid, IKnown** ppv);
    unsigned long  FUNCTION_CALL_MODE AddRef();
    unsigned long  FUNCTION_CALL_MODE Release();

    // 各种事件发生时的回调方法，实际使用时可以根据需要来选择实现，对于不需要的事件回调方法，可直接return
    // Reserved?为保留方法，为以后扩展做准备，实现时可直接return或return 0。
    void FUNCTION_CALL_MODE OnConnect(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnSafeConnect(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnRegister(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnClose(CConnectionInterface* lpConnection);
    void FUNCTION_CALL_MODE OnSent(CConnectionInterface* lpConnection, int hSend, void* reserved1, void* reserved2, int nQueuingData);
    void FUNCTION_CALL_MODE Reserved1(void* a, void* b, void* c, void* d);
    void FUNCTION_CALL_MODE Reserved2(void* a, void* b, void* c, void* d);
    int  FUNCTION_CALL_MODE Reserved3();
    void FUNCTION_CALL_MODE Reserved4();
    void FUNCTION_CALL_MODE Reserved5();
    void FUNCTION_CALL_MODE Reserved6();
    void FUNCTION_CALL_MODE Reserved7();
    void FUNCTION_CALL_MODE OnReceivedBiz(CConnectionInterface* lpConnection, int hSend, const void* lpUnPackerOrStr, int nResult);
    void FUNCTION_CALL_MODE OnReceivedBizEx(CConnectionInterface* lpConnection, int hSend, LPRET_DATA lpRetData, const void* lpUnpackerOrStr, int nResult);
    void FUNCTION_CALL_MODE OnReceivedBizMsg(CConnectionInterface* lpConnection, int hSend, IBizMessage* lpMsg);
public:
    void SetRequestMode(CFutuRequestMode* lpMode);
    //331100 登入
    void OnResponse_331100(IF2UnPacker* lpUnPacker);
    void OnResponse_338301(IF2UnPacker* lpUnPacker);
private:
    CFutuRequestMode* lpReqMode;
};

class CFutuRequestMode
{
public:
    CFutuRequestMode()
    {
        lpConfig = NULL;
        lpConnection = NULL;
        callback.SetRequestMode(this);
        lpConfig = NewConfig();
        lpConfig->AddRef();
        memset(m_client_id, 0, sizeof(m_client_id));
        m_FutuUserToken = "0";
        m_BranchNo = 0;
        m_op_branch_no = 0;
        l_entrust_reference = 0;
        memset(m_FutuAccountName, 0, sizeof(m_FutuAccountName));
        memset(m_FutuPassword, 0, sizeof(m_FutuPassword));
        m_FutuEntrustWay = '\0';
        m_FutuFuturesAccount = "0";
        m_FutuStation = "0";
    };

    ~CFutuRequestMode()
    {
        lpConnection->Release();
        lpConfig->Release();
    };
    int InitConn();
    unsigned long Release();
public:
    string m_FutuUserToken;
    int m_BranchNo;
    char m_client_id[18];
    int m_op_branch_no;
    int l_entrust_reference;

    //331100 登入
    int ReqFunction331100();
    void SendPack(int func_no, void* content, int len);
    //338202期货委托确认
    int ReqFunction338202(char* g_exchange_type, char* g_futu_code, char g_entrust_bs, char g_futures_direction, int g_entrust_amount, double g_futu_entrust_price);
    //338317期货撤单委托
    int ReqFunction338217(int g_entrust_no);
    //338301期货委托查询
    int ReqFunction338301(const char* position_str, const char* request_num);
    //338302期货成交查询
    int ReqFunction338302();
    //338303期货持仓查询
    int ReqFunction338303();
    //维护心跳
    void OnHeartbeat(IBizMessage* lpMsg);
    //620001_33101期货订阅委托成交回报功能
    int SubFunction33101(int issue_type);
    //字符串替换
    string& replace_all(string& str, const string& old_value, const string& new_value);
private:
    CConfigInterface* lpConfig;
    CConnectionInterface* lpConnection;
    CTradeCallback callback;

    char m_FutuAccountName[12];
    char m_FutuPassword[8];
    char m_FutuEntrustWay;
    string m_FutuFuturesAccount;
    string m_FutuStation;

    int m_SubSystemNo;
};