#include "TcpConnection.h"

#include <errno.h>
#include <functional>
#include <netinet/tcp.h>
#include <string>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

// 强制要求传入的 EventLoop* loop (baseLoop) 不能为空
static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd,
                             const InetAddress& localAddr, const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024)  // 64M
{
    // 设置 Channel 回调
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->fd(),
             (int)state_);
}

void TcpConnection::send(const std::string& buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing");
        return;
    }

    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote > 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }
    if (!faultError && remaining > 0)
    {
        size_t oldlen = outputBuffer_.readableBytes();
        if (oldlen + remaining >= highWaterMark_ && oldlen < highWaterMark_ &&
            highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}
// 连接断开
void TcpConnection::connectDestroyed()
{
    if (state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
//当 Poller 检测到 connfd 变为可读时，Channel会调用此方法
{
    int saveErrno = 0;
    // 从 connfd 读取数据，并将数据存入 inputBuffer_。
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0)  // 成功读取数据
    {
        // 这是网络库使用者最关心的回调之一(通常对应 onMessage)。
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)  // 对端关闭连接
    {
        handleClose();
    }
    else  // 发送错误
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
// 当 Poller 检测到 connfd 变为可写时 并且 Channel 当前正关注写事件
// (通常是因为上次 send操作未能一次性将 outputBuffer_ 中的数据全部发送出去)
{
    // 检查写状态
    if (channel_->isWriting())
    {
        int saveErrno = 0;
        // 将 outputBuffer_ 中缓存的数据写入 connfd
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if (n > 0)  // 成功写入部分或全部数据
        {
            // 移除已成功发送的数据
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            // 数据全部发送完毕
            {
                // 告知 Channel 不再需要关注写事件
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // 防御性编程，确保在下轮事件循环执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)  // 如果正在断开连接，则关闭连接
                {
                    shutdownInLoop();
                }
            }
            // 数据未发送完毕，此时channel_会继续关注写事件，再次调用handleWrite
        }
        else  // 写入失败
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else  // Channel不在写状态，却调用了handleWrite，异常
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    // 将连接状态更新为已断开
    setState(kDisconnected);
    // 移除 Channel
    channel_->disableAll();
    channel_->remove();
    // 执行连接断开回调函数
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}