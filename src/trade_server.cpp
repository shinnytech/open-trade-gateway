/////////////////////////////////////////////////////////////////////////
///@file trade_server.cpp
///@brief	交易网关服务器
///@copyright	上海信易信息科技股份有限公司 版权所有
/////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "trade_server.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "config.h"
#include "rapid_serialize.h"
#include "md_service.h"
#include "trader_base.h"
#include "ctp/trader_ctp.h"
#include "sim/trader_sim.h"


namespace trade_server
{
class TradeSession
    : public std::enable_shared_from_this<TradeSession>
{
  public:
    // Take ownership of the socket
    explicit TradeSession(boost::asio::ip::tcp::socket socket)
        : m_ws_socket(std::move(socket)), strand_(m_ws_socket.get_executor())
    {
        m_trader_instance = NULL;
    }
    void Run();
    // Start the asynchronous operation
    void OnOpenConnection(boost::system::error_code ec);
    void DoRead();
    void OnRead(boost::system::error_code ec, std::size_t bytes_transferred);
    void OnMessage(const std::string &json_str);
    void SendTextMsg(const std::string &msg);
    void DoWrite();
    void OnWrite(boost::system::error_code ec, std::size_t bytes_transferred);
    void OnCloseConnection();

private:
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_ws_socket;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::beast::multi_buffer m_input_buffer;
    boost::beast::multi_buffer m_output_buffer;
    trader_dll::TraderBase* m_trader_instance;
};



// Start the asynchronous operation
void TradeSession::Run()
{
    // Accept the websocket handshake
    m_ws_socket.async_accept(
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &TradeSession::OnOpenConnection,
                shared_from_this(),
                std::placeholders::_1)));
}
// ws.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send);
// ws.close(close_code::normal);
// ws.auto_fragment(true);
// ws.write_buffer_size(16384);

void TradeSession::OnOpenConnection(boost::system::error_code ec)
{
    if (ec)
    {
        Log(LOG_WARNING, NULL, "trade session accept fail, session=%p", this);
        return;
    }
    SendTextMsg(g_config.broker_list_str);
    Log(LOG_INFO, NULL, "trade server got connection, session=%p", this);
    DoRead();
}

void TradeSession::DoRead()
{
    m_ws_socket.async_read(
        m_input_buffer,
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &TradeSession::OnRead,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2)));
}

void TradeSession::OnRead(boost::system::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);
    if (ec)
    {
        if (ec != boost::beast::websocket::error::closed)
        {
            Log(LOG_WARNING, NULL, "trade session read fail, session=%p", this);
        }
        OnCloseConnection();
        return;
    }
    OnMessage(boost::beast::buffers_to_string(m_input_buffer.data()));
    m_input_buffer.consume(m_input_buffer.size());
    // Do another read
    DoRead();
}

void TradeSession::OnMessage(const std::string &json_str)
{
    trader_dll::SerializerTradeBase ss;
    if (!ss.FromString(json_str.c_str()))
    {
        Log(LOG_WARNING, NULL, "trade session parse json(%s) fail, session=%p", json_str.c_str(), this);
        return;
    }
    Log(LOG_INFO, json_str.c_str(), "trade session received package, session=%p", this);
    trader_dll::ReqLogin req;
    ss.ToVar(req);
    if (req.aid == "req_login")
    {
        if (m_trader_instance)
        {
            m_trader_instance->Stop();
            m_trader_instance = NULL;
        }
        auto broker = g_config.brokers.find(req.bid);
        if (broker == g_config.brokers.end())
        {
            Log(LOG_WARNING, json_str.c_str(), "trade server req_login invalid bid, session=%p, bid=%s", this, req.bid.c_str());
            return;
        }
        req.broker = broker->second;
        // req.client_addr = con->get_request_header("X-Real-IP");
        // if (req.client_addr.empty())
        //     req.client_addr = con->get_remote_endpoint();
        if (broker->second.broker_type == "ctp")
        {
            m_trader_instance = new trader_dll::TraderCtp(std::bind(&TradeSession::SendTextMsg, shared_from_this(), std::placeholders::_1));
        }
        else if (broker->second.broker_type == "sim")
        {
            m_trader_instance = new trader_dll::TraderSim(std::bind(&TradeSession::SendTextMsg, shared_from_this(), std::placeholders::_1));
        }
        else
        {
            Log(LOG_ERROR, json_str.c_str(), "trade server req_login invalid broker_type=%s, session=%p", broker->second.broker_type.c_str(), this);
            return;
        }
        m_trader_instance->Start(req);
        Log(LOG_INFO, NULL, "create-trader-instance, session=%p", this);
        return;
    }
    if (m_trader_instance)
        m_trader_instance->m_in_queue.push_back(json_str);
}

void TradeSession::SendTextMsg(const std::string &msg)
{
    if(m_output_buffer.size() > 0){
        size_t n = boost::asio::buffer_copy(m_output_buffer.prepare(msg.size()), boost::asio::buffer(msg));
        m_output_buffer.commit(n);
    } else {
        size_t n = boost::asio::buffer_copy(m_output_buffer.prepare(msg.size()), boost::asio::buffer(msg));
        m_output_buffer.commit(n);
        DoWrite();
    }
}

void TradeSession::DoWrite()
{
    m_ws_socket.text(true);
    m_ws_socket.async_write(
        m_output_buffer.data(),
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &TradeSession::OnWrite,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2)));
}

void TradeSession::OnWrite(
    boost::system::error_code ec,
    std::size_t bytes_transferred)
{
    if (ec)
        Log(LOG_WARNING, NULL, "trade server send message fail");
    else
        Log(LOG_INFO, NULL, "trade server send message success, session=%p, len=%d", this, bytes_transferred);
    m_output_buffer.consume(bytes_transferred);
    if(m_output_buffer.size() > 0)
        DoWrite();
}

void TradeSession::OnCloseConnection()
{
    Log(LOG_INFO, NULL, "trade session loss connection, session=%p", this);
    if(m_trader_instance){
        m_trader_instance->Stop();
        for(int i=0;i<10;i++){
            if(m_trader_instance->m_finished)
                break;
            sleep(1000);
        }
    }
    m_trader_instance = NULL;
    exit(0);
}

struct TradeServerContext
{
    boost::asio::io_context* m_ioc;
    boost::asio::ip::tcp::acceptor* m_acceptor;
    boost::asio::signal_set* m_signal;
} trade_server_context;

void WaitForSignal()
{
    trade_server_context.m_signal->async_wait(
        [](boost::system::error_code /*ec*/, int /*signo*/) {
            // Only the parent process should check for this signal. We can
            // determine whether we are in the parent by checking if the acceptor
            // is still open.
            if (trade_server_context.m_acceptor->is_open())
            {
                // Reap completed child processes so that we don't end up with
                // zombies.
                int status = 0;
                while (waitpid(-1, &status, WNOHANG) > 0)
                {
                }

                WaitForSignal();
            }
        });
}

void DoAccept();

void OnAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
{
    if (ec)
    {
        Log(LOG_WARNING, NULL, "trade server accept error, ec=%s", ec.message());
        DoAccept();
        return;
    }

    // Inform the io_context that we are about to fork. The io_context
    // cleans up any internal resources, such as threads, that may
    // interfere with forking.
    trade_server_context.m_ioc->notify_fork(boost::asio::io_context::fork_prepare);

    if (fork() == 0)
    {
        // In child process

        // Inform the io_context that the fork is finished and that this
        // is the child process. The io_context uses this opportunity to
        // create any internal file descriptors that must be private to
        // the new process.
        trade_server_context.m_ioc->notify_fork(boost::asio::io_context::fork_child);

        // The child won't be accepting new connections, so we can close
        // the acceptor. It remains open in the parent.
        trade_server_context.m_acceptor->close();

        // The child process is not interested in processing the SIGCHLD
        // signal.
        trade_server_context.m_signal->cancel();

        std::make_shared<TradeSession>(std::move(socket))->Run();
    }
    else
    {
        // In parent process
        // Inform the io_context that the fork is finished (or failed)
        // and that this is the parent process. The io_context uses this
        // opportunity to recreate any internal resources that were
        // cleaned up during preparation for the fork.
        trade_server_context.m_ioc->notify_fork(boost::asio::io_context::fork_parent);

        // The parent process can now close the newly accepted socket. It
        // remains open in the child.
        socket.close();

        DoAccept();
    }
}

void DoAccept()
{
    if (!trade_server_context.m_acceptor->is_open())
        return;
    trade_server_context.m_acceptor->async_accept(
        std::bind(
            OnAccept,
            std::placeholders::_1,
            std::placeholders::_2));
}

void Init(boost::asio::io_context &ioc, boost::asio::ip::tcp::endpoint endpoint)
{
    trade_server_context.m_ioc = &ioc;
    trade_server_context.m_signal = new boost::asio::signal_set(ioc, SIGCHLD);
    trade_server_context.m_acceptor = new boost::asio::ip::tcp::acceptor(ioc);

    WaitForSignal();
    boost::system::error_code ec;

    // Open the acceptor
    trade_server_context.m_acceptor->open(endpoint.protocol(), ec);
    if (ec)
    {
        Log(LOG_ERROR, NULL, "trade server acceptor open fail, ec=%s", ec.message().c_str());
        return;
    }

    // Allow address reuse
    trade_server_context.m_acceptor->set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
        Log(LOG_ERROR, NULL, "trade server acceptor set option fail, ec=%s", ec.message().c_str());
        return;
    }

    // Bind to the server address
    trade_server_context.m_acceptor->bind(endpoint, ec);
    if (ec)
    {
        Log(LOG_ERROR, NULL, "trade server acceptor bind fail, ec=%s", ec.message().c_str());
        return;
    }

    // Start listening for connections
    trade_server_context.m_acceptor->listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        Log(LOG_ERROR, NULL, "trade server acceptor listen fail, ec=%s", ec.message().c_str());
        return;
    }

    DoAccept();
}

void Stop()
{
    trade_server_context.m_acceptor->close();
}

} // namespace trade_server