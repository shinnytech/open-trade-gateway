/////////////////////////////////////////////////////////////////////////
///@file md_connection.cpp
///@brief	行情连接管理
///@copyright	上海信易信息科技股份有限公司 版权所有 
/////////////////////////////////////////////////////////////////////////

#include "md_connection.h"
#include "log.h"
#include "version.h"
#include "rapid_serialize.h"
#include "md_service.h"

const char* md_host = "openmd.shinnytech.com";
const char* md_port = "80";
const char* md_path = "/t/md/front/mobile";

class MdParser
	: public RapidSerialize::Serializer<MdParser>
{
public:
	using RapidSerialize::Serializer<MdParser>::Serializer;

	void DefineStruct(Instrument& data)
	{
		AddItem(data.last_price,("last_price"));
		AddItem(data.pre_settlement,("pre_settlement"));
		AddItem(data.upper_limit,("upper_limit"));
		AddItem(data.lower_limit,("lower_limit"));
		AddItem(data.ask_price1,("ask_price1"));
		AddItem(data.bid_price1,("bid_price1"));		
		AddItem(data.settlement,("settlement"));
		AddItem(data.pre_close,("pre_close"));		
		AddItem(data.volume,("volume"));
	}
};

md_connection::md_connection(boost::asio::io_context& ios
	,const std::string& req_subscribe_quote
	,const std::string& req_peek_message
	,InsMapType* ins_map
	,mdservice& mds)
	:io_context_(ios)
	, m_req_subscribe_quote(req_subscribe_quote)
	, m_req_peek_message(req_peek_message)
	, m_ins_map(ins_map)
	, m_mds(mds)
	, m_resolver(io_context_)
	, m_ws_socket(io_context_)
	, m_input_buffer()
	, m_output_buffer()	
	, m_connect_to_server(false)
{	
}

md_connection::~md_connection()
{
	Log().WithField("fun","~md_connection()")
		.WithField("key","mdservice")		
		.Log(LOG_INFO,"md_connection is deleted");
	
	if (m_ws_socket.next_layer().is_open())
	{
		Log().WithField("fun","~md_connection()")
			.WithField("key","mdservice")
			.Log(LOG_INFO,"m_ws_socket next_layer is still open()");	
	}
	else
	{
		Log().WithField("fun","~md_connection()")
			.WithField("key","mdservice")
			.Log(LOG_INFO,"m_ws_socket next_layer is closed");
	}
}

void md_connection::Start()
{
	DoResolve();
}

void md_connection::Stop()
{	
	boost::system::error_code ec;
	m_ws_socket.next_layer().close(ec);
	if (ec)
	{
		Log().WithField("fun","Stop")
			.WithField("key","mdservice")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"m_ws_socket close exception");
	}
}

void md_connection::DoResolve()
{
	m_resolver.async_resolve(md_host,md_port,
		std::bind(&md_connection::OnResolve,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2));
}

void md_connection::OnResolve(boost::system::error_code ec
	, boost::asio::ip::tcp::resolver::results_type results)
{
	if (ec)
	{
		Log().WithField("fun","OnResolve")
			.WithField("key","mdservice")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"md_connection resolve fail");		
		//连接错误
		OnConnectionnError();
		return;
	}

	boost::asio::async_connect(m_ws_socket.next_layer(),
		results.begin(),
		results.end(),
		std::bind(&md_connection::OnConnect,
			shared_from_this(),
			std::placeholders::_1));
}

void md_connection::OnConnect(boost::system::error_code ec)
{
	if (ec)
	{
		Log().WithField("fun","OnConnect")
			.WithField("key","mdservice")
			.WithField("errmsg",ec.message())
			.Log(LOG_WARNING,"md_connection connect fail");
		//连接错误
		OnConnectionnError();
		return;
	}
	m_connect_to_server = true;
	
	boost::beast::websocket::permessage_deflate pmd;
	pmd.client_enable = true;
	pmd.server_enable = true;	
	m_ws_socket.set_option(pmd);
	m_ws_socket.auto_fragment(false);
	m_ws_socket.read_message_max(64 * 1024 * 1024);

	//Perform the websocket handshake	
	m_ws_socket.set_option(boost::beast::websocket::stream_base::decorator(
		[](boost::beast::websocket::request_type& m)
	{
		m.insert(boost::beast::http::field::accept,"application/v1+json");
		m.insert(boost::beast::http::field::user_agent,"OTG-" VERSION_STR);
	}));
	m_ws_socket.async_handshake(md_host,md_path,
		boost::beast::bind_front_handler(
			&md_connection::OnHandshake
			,shared_from_this()));	
}

void md_connection::OnHandshake(boost::system::error_code ec)
{
	if (ec)
	{
		Log().WithField("fun","OnHandshake")
			.WithField("key","mdservice")
			.WithField("errmsg",ec.message())
			.Log(LOG_ERROR,"md_connection handshake fail");		
		//连接错误
		OnConnectionnClose();
		return;
	}
		
	Log().WithField("fun", "OnHandshake")
		.WithField("key", "mdservice")	
		.WithField("msglen", (int)m_req_subscribe_quote.length())
		.WithPack("diff_req_pack",m_req_subscribe_quote)
		.Log(LOG_INFO, "send req subscribe quote message");

	SendTextMsg(m_req_subscribe_quote);
		
	SendTextMsg(m_req_peek_message);

	DoRead();
}

void md_connection::SendTextMsg(const std::string &msg)
{
	try
	{
		if (m_output_buffer.size() > 0)
		{
			m_output_buffer.push_back(msg);
		}
		else
		{
			m_output_buffer.push_back(msg);
			DoWrite();
		}
	}
	catch (std::exception& ex)
	{
		Log().WithField("fun","SendTextMsg")
			.WithField("key","mdservice")
			.WithField("errmsg",ex.what())
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_ERROR,"SendTextMsg exception");
	}
}

