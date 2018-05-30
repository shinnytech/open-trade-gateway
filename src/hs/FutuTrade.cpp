/*******************************************************************************************************/
/*
/*    1、此C++ Demo使用异步发送接收模式；
/*    2、Demo连接的是恒生仿真测试环境，如要对接券商环境，需要修改t2sdk.ini文件中的IP和许可证文件等；
/*    3、不同环境的验证信息可能不一样，更换连接环境时，包头字段设置需要确认；
/*    4、接口字段说明请参考接口文档"恒生统一接入平台_周边接口规范(期货，期权).xls"；
/*    5、T2函数技术说明参考开发文档“T2SDK 外部版开发指南.docx"；
/*    6、如有UFX接口技术疑问可联系大金融讨论组（261969915）；
/*    7、UFX技术支持网站 https://ufx.hscloud.cn/；
/*    8、demo仅供参考。
/*
/*******************************************************************************************************/
#include "stdafx.h"
#include "FutuTrade.h"
#include "encoding.h"

unsigned long CTradeCallback::QueryInterface(const char* iid, IKnown** ppv)
{
    return 0;
}

unsigned long CTradeCallback::AddRef()
{
    return 0;
}

unsigned long CTradeCallback::Release()
{
    return 0;
}


void CTradeCallback::OnConnect(CConnectionInterface* lpConnection)
{
    puts("CTradeCallback::OnConnect");
}

void CTradeCallback::OnSafeConnect(CConnectionInterface* lpConnection)
{
    puts("CTradeCallback::OnSafeConnect");
}

void CTradeCallback::OnRegister(CConnectionInterface* lpConnection)
{
    puts("CTradeCallback::OnRegister");
}
void CTradeCallback::OnClose(CConnectionInterface* lpConnection)
{
    puts("CTradeCallback::OnClose");
}

void CTradeCallback::OnSent(CConnectionInterface* lpConnection, int hSend, void* reserved1, void* reserved2, int nQueuingData)
{
    puts("CTradeCallback::Onsent");
}

void CTradeCallback::Reserved1(void* a, void* b, void* c, void* d)
{
    puts("CTradeCallback::Reserved1");
}


void CTradeCallback::Reserved2(void* a, void* b, void* c, void* d)
{
    puts("CTradeCallback::Reserved2");
}

void CTradeCallback::OnReceivedBizEx(CConnectionInterface* lpConnection, int hSend, LPRET_DATA lpRetData, const void* lpUnpackerOrStr, int nResult)
{
    puts("CTradeCallback::OnReceivedBizEx");
}
void CTradeCallback::OnReceivedBizMsg(CConnectionInterface* lpConnection, int hSend, IBizMessage* lpMsg)
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
    //int iMsgLen = 0;
    //void * lpMsgBuffer = lpBizMessageRecv->GetBuff(iMsgLen);
    //将lpMsgBuffer拷贝走，然后在其他线程中恢复成消息可进行如下操作：
    //lpBizMessageRecv->SetBuff(lpMsgBuffer,iMsgLen);
    //没有错误信息
    int iLen = 0;
    //取得业务包的内容
    const void* lpBuffer = lpMsg->GetContent(iLen);
    //定义业务包解包器
    IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
    int iLen_ley = 0;
    const void* lpBuffer_key = lpMsg->GetKeyInfo(iLen_ley);
    IF2UnPacker* lpUnPacker_key = NewUnPacker((void*)lpBuffer_key, iLen_ley);
    switch (lpMsg->GetFunction()) {
        //接收到客户登录接口的返回包解包
        case 331100:
            ShowPacket(lpUnPacker);
            OnResponse_331100(lpUnPacker);
            lpUnPacker->AddRef();
            lpUnPacker->Release();
            break;
        //接收到期货委托查询的接口返回包
        case 338301:
            OnResponse_338301(lpUnPacker);
            lpUnPacker->AddRef();
            lpUnPacker->Release();
            break;
        //接收到心跳包
        case MSGCENTER_FUNC_HEART:
            if (lpMsg->GetPacketType() == REQUEST_PACKET) {
                //收到心跳包
                lpReqMode->OnHeartbeat(lpMsg);
            }
            break;
        //接收到订阅的返回包
        case MSGCENTER_FUNC_REG:
            if (lpUnPacker_key->GetInt("error_no") != 0) {
                puts(lpUnPacker_key->GetStr("error_info"));
            }
            ShowPacket(lpUnPacker_key);
            lpUnPacker_key->AddRef();
            lpUnPacker_key->Release();
            break;
        //接收到主推包
        case MSGCENTER_FUNC_SENDED:
            if (lpMsg->GetIssueType() == ISSUE_TYPE_REALTIME_SECU) {
                puts("收到期货委托成交回报！");
                if (lpUnPacker->GetChar("LY") == 'B') {
                    puts("收到期货成交回报");
                    string futuqh = lpUnPacker->GetStr("QH");
                    cout << lpReqMode->replace_all(futuqh, "\001", " | ") << endl;
                    lpUnPacker_key->AddRef();
                    lpUnPacker->AddRef();
                    lpUnPacker->Release();
                    lpUnPacker_key->Release();
                } else {
                    puts("收到期货委托回报");
                    string futuqh = lpUnPacker->GetStr("QH");
                    cout << lpReqMode->replace_all(futuqh, "\001", " | ") << endl;
                    lpUnPacker_key->AddRef();
                    lpUnPacker->AddRef();
                    lpUnPacker->Release();
                    lpUnPacker_key->Release();
                }
            }
            break;
        default:
            puts("业务操作成功,输出参数如下：");
            ShowPacket(lpUnPacker);
            lpUnPacker->Release();
            break;
    }
}

