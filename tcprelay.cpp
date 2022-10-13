//
// Created by clay on 22-10-13.
//

#include "tunnel.h"

#include "tunnel.h"
#include <stdio.h>
#include <malloc.h>
#include <sys/resource.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

EventLoop *g_eventLoop;
InetAddress *g_serverAddr;
std::map<string, TunnelPtr> g_tunnels;

void onServerConnection(const TcpConnectionPtr &conn)
{

}

void onServerMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp)
{

}

void memstat()
{
    malloc_stats();
}

int main()
{
    return 0;
}