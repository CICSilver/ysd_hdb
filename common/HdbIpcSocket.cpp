#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "HdbIpcSocket.h"

#include "HdbIpcProtocol.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include <winsock2.h>
typedef SOCKET HdbNativeSocket;
#define HDB_IPC_SOCKET_SNPRINTF _snprintf
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int HdbNativeSocket;
#define HDB_IPC_SOCKET_SNPRINTF snprintf
#endif

#ifdef _WIN32
static int HdbIpcSocketStartup()
{
    static int started = 0;
    WSADATA data;

    if (started)
    {
        return HDB_IPC_OK;
    }
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        return HDB_IPC_ERR_BUFFER;
    }
    started = 1;
    return HDB_IPC_OK;
}

static void HdbIpcCloseSocket(HdbIpcSocketHandle socketHandle)
{
    if (socketHandle != HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        closesocket((HdbNativeSocket)socketHandle);
    }
}

static int HdbIpcGetSocketError()
{
    return WSAGetLastError();
}
#else
static int HdbIpcSocketStartup()
{
    signal(SIGPIPE, SIG_IGN);
    return HDB_IPC_OK;
}

static void HdbIpcCloseSocket(HdbIpcSocketHandle socketHandle)
{
    if (socketHandle != HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        close((int)socketHandle);
    }
}

static int HdbIpcGetSocketError()
{
    return errno;
}

static int HdbIpcSendFlags()
{
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}
#endif

static const char* HdbIpcNormalizeHost(const char* host)
{
    if (host == NULL || host[0] == '\0')
    {
        return HDB_IPC_DEFAULT_HOST;
    }
    return host;
}

static int HdbIpcCheckPort(int port)
{
    if (port <= 0 || port > 65535)
    {
        return HDB_IPC_ERR_PARAM;
    }
    return HDB_IPC_OK;
}

static int HdbIpcBuildSockAddr(const char* host, int port, sockaddr_in& address)
{
    unsigned long ip;

    if (HdbIpcCheckPort(port) != HDB_IPC_OK)
    {
        return HDB_IPC_ERR_PARAM;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);

    ip = inet_addr(HdbIpcNormalizeHost(host));
    if (ip == INADDR_NONE)
    {
        return HDB_IPC_ERR_PARAM;
    }
    address.sin_addr.s_addr = ip;
    return HDB_IPC_OK;
}

CHdbIpcTcpConnection::CHdbIpcTcpConnection()
{
    m_socket = HDB_IPC_INVALID_SOCKET_HANDLE;
}

CHdbIpcTcpConnection::~CHdbIpcTcpConnection()
{
    Close();
}

int CHdbIpcTcpConnection::Attach(HdbIpcSocketHandle socketHandle)
{
    if (socketHandle == HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        SetLastError("invalid socket");
        return HDB_IPC_ERR_PARAM;
    }

    Close();
    m_socket = socketHandle;
    m_lastError.clear();
    return HDB_IPC_OK;
}

int CHdbIpcTcpConnection::Close()
{
    if (m_socket != HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        HdbIpcCloseSocket(m_socket);
        m_socket = HDB_IPC_INVALID_SOCKET_HANDLE;
    }
    return HDB_IPC_OK;
}

int CHdbIpcTcpConnection::SendFrame(const std::vector<unsigned char>& frame)
{
    HdbIpcFrame parsedFrame;
    int ret;

    if (frame.empty())
    {
        SetLastError("empty frame");
        return HDB_IPC_ERR_PARAM;
    }
    if (m_socket == HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        SetLastError("socket not connected");
        return HDB_IPC_ERR_BUFFER;
    }

    // XXX 发送前解析一次，避免把半帧或损坏帧写入连接
    ret = HdbIpcParseFrame(&frame[0], (unsigned int)frame.size(), parsedFrame);
    if (ret != HDB_IPC_OK)
    {
        SetLastError("invalid frame");
        return ret;
    }

    return SendAll(&frame[0], (unsigned int)frame.size());
}

int CHdbIpcTcpConnection::RecvFrame(std::vector<unsigned char>& frame)
{
    HdbIpcFrameHeader header;
    HdbIpcFrame parsedFrame;
    unsigned int frameSize;
    int ret;

    frame.clear();
    if (m_socket == HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        SetLastError("socket not connected");
        return HDB_IPC_ERR_BUFFER;
    }

    ret = RecvExact((unsigned char*)&header, (unsigned int)sizeof(header));
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }

    frameSize = 0;
    ret = HdbIpcGetFrameSize((const unsigned char*)&header,
        (unsigned int)sizeof(header),
        &frameSize);
    if (ret != HDB_IPC_OK && ret != HDB_IPC_ERR_INCOMPLETE)
    {
        SetLastError("invalid frame header");
        return ret;
    }
    if (frameSize < (unsigned int)sizeof(header) ||
        frameSize > (unsigned int)sizeof(header) + HDB_IPC_MAX_BODY_LENGTH)
    {
        SetLastError("invalid frame size");
        return HDB_IPC_ERR_BODY_SIZE;
    }

    frame.resize(frameSize);
    memcpy(&frame[0], &header, sizeof(header));
    if (frameSize > (unsigned int)sizeof(header))
    {
        ret = RecvExact(&frame[sizeof(header)], frameSize - (unsigned int)sizeof(header));
        if (ret != HDB_IPC_OK)
        {
            frame.clear();
            return ret;
        }
    }

    ret = HdbIpcParseFrame(&frame[0], (unsigned int)frame.size(), parsedFrame);
    if (ret != HDB_IPC_OK)
    {
        SetLastError("invalid frame checksum");
        frame.clear();
        return ret;
    }
    return HDB_IPC_OK;
}