void CTradeCallback::OnReceivedBiz(CConnectionInterface* lpConnection, int hSend, const void* lpUnPackerOrStr, int nResult)
{
}

int  CTradeCallback::Reserved3()
{
    return 0;
}

void CTradeCallback::Reserved4()
{
}

void CTradeCallback::Reserved5()
{
}

void CTradeCallback::Reserved6()
{
}

void CTradeCallback::Reserved7()
{
}

void CTradeCallback::SetRequestMode(CFutuRequestMode* lpMode)
{
    lpReqMode = lpMode;
}

//登陆后获取Usertoken，BranchNo等
void CTradeCallback::OnResponse_331100(IF2UnPacker* lpUnPacker)
{
    int iSystemNo = -1;
    const char* pClientId = lpUnPacker->GetStr("client_id");
    if (pClientId)
        strcpy(lpReqMode->m_client_id, pClientId);
    if (lpUnPacker->GetStr("user_token") != NULL) {
        lpReqMode->m_FutuUserToken = lpUnPacker->GetStr("user_token");
    }
    if (lpUnPacker->GetStr("branch_no") != NULL)
        lpReqMode->m_BranchNo = lpUnPacker->GetInt("branch_no");
    iSystemNo = lpUnPacker->GetInt("sysnode_id");
    if (lpUnPacker->GetInt("op_branch_no") != NULL)
        lpReqMode->m_op_branch_no = lpUnPacker->GetInt("op_branch_no");
    return;
}
//
void CTradeCallback::OnResponse_338301(IF2UnPacker* lpUnPacker)
{
    if (lpUnPacker->GetInt("error_no") != 0) {
        cout << lpUnPacker->GetStr("error_info") << endl;
        return;
    } else {
        int ct = lpUnPacker->GetRowCount();
        string pos = "";
        while (!lpUnPacker->IsEOF()) {
            const char* lpStrPos = lpUnPacker->GetStr("position_str");
            if ( lpStrPos == 0)
                pos = "";
            else
                pos = lpStrPos;
            //cout<<lpUnPacker->GetStr("contract_code")<<"|"<<lpUnPacker->GetStr("contract_name")<<endl;
            lpUnPacker->Next();
        }
        if ( pos.length() != 0) {
            ShowPacket(lpUnPacker);
            lpReqMode->ReqFunction338301(pos.c_str(), "");
        }
    }
    return;
}


