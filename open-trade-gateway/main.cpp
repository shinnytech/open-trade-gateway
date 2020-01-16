/////////////////////////////////////////////////////////////////////
//@file main.cpp
//@brief	主程序
//@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////

#include "trade_server.h"
#include "config.h"
#include "http.h"
#include "version.h"
#include <iostream>
#include <string>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/stacktrace.hpp>

boost::interprocess::managed_shared_memory* m_segment= nullptr;
ShmemAllocator* m_alloc_inst = nullptr;
InsMapType* m_ins_map = nullptr;

bool LoadInsList();

void FreeInstList();

void dump_execinfo();

bool otg_already_running(const char* procname);

int main(int argc, char* argv[])
{
	try
	{
		if (otg_already_running("open-trade-gateway"))
		{			
			std::cout << VERSION_STR <<std::endl;
			return -1;
		}

		Log().WithField("fun", "main")
			.WithField("key", "gateway")
			.Log(LOG_INFO, "trade server init");
							
		//加载配置文件
		if (!LoadConfig())
		{
			Log().WithField("fun","main")
				.WithField("key","gateway")
				.Log(LOG_WARNING,"trade_server load config failed!");			
			return -1;
		}

		//加载合约列表
		if (!LoadInsList())
		{
			Log().WithField("fun","main")
				.WithField("key","gateway")
				.Log(LOG_WARNING,"trade_server load inst list failed!");
			return -1;
		}

		//启动md server
		boost::process::child md_child(boost::process::search_path("open-trade-mdservice"));

		boost::asio::io_context ios;		
		trade_server s(ios,g_config.port);
		if (!s.init())
		{
			md_child.terminate();
			md_child.wait();
			Log().WithField("fun","main")
				.WithField("key","gateway")
				.Log(LOG_INFO,"trade_server init fail!");			
			return -1;
		}
		
		std::atomic_bool flag;
		flag.store(true);

		boost::asio::signal_set signals_(ios);
		signals_.add(SIGINT);
		signals_.add(SIGTERM);
		#if defined(SIGQUIT)
			signals_.add(SIGQUIT);
		#endif 
		signals_.async_wait(
			[&s,&ios,&md_child,&flag](boost::system::error_code, int sig)
		{			
			Log().WithField("fun", "main")
				.WithField("key", "gateway")
				.WithField("sig", sig)
				.Log(LOG_INFO, "trade_server got sig");

			flag.store(false);
												
			s.stop();			
			
			md_child.terminate();
			md_child.wait();
			
			FreeInstList();
			
			ios.stop();

			Log().WithField("fun", "main")
				.WithField("key", "gateway")
				.Log(LOG_INFO, "trade_server exit");
		});
		
		while (flag.load())
		{
			try
			{
				ios.run();
				break;
			}
			catch (std::bad_alloc& bd)
			{
				dump_execinfo();
				Log().WithField("fun","main")
					.WithField("key","gateway")
					.WithField("errmsg",bd.what())
					.Log(LOG_WARNING, "ios run exception");
				break;
			}
			catch(std::exception& ex)
			{
				dump_execinfo();
				Log().WithField("fun","main")
					.WithField("key","gateway")
					.WithField("errmsg",ex.what())
					.Log(LOG_WARNING,"ios run exception");				
			}
		}

		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "exception:" << e.what() << std::endl;
	}
}

class InsFileParser
	: public RapidSerialize::Serializer<InsFileParser>
{
public:
	void DefineStruct(Instrument& d)
	{
		AddItem(d.expired, ("expired"));
		AddItemEnum(d.product_class, ("class"),
			{
				{ kProductClassFutures, ("FUTURE") },
				{ kProductClassOptions, ("OPTION") },
				{ kProductClassCombination, ("FUTURE_COMBINE") },
				{ kProductClassFOption, ("FUTURE_OPTION") },
				{ kProductClassFutureIndex, ("FUTURE_INDEX") },
				{ kProductClassFutureContinuous, ("FUTURE_CONT") },
				{ kProductClassStock, ("STOCK") },
			});
		AddItem(d.volume_multiple, ("volume_multiple"));
		AddItem(d.price_tick, ("price_tick"));
		AddItem(d.margin, ("margin"));
		AddItem(d.commission, ("commission"));
	}
};

bool GetInstListFromOldService();

bool GetInstListFromNewService();

