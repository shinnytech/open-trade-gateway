#include "stdafx.h"
#include "hs_callback.h"
#include "encoding.h"
#include "trader_hs.h"

namespace trader_dll
{

static std::string& replace_all(std::string& str, const std::string& old_value, const std::string& new_value)
{
    while (true) {
        string::size_type pos(0);
        if ((pos = str.find(old_value)) != string::npos)
            str.replace(pos, old_value.length(), new_value);
        else
            break;
    }
    return str;
}

void HsCallback::OnConnect(CConnectionInterface* lpConnection)
{
    m_trader->Input("on_connect", std::string());
}

void HsCallback::OnSafeConnect(CConnectionInterface* lpConnection)
{
    puts("HsCallback::OnSafeConnect");
}

void HsCallback::OnRegister(CConnectionInterface* lpConnection)
{
    puts("HsCallback::OnRegister");
}

void HsCallback::OnClose(CConnectionInterface* lpConnection)
{
    puts("HsCallback::OnClose");
}

void HsCallback::OnSent(CConnectionInterface* lpConnection, int hSend, void* reserved1, void* reserved2, int nQueuingData)
{
    puts("HsCallback::Onsent");
}

void HsCallback::OnReceivedBizEx(CConnectionInterface* lpConnection, int hSend, LPRET_DATA lpRetData, const void* lpUnpackerOrStr, int nResult)
{
    puts("HsCallback::OnReceivedBizEx");
}

void HsCallback::OnReceivedBizMsg(CConnectionInterface* lpConnection, int hSend, IBizMessage* lpMsg)
{
    if (!lpMsg)
        return;
    //成功,应用程序不能释放lpBizMessageRecv消息
    if (lpMsg->GetErrorNo() != 0) {
        //有错误信息
        int iLen = 0;
        const void* lpBuffer = lpMsg->GetContent(iLen);
        IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
        lpUnPacker->AddRef();//添加释放内存引用
        ShowPacket(lpUnPacker);
        lpUnPacker->Release();
        return;
    }
    //如果要把消息放到其他线程处理，必须自行拷贝，操作如下：
    int func_id = lpMsg->GetFunction();
    char func_id_str[32];
    sprintf(func_id_str, "%06d", func_id);
    int iMsgLen = 0;
    void* lpMsgBuffer = lpMsg->GetBuff(iMsgLen);
    m_trader->Input(func_id_str, std::string((char*)lpMsgBuffer, iMsgLen));
}

void HsCallback::OnReceivedBiz(CConnectionInterface* lpConnection, int hSend, const void* lpUnPackerOrStr, int nResult)
{
}

}