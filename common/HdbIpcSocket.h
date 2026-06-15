#ifndef YSD_HDB_IPC_SOCKET_H
#define YSD_HDB_IPC_SOCKET_H

#include <string>
#include <vector>

#define HDB_IPC_DEFAULT_HOST "127.0.0.1"
#define HDB_IPC_DEFAULT_PORT 18150

typedef unsigned long long HdbIpcSocketHandle;

#define HDB_IPC_INVALID_SOCKET_HANDLE ((HdbIpcSocketHandle)(~0ULL))

class CHdbIpcTcpConnection
{
public:
    CHdbIpcTcpConnection();
    ~CHdbIpcTcpConnection();

    int Attach(HdbIpcSocketHandle socketHandle);
    int Close();
    int SendFrame(const std::vector<unsigned char>& frame);
    int RecvFrame(std::vector<unsigned char>& frame);
    const char* GetLastError() const;

private:
    int SendAll(const unsigned char* data, unsigned int length);
    int RecvExact(unsigned char* data, unsigned int length);
    void SetLastError(const char* text);
    void SetSocketLastError(const char* action);

private:
    HdbIpcSocketHandle m_socket;
    std::string m_lastError;
};

class CHdbIpcTcpClient
{
public:
    CHdbIpcTcpClient();

    int Request(const char* host,
        int port,
        const std::vector<unsigned char>& requestFrame,
        std::vector<unsigned char>& responseFrame);
    const char* GetLastError() const;

private:
    void SetLastError(const char* text);
    void SetSocketLastError(const char* action);

private:
    std::string m_lastError;
};

class CHdbIpcTcpServer
{
public:
    CHdbIpcTcpServer();
    ~CHdbIpcTcpServer();

    int Open(const char* host, int port, int backlog);
    int Accept(CHdbIpcTcpConnection& connection);
    int Close();
    const char* GetLastError() const;

private:
    void SetLastError(const char* text);
    void SetSocketLastError(const char* action);

private:
    HdbIpcSocketHandle m_listenSocket;
    std::string m_lastError;
};

#endif
