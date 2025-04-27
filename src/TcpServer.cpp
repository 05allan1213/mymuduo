#include "TcpServer.h"

#include <functional>
#include <strings.h>

#include "Logger.h"
#include "TcpConnection.h"

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
      nextConnId_(1),
      started_(0)
{
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    // 遍历并关闭所有连接
    for (auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second);  // 获取连接的shared_ptr
        item.second.reset();  // 置空shared_ptr，断开TcpServer对TcpConnection对象的强引用
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
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

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 轮询获取一个subLoop，以管理channel
    EventLoop* ioLoop = threadPool_->getNextLoop();

    // 生成连接名称
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n", name_.c_str(),
             connName.c_str(), peerAddr.toIpPort().c_str());

    // 获取本地地址
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    // 创建TcpConnection对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    // 存储新连接
    connections_[connName] = conn;
    // 设置TcpConnection回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 设置内部关闭回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 启动TCPConnection
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
// 此方法总是在TCPServer所属的 mainLoop 线程中执行
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(),
             conn->name().c_str());
    // 从map中删除
    connections_.erase(conn->name());
    // 获取连接所属的subLoop
    EventLoop* ioLoop = conn->getLoop();
    // 删除连接
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}