const char* CHdbIpcTcpConnection::GetLastError() const
{
    return m_lastError.c_str();
}

int CHdbIpcTcpConnection::SendAll(const unsigned char* data, unsigned int length)
{
    unsigned int offset;

    if (data == NULL && length > 0)
    {
        SetLastError("send buffer is null");
        return HDB_IPC_ERR_PARAM;
    }

    offset = 0;
    while (offset < length)
    {
        int chunk;
        int sent;

        chunk = (length - offset) > 65536u ? 65536 : (int)(length - offset);
#ifdef _WIN32
        sent = send((HdbNativeSocket)m_socket, (const char*)(data + offset), chunk, 0);
#else
        do
        {
            sent = send((HdbNativeSocket)m_socket, (const char*)(data + offset), chunk, HdbIpcSendFlags());
        }
        while (sent < 0 && errno == EINTR);
#endif
        if (sent <= 0)
        {
            SetSocketLastError("send");
            return HDB_IPC_ERR_BUFFER;
        }
        offset += (unsigned int)sent;
    }
    return HDB_IPC_OK;
}

int CHdbIpcTcpConnection::RecvExact(unsigned char* data, unsigned int length)
{
    unsigned int offset;

    if (data == NULL && length > 0)
    {
        SetLastError("recv buffer is null");
        return HDB_IPC_ERR_PARAM;
    }

    offset = 0;
    while (offset < length)
    {
        int chunk;
        int received;

        chunk = (length - offset) > 65536u ? 65536 : (int)(length - offset);
#ifdef _WIN32
        received = recv((HdbNativeSocket)m_socket, (char*)(data + offset), chunk, 0);
#else
        do
        {
            received = recv((HdbNativeSocket)m_socket, (char*)(data + offset), chunk, 0);
        }
        while (received < 0 && errno == EINTR);
#endif
        if (received == 0)
        {
            SetLastError("socket closed");
            return HDB_IPC_ERR_INCOMPLETE;
        }
        if (received < 0)
        {
            SetSocketLastError("recv");
            return HDB_IPC_ERR_BUFFER;
        }
        offset += (unsigned int)received;
    }
    return HDB_IPC_OK;
}

void CHdbIpcTcpConnection::SetLastError(const char* text)
{
    m_lastError = text == NULL ? "" : text;
}

void CHdbIpcTcpConnection::SetSocketLastError(const char* action)
{
    char buffer[128];

    HDB_IPC_SOCKET_SNPRINTF(buffer,
        sizeof(buffer) - 1,
        "%s socket error %d",
        action == NULL ? "socket" : action,
        HdbIpcGetSocketError());
    buffer[sizeof(buffer) - 1] = '\0';
    m_lastError = buffer;
}

CHdbIpcTcpClient::CHdbIpcTcpClient()
{
}

int CHdbIpcTcpClient::Request(const char* host,
    int port,
    const std::vector<unsigned char>& requestFrame,
    std::vector<unsigned char>& responseFrame)
{
    sockaddr_in address;
    CHdbIpcTcpConnection connection;
    HdbNativeSocket nativeSocket;
    HdbIpcSocketHandle socketHandle;
    int ret;

    responseFrame.clear();
    ret = HdbIpcSocketStartup();
    if (ret != HDB_IPC_OK)
    {
        SetLastError("socket startup failed");
        return ret;
    }
    ret = HdbIpcBuildSockAddr(host, port, address);
    if (ret != HDB_IPC_OK)
    {
        SetLastError("invalid endpoint");
        return ret;
    }

    nativeSocket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (nativeSocket == INVALID_SOCKET)
#else
    if (nativeSocket < 0)
#endif
    {
        SetSocketLastError("socket");
        return HDB_IPC_ERR_BUFFER;
    }
    socketHandle = (HdbIpcSocketHandle)nativeSocket;
    if (connect(nativeSocket, (const sockaddr*)&address, sizeof(address)) != 0)
    {
        SetSocketLastError("connect");
        HdbIpcCloseSocket(socketHandle);
        return HDB_IPC_ERR_BUFFER;
    }

    connection.Attach(socketHandle);
    ret = connection.SendFrame(requestFrame);
    if (ret != HDB_IPC_OK)
    {
        m_lastError = connection.GetLastError();
        return ret;
    }
    ret = connection.RecvFrame(responseFrame);
    if (ret != HDB_IPC_OK)
    {
        m_lastError = connection.GetLastError();
        return ret;
    }
    return HDB_IPC_OK;
}

