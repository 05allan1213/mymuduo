#include "TcpServer.h"

#include <functional>

#include "Logger.h"

// 强制要求传入的 EventLoop* loop (baseLoop) 不能为空
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg,
                     Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      // 将loop(baseLoop)传递给Acceptor，明确Acceptor在baseLoop中执行
      // 将监听地址(listenAddr)传递给 Acceptor，用于后续的 socket, bind, listen 操作
      // 根据 option 决定是否设置 SO_REUSEPORT 选项
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      // 此处只创建线程池对象，还未启动任何IO线程(subLoop)
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

void TcpServer::setThreadNum(int numThreads) { threadPool_->setThreadNum(numThreads); }

void TcpServer::start()
{
    // 防止重复启动
    if (started_++ == 0)
    {
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {}