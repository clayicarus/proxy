//
// Created by clay on 22-10-13.
//

#ifndef PROXY_TUNNEL_H
#define PROXY_TUNNEL_H

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

class Tunnel : public std::enable_shared_from_this<Tunnel>, muduo::noncopyable {
static constexpr size_t kHighMark = 1024 * 1024;
public:
    Tunnel(muduo::net::EventLoop *loop,
           const muduo::net::InetAddress &serverAddr,
           const muduo::net::TcpConnectionPtr serverConn)
    : client_(loop, serverAddr, serverConn->name())
    {
        LOG_INFO << "Tunnel " << serverConn->peerAddress().toIpPort()
                 << " <-> " << serverAddr.toIpPort();
    }
    ~Tunnel()
    {
        LOG_INFO << "~Tunnel";
    }

    void setup()
    {
        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;

        client_.setConnectionCallback(std::bind(&Tunnel::onClientConnection/* source connection */,
                                                shared_from_this(), _1));
        client_.setMessageCallback(std::bind(&Tunnel::onClientMessage,
                                             shared_from_this(), _1, _2, _3));
        serverConn_->setHighWaterMarkCallback(std::bind(&Tunnel::onHighWaterMarkWeak,
                                                        std::weak_ptr<Tunnel>(shared_from_this()), kServer, _1, _2),
                                              kHighMark);
    }

    void connect()
    {
        client_.connect();
    }

    void disconnect()
    {
        client_.disconnect();
    }

private:
    void teardown() // Q3: disconnect destination conn actively
    {
        client_.setConnectionCallback(muduo::net::defaultConnectionCallback);
        client_.setMessageCallback(muduo::net::defaultMessageCallback);
        if(serverConn_) {
            serverConn_->setContext(boost::any());  // Q2 ?
            serverConn_->shutdown();    // how about close destination directly ?
        }
        clientConn_.reset();    // free shared_ptr (~clientConn_)
    }

    void onClientConnection(const muduo::net::TcpConnectionPtr &conn)   // source connection
    {
        using std::placeholders::_1;
        using std::placeholders::_2;

        LOG_DEBUG << (conn->connected() ? "UP" : "DOWN");
        if(conn->connected()) { // source connected
            conn->setTcpNoDelay(true);
            conn->setHighWaterMarkCallback(std::bind(&Tunnel::onHighWaterMarkWeak,
                                                     std::weak_ptr<Tunnel>(shared_from_this()), kClient, _1, _2),
                                           kHighMark);
            serverConn_->setContext(conn);  // Q2: record conn to match its client_ ?
            serverConn_->startRead();       // Q1: when source connected then start read destination requests
            clientConn_ = conn;
            if(serverConn_->inputBuffer()->readableBytes() > 0) {   // Q1
                conn->send(serverConn_->inputBuffer()); // send requests from destination to source
            }
        } else {    // Q3: source disconnected actively
            teardown(); // disconnect destination conn actively
        }
    }

    void onClientMessage(const muduo::net::TcpConnectionPtr &conn,
                         muduo::net::Buffer *buf,
                         muduo::Timestamp)  // received from source
    {
        LOG_DEBUG << conn->name() << " " << buf->readableBytes();
        if(serverConn_) {
            serverConn_->send(buf); // send response from source to destination
        } else {    // destination died
            buf->retrieveAll(); // discard all received data
            abort();
        }
    }

    enum ServerClient {
        kServer, kClient
    };

    void onHighWaterMark(ServerClient which,
                         const muduo::net::TcpConnectionPtr &conn,
                         size_t bytesToSent)
    {
        using std::placeholders::_1;

        LOG_INFO << (which == kServer ? "server" : "client")
                 << " onHighWaterMark " << conn->name()
                 << " bytes " << bytesToSent;
        if(which == kServer) {  // destination output buffer full
            if(serverConn_->outputBuffer()->readableBytes() > 0) {  // sent not yet
                clientConn_->stopRead();    // stop reading response from source
                serverConn_->setWriteCompleteCallback(std::bind(&Tunnel::onWriteCompleteWeak,
                                                                std::weak_ptr<Tunnel>(shared_from_this()),
                                                                        kServer, _1));  // continue to send to destination when write completely
            }
            // sent yet
        } else {    // source output buffer full
            if(clientConn_->outputBuffer()->readableBytes() > 0) {
                serverConn_->stopRead();
                clientConn_->setWriteCompleteCallback(std::bind(&Tunnel::onWriteCompleteWeak,
                                                                std::weak_ptr<Tunnel>(shared_from_this()), kClient, _1));
            }
        }
    }
    static void onHighWaterMarkWeak(const std::weak_ptr<Tunnel> &wkTunnel,
                                    ServerClient which,
                                    const muduo::net::TcpConnectionPtr &conn,
                                    size_t bytesToSent) // weak callback for what ?
    {
        std::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
        if(tunnel) {
            tunnel->onHighWaterMark(which, conn, bytesToSent);
        }
    }

    void onWriteComplete(ServerClient which, const muduo::net::TcpConnectionPtr &conn)  // continue to send
    {
        LOG_INFO << (which == kServer ? "server" : "client")
                 << " onWriteComplete " << conn->name();
        if(which == kServer) {  // sent to source(server) yet, destination output buffer not full
            clientConn_->startRead();   // start to read from source
            serverConn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback()); // default callback
        } else {
            serverConn_->startRead();
            clientConn_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
        }
    }
    static void onWriteCompleteWeak(const std::weak_ptr<Tunnel> &wkTunnel,
                                    ServerClient which,
                                    const muduo::net::TcpConnectionPtr &conn)   // weak callback for what ?
    {
        std::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
        if(tunnel) {
            tunnel->onWriteComplete(which, conn);
        }
    }

    muduo::net::TcpClient client_;
    muduo::net::TcpConnectionPtr  serverConn_;
    muduo::net::TcpConnectionPtr clientConn_;
};
typedef std::shared_ptr<Tunnel> TunnelPtr;

#endif //PROXY_TUNNEL_H