//----------------------------------------CTraderRequestMode-----------------------------------------
//链接初始化函数
int CFutuRequestMode::InitConn()
{
    int r = lpConfig->Load("t2sdk.ini");
    const char* p_fund_account = lpConfig->GetString("ufx", "fund_account", "");
    const char* p_password = lpConfig->GetString("ufx", "password", "");
    strcpy(m_FutuAccountName, p_fund_account);
    strcpy(m_FutuPassword, p_password);
    m_FutuEntrustWay = '7';
    m_FutuStation = "IPMAC";
    //配置连接对象
    //lpConfig->SetString("t2sdk", "servers", serverAddr);
    ////cout<<"start checking license"<<endl;
    //lpConfig->SetString("t2sdk", "license_file", licFile);
    ////cout<<"start loading clientname"<<endl;
    //lpConfig->SetString("t2sdk", "login_name", clientName);
    //cout<<"Connect Successful"<<endl;
    //如果接入ar设置了safe_level，则需要做以下代码
    //begin
    //lpConfig->SetString("safe", "safe_level", "ssl");
    //lpConfig->SetString("safe", "cert_file", "c20121011.pfx");
    //lpConfig->SetString("safe", "cert_pwd", "111111");
    //end
    int iRet = 0;
    if (lpConnection != NULL) {
        lpConnection->Release();
        lpConnection = NULL;
    }
    lpConnection = NewConnection(lpConfig);
    const char* p_server = lpConfig->GetString("t2sdk", "servers", "");
    lpConnection->AddRef();
    if (0 != (iRet = lpConnection->Create2BizMsg(&callback))) {
        cerr << "初始化失败.iRet=" << iRet << " msg:" << lpConnection->GetErrorMsg(iRet) << endl;
        return -1;
    }
    if (0 != (iRet = lpConnection->Connect(5000))) {
        const char* err_msg = lpConnection->GetErrorMsg(iRet);
        std::wstring err_unicode = Utf8ToWide(GBKToUTF8(err_msg).c_str());
        return -1;
    }
    return 0;
}

unsigned long CFutuRequestMode::Release()
{
    delete this;
    return 0;
};

void CFutuRequestMode::SendPack(int func_no, void* content, int len)
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    //设置包头
    lpBizMessage->SetFunction(func_no);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    //设置包内容
    lpBizMessage->SetContent(content, len);
    ////把打入包的内容打印出来，可以不打印注释掉
    //int iLen = 0;
    //const void * lpBuffer = lpBizMessage->GetContent(iLen);
    //IF2UnPacker * lpUnPacker = NewUnPacker((void *)lpBuffer, iLen);
    //lpUnPacker->AddRef();
    //ShowPacket(lpUnPacker);
    //lpUnPacker->Release();
    //把包发送出去
    lpConnection->SendBizMsg(lpBizMessage, 1);
    lpBizMessage->Release();
}

////维护心跳
void CFutuRequestMode::OnHeartbeat(IBizMessage* lpMsg)
{
    lpMsg->ChangeReq2AnsMessage();
    lpConnection->SendBizMsg(lpMsg, 1);
    return;
}

//331100期货客户登陆
int CFutuRequestMode::ReqFunction331100()
{
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    ///加入字段名
    pPacker->AddField("op_branch_no", 'I', 5);                 //操作分支机构
    pPacker->AddField("op_entrust_way", 'C', 1);               //委托方式
    pPacker->AddField("op_station", 'S', 255);                 //站点地址
    pPacker->AddField("branch_no", 'I', 5);                    //分支机构
    pPacker->AddField("input_content", 'C');                   //客户标志类别
    pPacker->AddField("account_content", 'S', 30);             //输入内容
    pPacker->AddField("content_type", 'S', 6);                 //银行号、市场类别
    pPacker->AddField("password", 'S', 10);                    //密码
    pPacker->AddField("password_type", 'C');                   //密码类型
    ///加入对应的字段值
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());
    pPacker->AddInt(m_BranchNo);
    pPacker->AddChar('1');
    pPacker->AddStr("8000017");
    pPacker->AddStr("0");
    pPacker->AddStr("666666");
    pPacker->AddChar('2');
    pPacker->EndPack();
    SendPack(331100, pPacker->GetPackBuf(), pPacker->GetPackLen());
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    return 0;
}

