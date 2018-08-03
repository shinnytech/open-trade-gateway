Introduction
=================================================
Open Trade Gateway 是一套主要用于期货交易的中继服务器系统. 它可以接受客户端以 `DIFF协议 (Differential Information Flow for Finance) <http://doc.shinnytech.com/diff/index.html>`_  接入, 完成用户终端与期货柜台系统的数据交互.

本项目目前支持的期货交易柜台系统包括:

* CTP
* Femas 主席系统 (测试中)
* 恒生 UFX 系统 (测试中)

`DIFF Collection <http://www.shinnytech.com/diff>`_ 中列出了一些支持本系统的终端产品


Install
-------------------------------------------------
安装前准备:

    Microsoft Windows Server 2008/2012 或 Windows 7/8/10

下载本项目运行包，解压::

    https://github.com/shinnytech/open-trade-gateway/releases

    
Config
-------------------------------------------------
本系统运行需要两个配置文件:

config.json 用于服务进程的一些配置项::

    {
      "host": "127.0.0.1",                          //提供服务的IP地址
      "port": 7777,                                 //提供服务的端口号
      "user_file_path": "c:\\tmp"                   //存放用户文件的目录，必须事先创建好
    }


brokers.json 中可以设置一组或多组期货公司前置机::

    {
      "simnow": {
        "type": "ctp",                              //交易系统类型
        "broker_id": "9999",                        //broker_id, 必须与交易系统中的设置一致
        "product_info": "abcd",
        "trading_fronts": [                         //交易前置机地址
          "tcp://218.202.237.33:10002"
        ]
      }
    }

Run
-------------------------------------------------
在命令行下运行服务器主程序::

  open_trade_gateway.exe


Test
-------------------------------------------------
主程序启动后，用任意websocket client 连接到服务端口，应该收到这样的信息::

    {
      "aid": "rtn_brokers",
      "brokers": ["simnow"]
    }

表示服务器主程序启动正常


Compile
-------------------------------------------------
编译前准备::

    Microsoft Visual Studio 2015 或 2017

从github下载 本项目代码::

    git clone https://github.com/shinnytech/open-trade-gateway.git

编译::

    用 Microsoft Visual Studio 打开 open_trade_gateway.sln, 编译 release 版本

