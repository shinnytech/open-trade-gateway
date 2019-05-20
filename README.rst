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

2019-03-21前的版本需要安装 boost 1.68.0, 参见 https://www.boost.org/doc/libs/1_68_0/more/getting_started/unix-variants.html

2019-03-21后的版本需要安装 boost 1.70.0, 参见 https://www.boost.org/users/history/version_1_70_0.html

安装 open-trade-gateway
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
获取 open-trade-gateway 代码::

    git clone https://github.com/shinnytech/open-trade-gateway.git

编译与安装::
  cd open-trade-gateway
  sudo make
  sudo make install

第一次安装后需要将如下两个路径加入/etc/ld.so.conf文件中::
	/usr/local/bin/
	/usr/local/lib/
	然后执行命令:ldconfig
  
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
        "name": "simnow",//一个系统中要保证唯一性
        "type": "ctp",//交易系统类型,目前支持ctp(ctp非穿管版)、ctpse13(ctp穿管版6.3.13版)、ctpse(ctp穿管版6.3.15)、sim(快期模拟)四种
        "is_fens":false,//前置地址是否是Fens地址,只对type=ctp,ctpse或者ctpse13时有效,type=sim时忽略
        "broker_id": "9999",//broker_id,必须与交易系统中的设置一致
        "product_info": "abcd",//如果type=ctp,这里填写从期货公司申请的产品UserProductInfo;如果type=ctpse、或者ctpse13,这里填写从期货公司审请的中继产品RelayAppID;type=sim时忽略
        "auth_code":"VUZMGH==",//如果type=ctp,这里填写从期货公司申请的产品AuthCode(由对应的UserProductInfo生成);如果type=ctpse、或者ctpse13,这里填写从期货公司申请的中继产品AuthCode(由对应的RelayAppId生成);type=sim时忽略
        "trading_fronts": [//如果is_fens=false，这里填写ctp的交易前置机地址,如果is_fens=true,则这里填写ctp的命名服务地址,type=sim时忽略
        "tcp://218.202.237.33:10002"
        ]
      }
    ]

/etc/open-trade-gateway/broker_list.json中的一组配置也可以用/etc/open-trade-gateway/broker_list/目录下的一个文件来代替::
   
   如可以用文件/etc/open-trade-gateway/broker_list/simnow.json代替上面的配置,如下:
   
	{
     "name": "simnow",
     "type": "ctp",
	 "is_fens":false,
     "broker_id": "9999",
     "product_info": "abcd",
	 "auth_code":"VUZMGH==",
     "trading_fronts": [ "tcp://218.202.237.33:10002" ]
    }


Run
-------------------------------------------------
在命令行下运行服务器主程序::

  open_trade_gateway

系统运行日志将输出到文件 /var/log/open-trade-gateway/open-trade-gateway.log 中


Test
-------------------------------------------------
主程序启动后，用任意websocket client 连接到服务端口，应该收到这样的信息::

    {
      "aid": "rtn_brokers",
      "brokers": ["simnow"]
    }

表示服务器主程序启动正常


Q&A
-------------------------------------------------
1、执行open_trade_gateway后，未启动重新返回命令行

解决：基本出现在编译完成后的首次运行，请检查是否对broker_list.json 、config.json重命名并配置。出现该问题时，一般/var/log/open-trade-gateway//open-trade-gateway.log中的提示信息是找不到config.json文件