//338202期货委托确认
int CFutuRequestMode::ReqFunction338202(char* g_exchange_type, char* g_futu_code, char g_entrust_bs, char g_futures_direction, int g_entrust_amount, double g_futu_entrust_price)
{
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    ///加入字段名
    pPacker->AddField("op_branch_no", 'I', 5);                  //操作分支机构
    pPacker->AddField("op_entrust_way", 'C', 1);                //委托方式
    pPacker->AddField("op_station", 'S', 255);                  //站点地址
    pPacker->AddField("branch_no", 'I', 5);                     //分支机构
    pPacker->AddField("client_id", 'S', 18);                    //客户编号
    pPacker->AddField("fund_account", 'S', 18);                 //资产账户
    pPacker->AddField("password", 'S', 10);	                    //密码
    pPacker->AddField("user_token", 'S', 40);                   //user_token
    pPacker->AddField("futu_exch_type", 'S', 4);                //交易类别
    pPacker->AddField("futures_account", 'S', 12);              //交易编码
    pPacker->AddField("futu_code", 'S', 30);                    //合约代码
    pPacker->AddField("entrust_bs", 'C', 1);                    //买卖方向
    pPacker->AddField("futures_direction", 'C', 1);             //开平仓方向
    pPacker->AddField("hedge_type", 'C', 1);                    //投机/套保类型
    pPacker->AddField("entrust_amount", 'I');                   //委托数量
    pPacker->AddField("futu_entrust_price", 'F', 12, 6);        //委托价格
    pPacker->AddField("entrust_prop", 'S', 3);                  //委托属性
    pPacker->AddField("entrust_occasion", 'S', 32);	            //委托场景
    pPacker->AddField("entrust_reference", 'S', 32);	        //委托引用
    //加入对应的字段值
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());
    pPacker->AddInt(m_BranchNo);
    pPacker->AddStr(m_client_id);
    pPacker->AddStr(m_FutuAccountName);
    pPacker->AddStr(m_FutuPassword);
    pPacker->AddStr(m_FutuUserToken.c_str());
    pPacker->AddStr(g_exchange_type);
    pPacker->AddStr("");
    pPacker->AddStr(g_futu_code);
    pPacker->AddChar(g_entrust_bs);
    pPacker->AddChar(g_futures_direction);
    pPacker->AddChar('1');
    pPacker->AddInt(g_entrust_amount);
    pPacker->AddDouble(g_futu_entrust_price);
    pPacker->AddStr("");
    pPacker->AddStr("");
    pPacker->AddStr("");
    ///结束打包
    pPacker->EndPack();
    //设置包内容
    SendPack(338202, pPacker->GetPackBuf(), pPacker->GetPackLen());
    //释放资源
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    return 0;
}