const char* CHdbIpcTcpClient::GetLastError() const
{
    return m_lastError.c_str();
}

void CHdbIpcTcpClient::SetLastError(const char* text)
{
    m_lastError = text == NULL ? "" : text;
}

void CHdbIpcTcpClient::SetSocketLastError(const char* action)
{
    char buffer[128];

    HDB_IPC_SOCKET_SNPRINTF(buffer,
        sizeof(buffer) - 1,
        "%s socket error %d",
        action == NULL ? "socket" : action,
        HdbIpcGetSocketError());
    buffer[sizeof(buffer) - 1] = '\0';
    m_lastError = buffer;
}

CHdbIpcTcpServer::CHdbIpcTcpServer()
{
    m_listenSocket = HDB_IPC_INVALID_SOCKET_HANDLE;
}

CHdbIpcTcpServer::~CHdbIpcTcpServer()
{
    Close();
}

int CHdbIpcTcpServer::Open(const char* host, int port, int backlog)
{
    sockaddr_in address;
    HdbNativeSocket nativeSocket;
    HdbIpcSocketHandle socketHandle;
    int ret;
    int reuseValue;

    Close();
    ret = HdbIpcSocketStartup();
    if (ret != HDB_IPC_OK)
    {
        SetLastError("socket startup failed");
        return ret;
    }
    ret = HdbIpcBuildSockAddr(host, port, address);
    if (ret != HDB_IPC_OK)
    {
        SetLastError("invalid endpoint");
        return ret;
    }

    nativeSocket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (nativeSocket == INVALID_SOCKET)
#else
    if (nativeSocket < 0)
#endif
    {
        SetSocketLastError("socket");
        return HDB_IPC_ERR_BUFFER;
    }
    socketHandle = (HdbIpcSocketHandle)nativeSocket;

    reuseValue = 1;
    setsockopt(nativeSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        (const char*)&reuseValue,
        (int)sizeof(reuseValue));

    if (bind(nativeSocket, (const sockaddr*)&address, sizeof(address)) != 0)
    {
        HdbIpcCloseSocket(socketHandle);
        SetSocketLastError("bind");
        return HDB_IPC_ERR_BUFFER;
    }
    if (listen(nativeSocket, backlog > 0 ? backlog : 16) != 0)
    {
        HdbIpcCloseSocket(socketHandle);
        SetSocketLastError("listen");
        return HDB_IPC_ERR_BUFFER;
    }

    m_listenSocket = socketHandle;
    m_lastError.clear();
    return HDB_IPC_OK;
}

int CHdbIpcTcpServer::Accept(CHdbIpcTcpConnection& connection)
{
    HdbNativeSocket nativeSocket;
    HdbIpcSocketHandle socketHandle;

    if (m_listenSocket == HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        SetLastError("listen socket not open");
        return HDB_IPC_ERR_BUFFER;
    }

#ifdef _WIN32
    nativeSocket = accept((HdbNativeSocket)m_listenSocket, NULL, NULL);
    if (nativeSocket == INVALID_SOCKET)
#else
    do
    {
        nativeSocket = accept((HdbNativeSocket)m_listenSocket, NULL, NULL);
    }
    while (nativeSocket < 0 && errno == EINTR);
    if (nativeSocket < 0)
#endif
    {
        SetSocketLastError("accept");
        return HDB_IPC_ERR_BUFFER;
    }
    socketHandle = (HdbIpcSocketHandle)nativeSocket;

    return connection.Attach(socketHandle);
}

int CHdbIpcTcpServer::Close()
{
    if (m_listenSocket != HDB_IPC_INVALID_SOCKET_HANDLE)
    {
        HdbIpcCloseSocket(m_listenSocket);
        m_listenSocket = HDB_IPC_INVALID_SOCKET_HANDLE;
    }
    return HDB_IPC_OK;
}

const char* CHdbIpcTcpServer::GetLastError() const
{
    return m_lastError.c_str();
}

void CHdbIpcTcpServer::SetLastError(const char* text)
{
    m_lastError = text == NULL ? "" : text;
}

void CHdbIpcTcpServer::SetSocketLastError(const char* action)
{
    char buffer[128];

    HDB_IPC_SOCKET_SNPRINTF(buffer,
        sizeof(buffer) - 1,
        "%s socket error %d",
        action == NULL ? "socket" : action,
        HdbIpcGetSocketError());
    buffer[sizeof(buffer) - 1] = '\0';
    m_lastError = buffer;
}
