//
// Created by clay on 10/14/22.
//

#ifndef PROXY_SOCKS_H
#define PROXY_SOCKS_H

#include <muduo/net/Endian.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

class Socks {
public:
    Socks() : ver_(0), cmd_(0), addr_(), valid_(true) {}

    enum command {
        CONNECT = 1,
        BIND = 2
    };

    bool decode(muduo::net::Buffer *buf);
    char& ver() { return ver_; }
    char& cmd() { return cmd_; }
    const muduo::net::InetAddress& addr() { return addr_; }
    bool isValid() const { return valid_; }
    std::string toAllowResponse();
    std::string toRefuseResponse();
    std::string toConnectionRefuseResponse();

private:
    char ver_;
    char cmd_;
    muduo::net::InetAddress addr_;
    bool valid_;
};


#endif //PROXY_SOCKS_H