//338317期货撤单委托
int CFutuRequestMode::ReqFunction338217(int g_entrust_no)
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    //设置包头
    lpBizMessage->SetFunction(338217);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    ///加入字段名
    pPacker->AddField("op_branch_no", 'I', 5);                    //操作分支机构
    pPacker->AddField("op_entrust_way", 'C', 1);                  //委托方式
    pPacker->AddField("op_station", 'S', 255);                    //站点地址
    pPacker->AddField("branch_no", 'I', 5);			              //分支机构
    pPacker->AddField("client_id", 'S', 18);			          //客户编号
    pPacker->AddField("fund_account", 'S', 18);		              //资产账户
    pPacker->AddField("password", 'S', 10);						  //密码
    pPacker->AddField("user_token", 'S', 40);                     //用户口令
    pPacker->AddField("futu_exch_type", 'S', 4);                  //交易类别
    pPacker->AddField("entrust_no", 'I', 8);                      //委托编号
    //pPacker->AddField("confirm_id", 'S', 20);                   //主场单号-非必输字段
    //pPacker->AddField("session_no", 'I', 8);                    //会话编号-非必输字段
    //pPacker->AddField("entrust_occasion", 'S', 32);	          //委托场景-非必输字段
    //pPacker->AddField("entrust_reference", 'S', 32);	          //委托引用-非必输字段
    //加入对应的字段值
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());
    pPacker->AddInt(m_BranchNo);
    pPacker->AddStr(m_client_id);
    pPacker->AddStr(m_FutuAccountName);
    pPacker->AddStr(m_FutuPassword);
    pPacker->AddStr(m_FutuUserToken.c_str());
    pPacker->AddStr("F1");
    pPacker->AddInt(g_entrust_no);
    //结束打包
    pPacker->EndPack();
    //设置包的内容
    lpBizMessage->SetContent(pPacker->GetPackBuf(), pPacker->GetPackLen());
    //把打入包里的内容打印出来，可以不打印注释掉
    int iLen = 0;
    const void* lpBuffer = lpBizMessage->GetContent(iLen);
    cout << "338317期货委托撤单入参如下：" << endl;
    IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
    lpUnPacker->AddRef();
    ShowPacket(lpUnPacker);
    lpUnPacker->Release();
    //把包发送出去
    lpConnection->SendBizMsg(lpBizMessage, 1);
    //释放资源
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    lpBizMessage->Release();
    return 0;
}
//338301期货委托查询
int CFutuRequestMode::ReqFunction338301(const char* position_str, const char* request_num)
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    //设置包头
    lpBizMessage->SetFunction(338301);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    //加入字段名
    pPacker->AddField("op_branch_no", 'I', 5);
    pPacker->AddField("op_entrust_way", 'C', 1);
    pPacker->AddField("op_station", 'S', 255);
    pPacker->AddField("branch_no", 'I', 5);
    pPacker->AddField("client_id", 'S', 18);
    pPacker->AddField("fund_account", 'S', 18);
    pPacker->AddField("password", 'S', 10);
    pPacker->AddField("user_token", 'S', 40);
    pPacker->AddField("position_str", 'S'); //定位串
    pPacker->AddField("request_num", 'S'); //请求行数
    //加入对应的字段值
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());
    pPacker->AddInt(m_BranchNo);
    pPacker->AddStr(m_client_id);
    pPacker->AddStr(m_FutuAccountName);
    pPacker->AddStr(m_FutuPassword);
    pPacker->AddStr(m_FutuUserToken.c_str());
    pPacker->AddStr(position_str);
    pPacker->AddStr(request_num);
    //结束打包
    pPacker->EndPack();
    //设置包的内容
    lpBizMessage->SetContent(pPacker->GetPackBuf(), pPacker->GetPackLen());
    //把打入包的内容打印出来，可以不打印注释掉
    int iLen = 0;
    const void* lpBuffer = lpBizMessage->GetContent(iLen);
    cout << "338301期货委托查询入参如下：" << endl;
    IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
    lpUnPacker->AddRef();
    ShowPacket(lpUnPacker);
    lpUnPacker->Release();
    //把包发送出去
    lpConnection->SendBizMsg(lpBizMessage, 1);
    //释放资源
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    lpBizMessage->Release();
    return 0;
}
//338302期货成交查询
int CFutuRequestMode::ReqFunction338302()
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    //设置包头
    lpBizMessage->SetFunction(338302);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    //加入字段名
    pPacker->AddField("op_branch_no", 'I', 5);
    pPacker->AddField("op_entrust_way", 'C', 1);
    pPacker->AddField("op_station", 'S', 255);
    pPacker->AddField("branch_no", 'I', 5);
    pPacker->AddField("client_id", 'S', 18);
    pPacker->AddField("fund_account", 'S', 18);
    pPacker->AddField("password", 'S', 10);
    pPacker->AddField("password_type", 'C', 1);
    pPacker->AddField("user_token", 'S', 40);
    pPacker->AddField("futu_exch_type", 'S', 4);
    //加入对应的字段值
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());
    pPacker->AddInt(m_BranchNo);
    pPacker->AddStr(m_client_id);
    pPacker->AddStr(m_FutuAccountName);
    pPacker->AddStr(m_FutuPassword);
    pPacker->AddChar('2');
    pPacker->AddStr(m_FutuUserToken.c_str());
    pPacker->AddStr("");
    //结束打包
    pPacker->EndPack();
    //设置包的内容
    lpBizMessage->SetContent(pPacker->GetPackBuf(), pPacker->GetPackLen());
    //打印包里的内容，可以不打印注释掉
    int iLen = 0;
    const void* lpBuffer = lpBizMessage->GetContent(iLen);
    cout << "338302期货成交查询入参如下：" << endl;
    IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
    lpUnPacker->AddRef();
    ShowPacket(lpUnPacker);
    lpUnPacker->Release();
    //把包发送出去
    lpConnection->SendBizMsg(lpBizMessage, 1);
    //释放资源
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    lpBizMessage->Release();
    return 0;
}
//338303期货持仓查询
int CFutuRequestMode::ReqFunction338303()
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    lpBizMessage->SetFunction(338303);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    ///加入字段名
    pPacker->AddField("op_branch_no", 'I', 5);
    pPacker->AddField("op_entrust_way", 'C', 1);
    pPacker->AddField("op_station", 'S', 255);
    pPacker->AddField("branch_no", 'I', 5);
    pPacker->AddField("client_id", 'S', 18);
    pPacker->AddField("fund_account", 'S', 18);
    pPacker->AddField("password", 'S', 10);
    pPacker->AddField("user_token", 'S', 40);
    //加入对应的字段值
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());
    pPacker->AddInt(m_BranchNo);
    pPacker->AddStr(m_client_id);
    pPacker->AddStr(m_FutuAccountName);
    pPacker->AddStr(m_FutuPassword);
    pPacker->AddStr(m_FutuUserToken.c_str());
    ///结束打包
    pPacker->EndPack();
    //设置包的内容
    lpBizMessage->SetContent(pPacker->GetPackBuf(), pPacker->GetPackLen());
    //打印包里的内容，可以不打印注释掉
    int iLen = 0;
    const void* lpBuffer = lpBizMessage->GetContent(iLen);
    cout << "338303期货持仓查询入参如下：" << endl;
    IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
    lpUnPacker->AddRef();
    ShowPacket(lpUnPacker);
    lpUnPacker->Release();
    //把包发送出去
    lpConnection->SendBizMsg(lpBizMessage, 1);
    //释放资源
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    lpBizMessage->Release();
    return 0;
}
//620001_33011期权订阅委托成交回报
int CFutuRequestMode::SubFunction33101(int g_issue_type)
{
    IBizMessage* lpBizMessage = NewBizMessage();
    lpBizMessage->AddRef();
    //设置包头
    lpBizMessage->SetFunction(MSGCENTER_FUNC_REG);
    lpBizMessage->SetPacketType(REQUEST_PACKET);
    lpBizMessage->SetIssueType(g_issue_type);
    IF2Packer* pPacker = NewPacker(2);
    if (!pPacker) {
        printf("取打包器失败！\r\n");
        return -1;
    }
    pPacker->AddRef();
    pPacker->BeginPack();
    //加入字段名
    pPacker->AddField("branch_no", 'I', 5);
    pPacker->AddField("fund_account", 'S', 18);
    pPacker->AddField("op_branch_no", 'I', 5);
    pPacker->AddField("op_entrust_way", 'C', 1);
    pPacker->AddField("op_station", 'S', 255);
    pPacker->AddField("client_id", 'S', 18);
    pPacker->AddField("password", 'S', 10);
    pPacker->AddField("user_token", 'S', 40);
    pPacker->AddField("issue_type", 'I', 8);
    pPacker->AddField("noreport_flag", 'C', 1);
    //加入对应的字段值
    pPacker->AddInt(m_BranchNo);
    pPacker->AddStr(m_FutuAccountName);
    pPacker->AddInt(m_op_branch_no);
    pPacker->AddChar(m_FutuEntrustWay);
    pPacker->AddStr(m_FutuStation.c_str());	//op_station
    pPacker->AddStr(m_client_id);
    pPacker->AddStr(m_FutuPassword);
    pPacker->AddStr(m_FutuUserToken.c_str());
    pPacker->AddInt(g_issue_type);
    pPacker->AddChar('1');
    ///结束打包
    pPacker->EndPack();
    //设置内容
    lpBizMessage->SetKeyInfo(pPacker->GetPackBuf(), pPacker->GetPackLen());
    //打印内容，可以不打印注释掉
    int iLen = 0;
    const void* lpBuffer = lpBizMessage->GetKeyInfo(iLen);
    cout << "620003订阅委托成交回报入参如下：" << endl;
    IF2UnPacker* lpUnPacker = NewUnPacker((void*)lpBuffer, iLen);
    lpUnPacker->AddRef();
    ShowPacket(lpUnPacker);
    lpUnPacker->Release();
    //发送出去
    lpConnection->SendBizMsg(lpBizMessage, 1);
    pPacker->FreeMem(pPacker->GetPackBuf());
    pPacker->Release();
    lpBizMessage->Release();
    return 0;
}

string& CFutuRequestMode::replace_all (string& str, const string& old_value, const string& new_value)
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