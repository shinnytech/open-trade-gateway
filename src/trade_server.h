/////////////////////////////////////////////////////////////////////////
///@file trade_server.h
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace trade_server
{
    void Init(boost::asio::io_context &ioc, boost::asio::ip::tcp::endpoint endpoint);
    void Stop();
}