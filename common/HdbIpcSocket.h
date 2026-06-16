#ifndef YSD_HDB_IPC_SOCKET_H
#define YSD_HDB_IPC_SOCKET_H

#include <string>
#include <vector>

#define HDB_IPC_DEFAULT_HOST "127.0.0.1" // 本机默认地址
#define HDB_IPC_DEFAULT_PORT 18150 // 默认监听端口

typedef unsigned long long HdbIpcSocketHandle;

#define HDB_IPC_INVALID_SOCKET_HANDLE ((HdbIpcSocketHandle)(~0ULL))

// 一条已连接的 TCP frame 连接
class CHdbIpcTcpConnection
{
public:
    CHdbIpcTcpConnection();
    ~CHdbIpcTcpConnection();

    int Attach(HdbIpcSocketHandle socketHandle);
    int Close();
    // SendFrame 会先解析一次 frame，避免把损坏帧写入连接
    int SendFrame(const std::vector<unsigned char>& frame);
    // RecvFrame 收满 header 和 body 后返回
    int RecvFrame(std::vector<unsigned char>& frame);
    const char* GetLastError() const;

private:
    int SendAll(const unsigned char* data, unsigned int length);
    int RecvExact(unsigned char* data, unsigned int length);
    void SetLastError(const char* text);
    void SetSocketLastError(const char* action);

private:
    HdbIpcSocketHandle m_socket; // 已接管的 socket
    std::string m_lastError;     // 最近错误文本
};

// TCP 短连接客户端
class CHdbIpcTcpClient
{
public:
    CHdbIpcTcpClient();

    // 每次 Request 建立短连接，发送一个请求并等待一个响应
    int Request(const char* host,
        int port,
        const std::vector<unsigned char>& requestFrame,
        std::vector<unsigned char>& responseFrame);
    const char* GetLastError() const;

private:
    void SetLastError(const char* text);
    void SetSocketLastError(const char* action);

private:
    std::string m_lastError; // 最近错误文本
};

// TCP 监听端
class CHdbIpcTcpServer
{
public:
    CHdbIpcTcpServer();
    ~CHdbIpcTcpServer();

    // Open 只监听一个 IPv4 地址和端口，backlog 非正时实现层使用默认值
    int Open(const char* host, int port, int backlog);
    int Accept(CHdbIpcTcpConnection& connection);
    int Close();
    const char* GetLastError() const;

private:
    void SetLastError(const char* text);
    void SetSocketLastError(const char* action);

private:
    HdbIpcSocketHandle m_listenSocket; // 监听 socket
    std::string m_lastError;           // 最近错误文本
};

#endif
