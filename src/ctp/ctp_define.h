/////////////////////////////////////////////////////////////////////////
///@file ctp_define.h
///@brief	CTP接口相关数据结构及序列化函数
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "ctp/ThostFtdcTraderApi.h"
#include "../datetime.h"
#include "../rapid_serialize.h"

namespace md_service {
struct Instrument;
}

namespace trader_dll
{
struct LocalOrderKey
{
    std::string user_id;
    std::string order_id;
    bool operator < (const LocalOrderKey& key) const
    {
        if (user_id != key.user_id)
            return user_id < key.user_id;
        return order_id < key.order_id;
    }
};

struct RemoteOrderKey
{
    std::string exchange_id;
    std::string instrument_id;
    int session_id;
    int front_id;
    std::string order_ref;
    std::string order_sys_id;
    bool operator < (const RemoteOrderKey& key) const
    {
        if (session_id != key.session_id)
            return session_id < key.session_id;
        if (front_id != key.front_id)
            return front_id < key.front_id;
        return order_ref < key.order_ref;
    }
};

struct OrderKeyPair
{
    LocalOrderKey local_key;
    RemoteOrderKey remote_key;
};

struct OrderKeyFile
{
    std::string trading_day;
    std::vector<OrderKeyPair> items;
};

struct CtpActionInsertOrder {
    CtpActionInsertOrder() {
        memset(&f, 0, sizeof(f));
    }
    LocalOrderKey local_key;
    CThostFtdcInputOrderField f;
};

struct CtpActionCancelOrder
{
    CtpActionCancelOrder() {
        memset(&f, 0, sizeof(f));
    }
    LocalOrderKey local_key;
    CThostFtdcInputOrderActionField f;
};

class SerializerCtp
    : public RapidSerialize::Serializer<SerializerCtp>
{
public:
    using RapidSerialize::Serializer<SerializerCtp>::Serializer;

    void DefineStruct(OrderKeyFile& d);
    void DefineStruct(OrderKeyPair& d);
    void DefineStruct(LocalOrderKey& d);
    void DefineStruct(RemoteOrderKey& d);
    void DefineStruct(CtpActionInsertOrder& d);
    void DefineStruct(CtpActionCancelOrder& d);
    void DefineStruct(CThostFtdcUserPasswordUpdateField& d);
    void DefineStruct(CThostFtdcReqTransferField& d);
    void DefineStruct(CThostFtdcTransferSerialField& d);
};
}
