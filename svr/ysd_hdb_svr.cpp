#include "HdbPgAdapter.h"
#include "HdbIpcCommandHandler.h"
#include "HdbIpcServerContext.h"
#include "../common/HdbIpcSocket.h"
#include "../test/HdbSvrSelfTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

static const char* ReadDefaultConnInfo()
{
    const char* envConn;

    envConn = getenv("HDB_PG_CONNINFO");
    if (envConn != NULL && envConn[0] != '\0')
    {
        return envConn;
    }
    return "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=postgres";
}

static const char* ReadSelfTestConnInfo(int argc, char* argv[])
{
    if (argc > 2 && argv[2] != NULL && argv[2][0] != '\0')
    {
        return argv[2];
    }
    return ReadDefaultConnInfo();
}

static const char* ReadIpcHost()
{
    const char* envHost;

    envHost = getenv("HDB_IPC_HOST");
    if (envHost != NULL && envHost[0] != '\0')
    {
        return envHost;
    }
    return HDB_IPC_DEFAULT_HOST;
}

static int ReadIpcPort()
{
    const char* envPort;
    int port;

    envPort = getenv("HDB_IPC_PORT");
    if (envPort == NULL || envPort[0] == '\0')
    {
        return HDB_IPC_DEFAULT_PORT;
    }
    port = atoi(envPort);
    if (port <= 0 || port > 65535)
    {
        return HDB_IPC_DEFAULT_PORT;
    }
    return port;
}

static int RunIpcServer(CHdbPgAdapter& adapter)
{
    CHdbIpcTcpServer server;
    CHdbIpcServerContext context;
    CHdbIpcCommandHandler handler(&context);
    const char* host;
    int port;
    int ret;

    context.adapter = &adapter;
    host = ReadIpcHost();
    port = ReadIpcPort();
    ret = server.Open(host, port, 16);
    if (ret != HDB_IPC_OK)
    {
        printf("open ipc listen failed: %s\n", server.GetLastError());
        return 1;
    }
    printf("ysd_hdb_svr listening on %s:%d\n", host, port);
    // 第一版单线程串行处理，每个 TCP 连接只承载一次请求
    while (1)
    {
        CHdbIpcTcpConnection connection;
        std::vector<unsigned char> requestFrame;
        std::vector<unsigned char> responseFrame;

        ret = server.Accept(connection);
        if (ret != HDB_IPC_OK)
        {
            printf("ipc accept failed: %s\n", server.GetLastError());
        }
        else
        {
            ret = connection.RecvFrame(requestFrame);
            if (ret == HDB_IPC_OK)
            {
                ret = handler.HandleRequest(requestFrame, responseFrame);
                if (ret == HDB_IPC_OK)
                {
                    ret = connection.SendFrame(responseFrame);
                }
            }
        }
        if (ret != HDB_IPC_OK)
        {
            printf("ipc request failed: %d %s\n", ret, connection.GetLastError());
        }
        connection.Close();
    }
}

static int RunSelfTest(int argc, char* argv[])
{
    return RunHdbSvrSelfTest(ReadSelfTestConnInfo(argc, argv));
}

int main(int argc, char* argv[])
{
    const char* connInfo;
    CHdbPgAdapter adapter;
    int ret;

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
    {
        return RunSelfTest(argc, argv);
    }
    connInfo = ReadDefaultConnInfo();
    printf("ysd_hdb_svr service mode\n");
    printf("conninfo: %s\n", connInfo);
    ret = adapter.Open(connInfo);
    if (ret != HDB_OK)
    {
        printf("open postgres failed: %s\n", adapter.GetLastError());
        return 1;
    }
    ret = adapter.Ping();
    if (ret != HDB_OK)
    {
        printf("ping postgres failed: %s\n", adapter.GetLastError());
        return 2;
    }
    printf("postgres ping ok\n");
    return RunIpcServer(adapter);
}
