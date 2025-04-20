#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装socket地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {
    }

    // 将地址转化为IP地址字符串
    std::string toIp() const;
    // 将地址转化为IP地址和端口号的字符串形式
    std::string toIpPort() const;
    // 将地址转化为端口号
    uint16_t toPort() const;

    // 获取当前对象封装的网络地址信息
    const sockaddr_in *getSockAddr() const { return &addr_; }
    // 设置当前对象封装的网络地址信息
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};