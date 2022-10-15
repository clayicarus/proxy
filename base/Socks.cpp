//
// Created by clay on 10/14/22.
//

#include "Socks.h"
#include <muduo/base/Logging.h>

using namespace muduo::net;
using namespace muduo;

bool Socks::decode(muduo::net::Buffer *buf)
{
    if(buf->readableBytes() > 128) {
        valid_ = false;
        return false;
    } else if(buf->readableBytes() > 8) {
        if(buf->readableBytes() > 8) {
            const char *begin = buf->peek() + 8;
            const char *end = buf->peek() + buf->readableBytes();
            const char *where = std::find(begin, end, '\0');
            if (where != end) {
                char ver = buf->peek()[0];
                char cmd = buf->peek()[1];
                const void *port = buf->peek() + 2;
                const void *ip = buf->peek() + 4;

                sockaddr_in addr;
                memZero(&addr, sizeof addr);
                addr.sin_family = AF_INET;
                addr.sin_port = *static_cast<const uint16_t *>(port);
                addr.sin_addr.s_addr = *static_cast<const uint32_t *>(ip);

                bool socks4a = sockets::networkToHost32(addr.sin_addr.s_addr) < 256;    // check DSTIP is socks4 or socks4a
                if (socks4a) {  // < 256 is hostname (socks4a)
                    const char *endOfHostName = std::find(where + 1, end, '\0');
                    if (endOfHostName != end) {
                        string hostname = where + 1;
                        where = endOfHostName;
                        LOG_INFO << "Socks4a host name " << hostname;
                        InetAddress temp;
                        if (InetAddress::resolve(hostname, &temp)) {
                            addr.sin_addr.s_addr = temp.ipv4NetEndian();
                        } else {    // resolve failed
                            valid_ = false;
                            return false;
                        }
                    } else {    // else endOfHostName == end
                        return false;
                    }
                }   // else >= 256 is host_ip (socks4)

                ver_ = ver;
                cmd_ = cmd;
                addr_ = InetAddress(addr);
                buf->retrieveUntil(where + 1);
                return true;
            }   // else '\0' not found, valid
        }   // else head <= 8, valid
    }
    return false;
}

std::string Socks::toAllowResponse()
{
    char response[] = "\0\x5aPTIPV4";
    auto port = addr_.port();
    auto ip = addr_.ipv4NetEndian();
    memcpy(response + 2, &port, 2);
    memcpy(response + 4, &ip, 4);

    return string(response, response + 8);
}

std::string Socks::toRefuseResponse()
{
    char response[] = "\0\x5bPTIPV4";
    return string(response, response + 8);
}

std::string Socks::toConnectionRefuseResponse()
{
    char response[] = "\0\x5cPTIPV4";
    return string(response, response + 8);
}

bool Socks::resolveHostname()
{
    InetAddress tmp(addr_);

    auto sock = *addr_.getSockAddr();
    sockaddr_in sock_addr = *reinterpret_cast<sockaddr_in*>(&sock);

    return false;
}


