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
		AddItem(data.last_price, ("last_price"));
		AddItem(data.pre_settlement, ("pre_settlement"));
		AddItem(data.upper_limit, ("upper_limit"));
		AddItem(data.lower_limit, ("lower_limit"));
		AddItem(data.ask_price1, ("ask_price1"));
		AddItem(data.bid_price1, ("bid_price1"));
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

void md_connection::Start()
{
	DoResolve();
}

void md_connection::Stop()
{
	if (!m_connect_to_server)
	{
		return;
	}
	boost::system::error_code ec;
	m_ws_socket.next_layer().close(ec);
	if (ec)
	{
		LogMs.WithField("fun", "Stop").WithField("errmsg", ec.message().c_str()).WithField("msg", "m_ws_socket close exception").WithField("key", "mdservice").Write(LOG_WARNING);
	}
}

void md_connection::DoResolve()
{
	m_resolver.async_resolve(
		md_host,
		md_port,
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
		Log.WithField("fun", "OnResolve").WithField("errmsg", ec.message().c_str()).WithField("msg", "md_connection resolve fail").WithField("key", "mdservice").Write(LOG_ERROR);
		//连接错误
		OnConnectionnError();
		return;
	}

	boost::asio::async_connect(
		m_ws_socket.next_layer(),
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
		Log.WithField("fun", "OnConnect").WithField("errmsg", ec.message().c_str()).WithField("msg", "md_connection connect fail").WithField("key", "mdservice").Write(LOG_ERROR);
		//连接错误
		OnConnectionnError();
		return;
	}
	m_connect_to_server = true;
	// Perform the websocket handshake
	m_ws_socket.set_option(boost::beast::websocket::stream_base::decorator(
		[](boost::beast::websocket::request_type& m)
	{
		m.insert(boost::beast::http::field::accept, "application/v1+json");
		m.insert(boost::beast::http::field::user_agent, "OTG-" VERSION_STR);
	}));
	m_ws_socket.async_handshake(
		md_host,
		md_path,
		boost::beast::bind_front_handler(
			&md_connection::OnHandshake
			,shared_from_this()));
}

void md_connection::OnHandshake(boost::system::error_code ec)
{
	if (ec)
	{
		Log.WithField("fun", "OnHandshake").WithField("errmsg", ec.message().c_str()).WithField("msg", "md_connection handshake fail").WithField("key", "mdservice").Write(LOG_ERROR);
		//连接错误
		OnConnectionnClose();
		return;
	}

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
		LogMs.WithField("fun", "SendTextMsg").WithField("errmsg", ex.what()).WithField("msg", "SendTextMsg exception").WithField("fd", m_ws_socket.next_layer().native_handle()).WithField("key", "mdservice").Write(LOG_ERROR);
	}
}

void md_connection::DoRead()
{
	m_ws_socket.async_read(
		m_input_buffer,
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
		Log.WithField("fun", "OnRead").WithField("errmsg", ec.message().c_str()).WithField("msg", "md service read fail").WithField("key", "mdservice").Write(LOG_WARNING);

		//连接关闭
		OnConnectionnClose();
		return;
	}
	OnMessage(boost::beast::buffers_to_string(m_input_buffer.data()));
	m_input_buffer.consume(m_input_buffer.size());
	DoRead();
}

void  md_connection::OnMessage(const std::string &json_str)
{
	Log.WithField("fun", "OnMessage").WithField("len", json_str.size()).WithField("msg", "md_connection receive md message").WithField("key", "mdservice").Write(LOG_INFO);

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
		strncpy(key.data(), m.name.GetString(), 64);
		auto it = m_ins_map->find(key);
		if (it == m_ins_map->end())
			continue;
		ss.ToVar(it->second, &m.value);
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
	m_ws_socket.async_write(
		write_buf,
		boost::beast::bind_front_handler(
			&md_connection::OnWrite,
			shared_from_this()));
}

void md_connection::OnWrite(boost::system::error_code ec
	, std::size_t bytes_transferred)
{
	if (ec)
	{
		LogMs.WithField("fun", "OnWrite").WithField("fd", m_ws_socket.next_layer().native_handle()).WithField("msg", "mdservice OnWrite exception").WithField("errmsg", ec.message().c_str()).WithField("key", "mdservice").Write(LOG_WARNING);

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
	m_mds.OnConnectionnError();
}