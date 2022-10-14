//
// Created by clay on 10/14/22.
//

#include "tunnel.h"
#include "Socks.h"

#include <muduo/net/Endian.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>

using namespace muduo;
using namespace muduo::net;

EventLoop *g_eventLoop;
std::map<string, TunnelPtr> g_tunnels;

bool isInWhiteList(const string &ip)
{
    FILE * fp;
    fp = fopen("white_list", "rb");
    if(fp == nullptr) {
        fp = fopen("white_list", "wb");
        fclose(fp);
        return false;
    } else {
        char buf[64 * 1024];
        std::shared_ptr<FILE> f_sp(fp);
        string white_list;
        size_t n;
        while((n = fread(buf, 1, sizeof buf, f_sp.get())) > 0) {
            white_list.append(buf, buf + n);
        }
        auto i1 = white_list.begin();
        auto i2 = std::find(white_list.begin(), white_list.end(), '\n');
        while(i2 != white_list.end()) {
            if(string(i1, i2) == ip)
                return true;
            i1 = ++i2;
            i2 = std::find(i2, white_list.end(), '\n');
        }
        return false;
    }
}

void onServerConnection(const TcpConnectionPtr &conn)
{
    LOG_DEBUG << conn->name() << (conn->connected() ? "UP" : "DOWN");
    if(conn->connected()) {
        if(!isInWhiteList(conn->peerAddress().toIp())) {
            LOG_WARN << "onServerConnection - " << conn->peerAddress().toIpPort() << " - Not in white list";
            // FIXME: use socks4a to response refuse
            conn->forceClose();
        } else {
            LOG_INFO << "onServerConnection - " << conn->peerAddress().toIpPort() << " - In white list";
            conn->setTcpNoDelay(true);
        }
    } else {
        auto it = g_tunnels.find(conn->name());
        if(it != g_tunnels.end()) {
            it->second->disconnect();
            g_tunnels.erase(it);
        }
    }
}

void onServerMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp)
{
    LOG_DEBUG << conn->name() << " " << buf->readableBytes();
    if(g_tunnels.find(conn->name()) == g_tunnels.end()) {   // new conn, decode head according to protocol
        Socks socks;
        if(socks.decode(buf)) {
            InetAddress serverAddr(socks.addr());
            if(socks.ver() == 4 && socks.cmd() == 1) {
                TunnelPtr tunnel(std::make_shared<Tunnel>(g_eventLoop, serverAddr, conn));
                tunnel->setup();
                tunnel->connect();
                g_tunnels[conn->name()] = tunnel;
                conn->send(socks.toAllowResponse());
            } else {
                conn->send(socks.toRefuseResponse());
                conn->shutdown();
                conn->forceClose();
            }
        } else if(!socks.isValid()) {
            conn->shutdown();
            conn->forceClose();
        } // else: valid but not decoded
    } else if(!conn->getContext().empty()) {    // old conn
        const auto &destinationConn = boost::any_cast<const TcpConnectionPtr &>(conn->getContext());
        destinationConn->send(buf);
    }
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
    } else {
        LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

        uint16_t port = atoi(argv[1]);
        InetAddress listenAddr(port);

        EventLoop loop;
        g_eventLoop = &loop;

        TcpServer server(&loop, listenAddr, "Socks4");

        server.setConnectionCallback(onServerConnection);
        server.setMessageCallback(onServerMessage);

        server.start();

        loop.loop();
    }
}