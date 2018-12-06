Introduction
=================================================
Open Trade Gateway 是一套主要用于期货交易的中继服务器系统. 它可以接受客户端以 `DIFF协议 (Differential Information Flow for Finance) <http://doc.shinnytech.com/diff/latest/index.html>`_  接入, 完成用户终端与期货柜台系统的数据交互.

本项目目前支持的期货交易柜台系统包括:

* CTP
* Femas 主席系统 (测试中)
* 恒生 UFX 系统 (测试中)

`DIFF Collection <http://www.shinnytech.com/diff>`_ 中列出了一些支持本系统的终端产品


Install
-------------------------------------------------
本服务必须在Linux环境下安装运行。下面的安装步骤以 Debian 9 为例，其它 linux 发行版可能需要相应调整.

安装 openssl, libcurl, boost 等依赖库
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

用apt命令安装 openssl 和 libcurl::

    sudo apt install libcurl4-openssl-dev

安装 boost 1.68.0, 参见 https://www.boost.org/doc/libs/1_68_0/more/getting_started/unix-variants.html


安装 open-trade-gateway
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
获取 open-trade-gateway 代码::

    git clone https://github.com/shinnytech/open-trade-gateway.git

编译::

    cd open-trade-gateway
    make
    sudo make install


Config
-------------------------------------------------
本系统运行需要两个配置文件:

/etc/open-trade-gateway/config.json 用于服务进程的一些配置项::

    {
      "host": "0.0.0.0",                                      //提供服务的IP地址  
      "port": 7788,                                           //提供服务的端口号
      "auto_confirm_settlement": false,                       //是否自动确认结算单
      "user_file_path": "/var/local/lib/open-trade-gateway"   //存放用户文件的目录，必须事先创建好
    }


/etc/open-trade-gateway/broker_list.json 中可以设置一组或多组期货公司前置机::

    [
      {
        "name": "simnow",
        "type": "ctp",                              //交易系统类型
        "broker_id": "9999",                        //broker_id, 必须与交易系统中的设置一致
        "product_info": "abcd",
        "trading_fronts": [                         //交易前置机地址
          "tcp://218.202.237.33:10002"
        ]
      }
    ]


Run
-------------------------------------------------
在命令行下运行服务器主程序::

  open_trade_gateway

系统运行日志将输出到 /var/log/open-trade-gateway


Test
-------------------------------------------------
主程序启动后，用任意websocket client 连接到服务端口，应该收到这样的信息::

    {
      "aid": "rtn_brokers",
      "brokers": ["simnow"]
    }

表示服务器主程序启动正常