bool LoadInsList()
{
	try
	{		
		boost::interprocess::shared_memory_object::remove("InsMapSharedMemory");

		m_segment = new boost::interprocess::managed_shared_memory
		(boost::interprocess::create_only
			, "InsMapSharedMemory" //segment name
			, 32 * 1024 * 1024);  //segment size in bytes

		//Initialize the shared memory STL-compatible allocator
		//ShmemAllocator alloc_inst (segment.get_segment_manager());
		m_alloc_inst = new ShmemAllocator(m_segment->get_segment_manager());

		//Construct a shared memory map.
		//Note that the first parameter is the comparison function,
		//and the second one the allocator.
		//This the same signature as std::map's constructor taking an allocator
		m_ins_map =
			m_segment->construct<InsMapType>("InsMap")//object name
			(CharArrayComparer() //first  ctor parameter
				, *m_alloc_inst);     //second ctor parameter		

		Log().WithField("fun", "LoadInsList")
			.WithField("key", "gateway")
			.Log(LOG_INFO, "trade_server construct m_ins_map success");
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun", "LoadInsList")
			.WithField("key", "gateway")
			.WithField("errmsg", ex.what())
			.Log(LOG_FATAL, "trade_server construct m_ins_map fail");
		return false;
	}

	if (g_config.use_new_inst_service)
	{
		return GetInstListFromNewService();
	}
	else
	{
		return GetInstListFromOldService();
	}	
}

void FreeInstList()
{
	try
	{
		if (nullptr != m_segment)
		{
			m_segment->destroy<InsMapType>("InsMap");
			m_ins_map = nullptr;
			if (nullptr != m_alloc_inst)
			{
				delete m_alloc_inst;
				m_alloc_inst = nullptr;
			}
			delete m_segment;
			m_segment = nullptr;
		}
		boost::interprocess::shared_memory_object::remove("InsMapSharedMemory");

		Log().WithField("fun", "FreeInstList")
			.WithField("key", "gateway")		
			.Log(LOG_INFO, "trade_server destroy m_ins_map success");
	}
	catch (std::exception& ex)
	{
		dump_execinfo();
		Log().WithField("fun", "FreeInstList")
			.WithField("key", "gateway")
			.WithField("errmsg", ex.what())
			.Log(LOG_FATAL, "trade_server destroy m_ins_map fail");		
	}
}

bool GetInstListFromOldService()
{
	const char* ins_file_url = "http://openmd.shinnytech.com/t/md/symbols/latest.json";
	try
	{
		//下载和加载合约表文件
		std::string content;
		long ret = HttpGet(ins_file_url, &content);
		if (ret != 0)
		{
			Log().WithField("fun","GetInstListFromOldService")
				.WithField("key","gateway")
				.WithField("ret",(int)ret)
				.Log(LOG_FATAL,"md service download ins file fail");
			return false;
		}

		Log().WithField("fun","GetInstListFromOldService")
			.WithField("key","gateway")
			.WithField("msglen",(int)content.length())
			.Log(LOG_INFO,"mdservice download ins file success");

		InsFileParser ss;
		if (!ss.FromString(content.c_str()))
		{
			Log().WithField("fun","GetInstListFromOldService")
				.WithField("key","gateway")
				.Log(LOG_FATAL,"md service parse downloaded ins file fail");
			return false;
		}

		for (auto& m : ss.m_doc->GetObject())
		{
			std::array<char, 64> key = {};
			strncpy(key.data(), m.name.GetString(), 64);
			ss.ToVar((*m_ins_map)[key], &m.value);
		}
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","GetInstListFromOldService")
			.WithField("key","gateway")
			.WithField("errmsg",ex.what())
			.Log(LOG_FATAL,"get inst list fail");
		return false;
	}

	return true;
}

bool GetInstListFromNewService()
{
	return false;
}

static int lockfile(int fd)
{
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return(fcntl(fd,F_SETLK, &fl));
}

bool otg_already_running(const char* procname)
{
	int  fd;
	char buf[16];
	char filename[100];
	sprintf(filename,"/var/run/%s.pid",procname);

	fd = open(filename, O_RDWR | O_CREAT, (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
	if (fd < 0) 
	{
		std::cout << "open pid file fail!" << std::endl;
		return true;
	}

	if (lockfile(fd) == -1)
	{
		close(fd);
		std::cout << "lock pid file fail!" << std::endl;
		return true;
	}
	else
	{
		ftruncate(fd, 0);
		sprintf(buf,"%ld\n", (long)getpid());
		write(fd,buf,strlen(buf)+1);
		return false;
	}
}

#include <execinfo.h>

void dump_execinfo()
{
	#define BACKTRACE_SIZE   16
	
	void *buffer[BACKTRACE_SIZE];
	char **strings;

	int nptrs = backtrace(buffer,BACKTRACE_SIZE);
	strings = backtrace_symbols(buffer,nptrs);
	if (nullptr ==strings )
	{
		Log().WithField("fun", "dump_execinfo")
			.WithField("key", "gateway")
			.Log(LOG_INFO, "dump_execinfo strings is nullptr");
		return;
	}

	for (int j = 0; j < nptrs; j++)
	{
		Log().WithField("fun","dump_execinfo")
			.WithField("key","gateway")
			.WithField("j",j)
			.WithField("str",strings[j])
			.Log(LOG_INFO, "dump_execinfo strings");
	}
}