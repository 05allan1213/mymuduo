#pragma once

#include <algorithm>
#include <string>
#include <vector>

// +-------------------+------------------+------------------+
// | prependable bytes |  readable bytes  |  writable bytes  |
// |    (前置预留区)    |   (可读数据区)    |   (可写数据区)    |
// +-------------------+------------------+------------------+
// |                   |                  |                  |
// 0      <=      readerIndex   <=   writerIndex    <=     size

class Buffer
{
   public:
    static const size_t kCheapPrepend = 8;  // 前置预留区,大小8字节
    static const size_t kInitialSize = 1024;  // 缓冲区(readable + writable)的初始大小,大小1024字节

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),  // 缓冲区总大小 = 预留区 + 初始大小
          readerIndex_(kCheapPrepend),           // 读索引指向预留区之后
          writerIndex_(kCheapPrepend)            // 写索引初始时与读索引相同
    {
    }

    // 返回可读数据的长度
    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    // 返回可写空间的长度
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    // 返回前置预留区的长度
    size_t prependableBytes() const { return readerIndex_; }

    // 返回可读数据的起始地址
    const char* peek() const { return begin() + readerIndex_; }
    // 返回可写区域的起始地址
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    // 从缓冲区读了len字节的数据
    void retrieve(size_t len)
    {
        if (len < readableBytes())  // 只读取了一部分可读数据
        {
            readerIndex_ += len;
        }
        else  // 所有可读数据都被读取了
        {
            retrieveAll();
        }
    }
    // 读完缓冲区所有数据,并执行复位操作
    void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }
    // 从缓冲区读取len字节的数据,并作为字符串返回
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    // 从缓冲区读取len字节的数据,作为字符串返回,并执行复位操作
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }
    // 确保缓冲区至少有len字节的可写空间
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);  // 扩容
        }
    }
    // 向缓冲区写入len字节的数据
    void append(const char* data, size_t len)
    {
        // 1. 确保空间
        ensureWriteableBytes(len);
        // 2. 拷贝数据
        std::copy(data, data + len, beginWrite());
        // 3. 更新写索引
        writerIndex_ += len;
    }
    // 从指定fd中读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // 向指定fd中写入数据
    ssize_t writeFd(int fd, int* saveErrno);

   private:
    // 返回底层 vector 存储区的起始地址
    char* begin()
    {
        return &*buffer_.begin();  // vector底层数组首元素的地址，也就是数组的起始地址
    }
    const char* begin() const { return &*buffer_.begin(); }

    // 扩容
    void makeSpace(size_t len)
    {
        // 策略二：移动数据(空间复用)
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        // 策略一：扩容(内存重新分配)
        else
        {
            size_t readalbe = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readalbe;
        }
    }

    std::vector<char> buffer_;  // 缓冲区
    size_t readerIndex_;        // 读索引，应用程序从这里开始读取数据
    size_t writerIndex_;        // 写索引，新数据从这里开始写入
};