//
// Created by clay on 22-10-13.
//

#include "tunnel.h"
#include <stdio.h>
#include <malloc.h>
#include <sys/resource.h>
#include <unistd.h>
#include <algorithm>

using namespace std;
using namespace muduo;
using namespace muduo::net;

EventLoop *g_eventLoop;
InetAddress *g_serverAddr;
std::map<string, TunnelPtr> g_tunnels;

void onServerConnection(const TcpConnectionPtr &conn)
{
    LOG_DEBUG << (conn->connected() ? "UP" : "DOWN");
    if(conn->connected()) { // connected
        conn->setTcpNoDelay(true);
        conn->stopRead();
        TunnelPtr tunnel(std::make_shared<Tunnel>(g_eventLoop, *g_serverAddr, conn));
        tunnel->setup();
        tunnel->connect();
        g_tunnels[conn->name()] = tunnel;
    } else {    // disconnected
        LOG_INFO << conn->name() << " - Source close actively";
        assert(g_tunnels.find(conn->name()) != g_tunnels.end());
        g_tunnels[conn->name()]->disconnect();
        g_tunnels.erase(conn->name());
    }
}

void onServerMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) // only receive requests from source
{
    LOG_DEBUG << buf->readableBytes();
    LOG_INFO << conn->name() << " - Request to destination";
    if(!conn->getContext().empty()) {   // tunnel has connected to destination
        const auto &sourceConn = boost::any_cast<const TcpConnectionPtr&>(conn->getContext());
#ifndef NOT_OUTPUT
        string req_str; // FIXME: O(buf->readableBytes())
        for_each(buf->toStringPiece().begin(), buf->toStringPiece().end(), [&req_str](const auto &i){
            if(i >= ' ' && i < 127) {
                req_str.push_back(i);
            } else {
                char format_num[5]; // 0 x 0 0 \0
                snprintf(format_num, sizeof format_num, "0x%02x", i);
                req_str.append(string("\\") + format_num);
            }
        });
        fprintf(stderr, "%s : %s - Request\n* begin *\n%s\n* end *\n", time.toFormattedString().c_str(), conn->name().c_str(), req_str.c_str());
#endif  // NOT_OUTPUT
        sourceConn->send(buf);  // send requests to destination
    }
}

void memstat()
{
    malloc_stats();
}

int main(int argc, char *argv[])
{
    if(argc < 4) {
        fprintf(stderr, "Usage: %s <host_ip> <port> <listen_port>\n", argv[0]);
    } else {
        LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();

        {
            // set max virtual memory to 256MB
            size_t kOneMB = 1024 * 1024;
            rlimit rl = { 256 * kOneMB, 256 * kOneMB };
            setrlimit(RLIMIT_AS, &rl);
        }

        // <host_ip> <port> <listen_port>
        const char *ip = argv[1];
        uint16_t port = atoi(argv[2]);
        InetAddress serverAddr(ip, port);   // destination addr
        g_serverAddr = &serverAddr;
        uint16_t acceptPort = atoi(argv[3]);
        InetAddress listenAddr(acceptPort);

        EventLoop loop;
        g_eventLoop = &loop;
        // print memory status every 30 secs
        // loop.runEvery(30, memstat);

        TcpServer server(&loop, listenAddr, "TcpRelay");
        server.setConnectionCallback(onServerConnection);
        server.setMessageCallback(onServerMessage);

        server.start();

        loop.loop();
    }
}