void md_connection::DoRead()
{
	m_ws_socket.async_read(m_input_buffer,
		boost::beast::bind_front_handler(
			&md_connection::OnRead,
			shared_from_this()));
}

void md_connection::OnRead(boost::system::error_code ec
	, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	if (ec)
	{
		Log().WithField("fun","OnRead")
			.WithField("key","mdservice")
			.WithField("errmsg",ec.message())		
			.Log(LOG_WARNING,"md service read fail");

		//连接关闭
		OnConnectionnClose();
		return;
	}
	OnMessage(boost::beast::buffers_to_string(m_input_buffer.data()));
	m_input_buffer.consume(m_input_buffer.size());
	DoRead();
}

bool ToString(rapidjson::Document& rootDoc, std::string& jsonStr)
{
	rapidjson::StringBuffer buffer(0, 2048);
	typedef rapidjson::EncodedOutputStream<rapidjson::UTF8<>, rapidjson::StringBuffer> OutputStream;
	OutputStream os(buffer, false);
	rapidjson::Writer<OutputStream
		, rapidjson::UTF8<>
		, rapidjson::UTF8<>
		, rapidjson::CrtAllocator
		, rapidjson::kWriteNanAndInfFlag> writer(os);
	rootDoc.Accept(writer);
	jsonStr = std::string(buffer.GetString());
	return true;
}

void  md_connection::OnMessage(const std::string &json_str)
{
	Log().WithField("fun","OnMessage")
		.WithField("key","mdservice")
		.WithField("msglen",(int)json_str.size())		
		.Log(LOG_INFO,"md_connection receive md message");
				
	SendTextMsg(m_req_peek_message);

	MdParser ss;
	ss.FromString(json_str.c_str());
	rapidjson::Value* dt = rapidjson::Pointer("/data/0/quotes").Get(*(ss.m_doc));
	if (!dt)
	{
		return;
	}

	for (auto& m : dt->GetObject())
	{
		std::array<char, 64> key = {};
		strncpy(key.data(),m.name.GetString(),64);
		auto it = m_ins_map->find(key);
		if (it == m_ins_map->end())
			continue;
		ss.ToVar(it->second,&m.value);	

		std::string strKey = "/";
		strKey += m.name.GetString();

		rapidjson::Document instDoc;		
		rapidjson::Value instObj(rapidjson::kObjectType);
		instObj.AddMember("expired",it->second.expired,instDoc.GetAllocator());
		instObj.AddMember("product_class",it->second.product_class, instDoc.GetAllocator());
		instObj.AddMember("volume_multiple", it->second.volume_multiple, instDoc.GetAllocator());
		instObj.AddMember("margin", it->second.margin, instDoc.GetAllocator());
		instObj.AddMember("commission", it->second.commission, instDoc.GetAllocator());
		instObj.AddMember("price_tick", it->second.price_tick, instDoc.GetAllocator());
		instObj.AddMember("last_price", it->second.last_price, instDoc.GetAllocator());
		instObj.AddMember("pre_settlement", it->second.pre_settlement, instDoc.GetAllocator());
		instObj.AddMember("upper_limit", it->second.upper_limit, instDoc.GetAllocator());
		instObj.AddMember("lower_limit", it->second.lower_limit, instDoc.GetAllocator());
		instObj.AddMember("ask_price1", it->second.ask_price1, instDoc.GetAllocator());
		instObj.AddMember("bid_price1", it->second.bid_price1, instDoc.GetAllocator());		
		instObj.AddMember("settlement", it->second.settlement, instDoc.GetAllocator());
		instObj.AddMember("pre_close", it->second.pre_close, instDoc.GetAllocator());
		instObj.AddMember("volume", it->second.volume, instDoc.GetAllocator());
		rapidjson::Pointer(strKey.c_str()).Set(instDoc,instObj);
		std::string strMerge = "";
		ToString(instDoc,strMerge);

		std::string strDiff = "";
		rapidjson::Document jsonDoc;
		rapidjson::Pointer(strKey.c_str()).Set(jsonDoc,m.value);
		ToString(jsonDoc,strDiff);
		Log().WithField("fun","OnMessage")
			.WithField("key","mdservice")
			.WithPack("diff",strDiff)
			.WithPack("merge",strMerge)
			.Log(LOG_INFO, "md_connection receive md message");	
	}
}

void md_connection::DoWrite()
{
	if (m_output_buffer.empty())
	{
		return;
	}
	auto write_buf = boost::asio::buffer(m_output_buffer.front());
	m_ws_socket.text(true);
	m_ws_socket.async_write(write_buf,
		boost::beast::bind_front_handler(
			&md_connection::OnWrite,
			shared_from_this()));
}

void md_connection::OnWrite(boost::system::error_code ec
	, std::size_t bytes_transferred)
{
	if (ec)
	{
		Log().WithField("fun","OnWrite")
			.WithField("key","mdservice")
			.WithField("errmsg",ec.message())
			.WithField("fd",(int)m_ws_socket.next_layer().native_handle())
			.Log(LOG_WARNING, "mdservice OnWrite exception");
		//连接关闭
		OnConnectionnClose();
		return;
	}

	if (m_output_buffer.empty())
	{
		return;
	}

	m_output_buffer.pop_front();
	if (m_output_buffer.size() > 0)
	{
		DoWrite();
	}
}

void md_connection::OnConnectionnClose()
{
	Stop();
	m_mds.OnConnectionnClose();
}

void md_connection::OnConnectionnError()
{
	Stop();
	m_mds.OnConnectionnError();
}