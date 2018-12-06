/////////////////////////////////////////////////////////////////////////
///@file trader_sim.h
///@brief	模拟交易实现
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#pragma once
#include "../trader_base.h"


namespace trader_dll
{

struct ActionOrder {
    ActionOrder()
    {
        price_type = kPriceTypeLimit;
        volume_condition = kOrderVolumeConditionAny;
        time_condition = kOrderTimeConditionGFD;
        hedge_flag = kHedgeFlagSpeculation;
        limit_price = 0;
        volume = 0;
    }
    std::string aid;
    std::string order_id;
    std::string user_id;
    std::string exchange_id;
    std::string ins_id;
    long direction;
    long offset;
    int volume;
    long price_type;
    double limit_price;
    long volume_condition;
    long time_condition;
    long hedge_flag;
};

struct ActionTransfer {
    ActionTransfer()
    {
        currency = "CNY";
        amount = 0.0;
    }
    std::string currency;
    double amount;
};

class SerializerSim
    : public RapidSerialize::Serializer<SerializerSim>
{
public:
    using RapidSerialize::Serializer<SerializerSim>::Serializer;

    void DefineStruct(ActionOrder& d);
    void DefineStruct(ActionTransfer& d);
};

class TraderSim
    : public TraderBase
{
public:
    TraderSim(std::function<void(const std::string&)> callback);
    virtual void ProcessInput(const char* json_str) override;

private:
    //框架函数
    virtual void OnInit() override;
    virtual void OnIdle() override;
    virtual void OnFinish() override;

    //用户请求处理
    void OnClientReqInsertOrder(ActionOrder action_insert_order);
    void OnClientReqCancelOrder(ActionOrder action_cancel_order);
    void OnClientReqTransfer(ActionTransfer action_transfer);

    //数据更新发送
    void OnClientPeekMessage();
    void SendUserData();
    bool m_peeking_message;
    bool m_something_changed;
    long long m_next_send_dt;

    //模拟交易管理
    Account* m_account; //指向 m_data.m_trade["SIM"].m_accounts["CNY"]
    void TryOrderMatch();
    void CheckOrderTrade(Order* order);
    void DoTrade(Order* order, int volume, double price);
    void UpdateOrder(Order* order);
    void UpdatePositionVolume(Position* position);
    std::set<Order*> m_alive_order_set;
    int m_last_seq_no;

    //数据存取档
    void LoadUserDataFile();
    void SaveUserDataFile();
};
}