cmake_minimum_required(VERSION 3.7)
project(open_trade_gateway_git)

set(CMAKE_CXX_STANDARD 14)

include_directories(contrib/include)
include_directories(src)
include_directories(src/ctp)
include_directories(src/femas_open)
include_directories(src/hs)

link_directories(contrib/lib)

add_executable(open_trade_gateway_git
        contrib/include/ctp/ThostFtdcMdApi.h
        contrib/include/ctp/ThostFtdcTraderApi.h
        contrib/include/ctp/ThostFtdcUserApiDataType.h
        contrib/include/ctp/ThostFtdcUserApiStruct.h
        src/ctp/ctp_define.cpp
        src/ctp/ctp_define.h
        src/ctp/ctp_spi.cpp
        src/ctp/ctp_spi.h
        src/ctp/trader_ctp.cpp
        src/ctp/trader_ctp.h
        src/config.cpp
        src/config.h
        src/datetime.cpp
        src/datetime.h
        src/encoding.cpp
        src/encoding.h
        src/http.cpp
        src/http.h
        src/main.cpp
        src/md_service.cpp
        src/md_service.h
        src/numset.cpp
        src/numset.h
        src/rapid_serialize.h
        src/resource.h
        src/stdafx.cpp
        src/stdafx.h
        src/trade_server.cpp
        src/trade_server.h
        src/trader_base.cpp
        src/trader_base.h
        src/utility.cpp
        src/utility.h
        src/version.h src/iconvpp.h)

target_link_libraries (open_trade_gateway_git
        ssl
        crypto
        pthread
        websockets
        thosttraderapi
        curl)

#install(TARGETS open_trade_gateway_git DESTINATION bin)