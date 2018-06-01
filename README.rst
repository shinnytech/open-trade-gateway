Introduction
=================================================
Open Trade Gateway 是一套主要用于期货交易的中继服务器系统. 它可以接受客户端以 `DIFF协议 (Differential Information Flow for Finance) <https://github.com/shinnytech/diff>`_  接入, 完成用户终端与常见期货柜台系统的数据交互.

本项目目前支持的期货交易柜台系统包括:

* CTP
* Femas 主席系统 (测试中)
* 恒生 UFX 系统 (测试中)

可以通过 `DIFF协议 <https://github.com/shinnytech/diff>`_ 接入本系统的终端产品包括:

* `Shinny Future Android <https://github.com/shinnytech/shinny-futures-android>`_ : 一个开源的 android 平台期货行情交易终端
* `天勤衍生品研究终端 <http://www.tq18.cn>`_ : 一套免费的PC行情交易终端, 支持以 DIFF 协议进行扩展开发.
* `Tianqin Python Sdk <https://github.com/tianqin18/tqsdk-python>`_ : 一套开源的 python 框架, 


Install
-------------------------------------------------
安装前准备:

* 编译器：Microsoft Visual Studio 2015 或 2017
* 运行环境：Microsoft Windows Server 2008/2012 或 Windows 7/8/10

从github下载 本项目代码::

    git clone https://github.com/shinnytech/open-trade-gateway.git

编译::

    用 Microsoft Visual Studio 打开 open_trade_gateway.sln, 编译 release 版本

安装::

    将 bin/release 目录下所有内容复制到准备运行的服务器上


Config
-------------------------------------------------
本系统运行需要两个配置文件:

* config.json: 服务进程配置
* brokers.json: 对接的期货公司交易系统前置机配置

.. code-block:: javascript
   :caption: config.json
   
{
  "host": "127.0.0.1",
  "port": 7777
}

brokers.json 中可以设置一组或多组期货公司前置机

.. code-block:: javascript
   :caption: brokers.json
   
{
  "simnow": {
    "type": "ctp",
    "broker_id": "9999",
    "product_info": "abcd",
    "trading_fronts": [
      "tcp://218.202.237.33:10002"
    ]
  }
}

Run
-------------------------------------------------
在命令行下运行服务器主程序::

  open_trade_gateway.exe
  
