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

第一次安装后需要将如下两个路径加入/etc/ld.so.conf文件中

* /usr/local/bin/
* /usr/local/lib/

然后执行命令:ldconfig
  
Config
-------------------------------------------------
本系统运行需要两个配置文件:

/etc/open-trade-gateway/config.json,用于服务进程的一些配置项::

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

/etc/open-trade-gateway/broker_list.json中的一组配置也可以用/etc/open-trade-gateway/broker_list/目录下的一个文件来代替，如可以用simnow.json文件代替上面的配置::   

      {
        "name": "simnow",
        "type": "ctp"，
        "is_fens":false,
        "broker_id": "9999",
        "product_info": "abcd",
        "auth_code":"VUZMGH==",
        "trading_fronts": [
        "tcp://218.202.237.33:10002"
        ]
      }

Run
-------------------------------------------------
在命令行下运行服务器主程序::

  open_trade_gateway

系统运行日志将输出到文件 /var/log/open-trade-gateway/open-trade-gateway.log 中,如果目录 /var/log/open-trade-gateway/ 不存在,请手工创建.


Test
-------------------------------------------------
主程序启动后，用任意websocket client 连接到服务端口，应该收到这样的信息::

    {
      "aid": "rtn_brokers",
      "brokers": ["simnow"]
    }

表示服务器主程序启动正常

负载均衡服务配置
-------------------------------------------------

1、首先按上述配置步骤在一台或者多台服务器上配置一个或者多个open_trade_gateway实例; 

2、按下面的配置文件(文件名config-ms.json,需要安装在/etc/open-trade-gateway/下)的说明配置负载均衡服务器;
::

	{
		"host":"0.0.0.0",//提供负载均衡服务的IP地址
		"port":5566,//负载均衡服务的端口号
		"slaveNodeList":[//在第1步中已经配好的open_trade_gateway实例列表    
		{
			"name":"135",//结点名称,不能重复
			"host":"192.168.1.35",//open_trade_gateway实例的IP地址
			"port":"7788", //open_trade_gateway实例的端口号(注意:这里是字符串)
			"path":"/" //open_trade_gateway实例的路径,默认为"/"
		},
		{
			"name":"136",
			"host":"192.168.1.36",
			"port":"7788",
			"path":"/"
		},
		{
			"name":"137",
			"host":"192.168.1.37",
			"port":"7788",
			"path":"/",
		}
		]
	}

3、上述多个open_trade_gateway实例的broker list的bid配置不可重复,如果重复,按步骤2中结点配置的顺序,先出现的有效,后出现的忽略;

4、首先正确启动上述结点上的open_trade_gateway实例,最后启动负载均衡服务器open-trade-gateway-ms;

5、采用DIFF协议的客户端应用连接open-trade-gateway-ms的服务端口(上例中的5566)发送请求,open-trade-gateway-ms会根据请求的bid自动将请求转发到不同的open-trade-gateway结点进行处理,实现负载均衡;

条件单服务配置
-------------------------------------------------

1、目前,条件单服务只是一个逻辑上的服务,因此正常编译安装了open-trade-gateway之后就同时安装了条件单服务;

2、按下面的配置文件(文件名config-condition-order.json,需要安装在/etc/open-trade-gateway/下)的说明配置条件单服务;
::

 {
  "run_server":true,
  "max_new_cos_per_day":20,
  "max_valid_cos_all":50,
  "auto_start_ctp_time": [{"weekday":1,"timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]},
  {"weekday":2,"timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]},
  {"weekday":3,"timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]},
  {"weekday":4,"timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]},
   {"weekday":5,"timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]}
  ],
  "auto_close_ctp_time": [{"weekday":1,"timespan":[{"begin":1535,"end":1540}]},
  {"weekday":2,"timespan":[{"begin":235,"end":240},{"begin":1535,"end":1540}]},
  {"weekday":3,"timespan":[{"begin":235,"end":240},{"begin":1535,"end":1540}]},
  {"weekday":4,"timespan":[{"begin":235,"end":240},{"begin":1535,"end":1540}]},
  {"weekday":5,"timespan":[{"begin":235,"end":240},{"begin":1535,"end":1540}]},
  {"weekday":6,"timespan":[{"begin":235,"end":240}]}
  ],
  "auto_restart_process_time":  [{"weekday":1,"timespan":[{"begin":900,"end":1530}]},
  {"weekday":2,"timespan":[{"begin":0,"end":230},{"begin":900,"end":1530}]},
  {"weekday":3,"timespan":[{"begin":0,"end":230},{"begin":900,"end":1530}]},
  {"weekday":4,"timespan":[{"begin":0,"end":230},{"begin":900,"end":1530}]},
  {"weekday":5,"timespan":[{"begin":0,"end":230},{"begin":900,"end":1530}]},
  {"weekday":6,"timespan":[{"begin":0,"end":230}]}
  ]
 }
  
* "run_server"表示是否启用条件单服务,true表示启用,false表示不启用;

* "max_new_cos_per_day"表示单个用户一个交易日能够添加的最大条件单数量限制,默认为20条;

* "max_valid_cos_all"表示单个用户最多可同时持有的最大未触发条件单数量限制,包括非本交易日添加的,默认为50条;

* "auto_start_ctp_time"表示自动重登录用户的时间段配置,在配置的时间段内,如果发现用户还没有登录交易系统,且用户有条件单数据,条件单服务会自动登录交易系统,以保证条件单能够正常被触发;

* "auto_close_ctp_time": 表示自动关闭CTP实例的时间段配置,在配置的时间段内,系统会自动关闭CTP实例,以防止CTP在非交易时间段内发生崩溃,关闭CTP实例后用户仍然能够登录交易系统并查询用户截面数据,但不能下单;

* "auto_restart_process_time":表示自动重启交易实例进程的时间段配置,在配置的时间段内,如果用户的交易实例进程崩溃,open-trade-gateway会自动重启该进程;如果open-trade-gateway进程在该配置项的时间段内重新启动,也会自动启动有条件单的用户进程;

* 上述的三个时间段配置全部采用{"weekday":1,"timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]的形式;

* "weekday":XX定义一周的某一天,0表示周日,1表示周一,依次类推;

* "timespan":[{"begin":835,"end":840},{"begin":2040,"end":2045}]表示一个时间区间的列表,列表项表示一天中的某个时间段,如{"begin":835,"end":840}表示早上8:30到8:40之间;

3、条件单服务配置文件修改后需要重启交易系统,open-trade-gateway只会在启动时加载config-condition-order.json配置文件;

Q&A
-------------------------------------------------
1、执行open_trade_gateway后，未启动重新返回命令行

解决：基本出现在编译完成后的首次运行，请检查是否对broker_list.json 、config.json重命名并配置。出现该问题时，一般/var/log/open-trade-gateway//open-trade-gateway.log中的提示信息是找不到config.json文件