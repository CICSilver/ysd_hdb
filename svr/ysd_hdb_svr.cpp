#include "HdbPgAdapter.h"
#include "HdbDatasetRegistry.h"
#include "HdbQueryExecutor.h"
#include "../common/HdbIpcProtocol.h"
#include "../common/HdbIpcResultCodec.h"
#include "../common/HdbIpcSocket.h"
#include "../test/HdbSvrSelfTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
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

static const unsigned char* GetVectorBuffer(const std::vector<unsigned char>& data)
{
    if (data.empty())
    {
        return NULL;
    }
    return &data[0];
}

static void ConvertQueryResultToIpcResult(const CHdbQueryResult& queryResult, HdbIpcResultSet& ipcResult)
{
    int rowIndex;
    int fieldIndex;

    ipcResult.Clear();
    for (fieldIndex = 0; fieldIndex < queryResult.FieldCount(); ++fieldIndex)
    {
        ipcResult.columns.push_back(queryResult.GetColumnName(fieldIndex));
    }
    for (rowIndex = 0; rowIndex < queryResult.RowCount(); ++rowIndex)
    {
        std::vector<HdbIpcResultCell> row;
        for (fieldIndex = 0; fieldIndex < queryResult.FieldCount(); ++fieldIndex)
        {
            HdbIpcResultCell cell;

            cell.isNull = queryResult.IsNull(rowIndex, fieldIndex);
            if (cell.isNull)
            {
                cell.value.clear();
            }
            else
            {
                cell.value = queryResult.GetValue(rowIndex, fieldIndex);
            }
            row.push_back(cell);
        }
        ipcResult.rows.push_back(row);
    }
}

static int BuildIpcResponse(unsigned int command,
    unsigned int sequence,
    int status,
    const std::vector<unsigned char>& body,
    std::vector<unsigned char>& responseFrame)
{
    return HdbIpcBuildResponse(command,
        sequence,
        status,
        GetVectorBuffer(body),
        (unsigned int)body.size(),
        responseFrame);
}

static int BuildIpcErrorResponse(unsigned int command,
    unsigned int sequence,
    int status,
    const char* errorText,
    std::vector<unsigned char>& responseFrame)
{
    std::vector<unsigned char> body;
    int ret;

    if (status == HDB_OK)
    {
        status = HDB_ERR_DB_EXEC;
    }
    ret = HdbIpcAppendString(body, HDB_IPC_FIELD_ERROR_TEXT, errorText == NULL ? "" : errorText);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    return BuildIpcResponse(command, sequence, status, body, responseFrame);
}

static int ExecuteIpcQuery(CHdbPgAdapter& adapter,
    const HdbIpcFrame& requestFrame,
    std::vector<unsigned char>& responseFrame)
{
    CHdbIpcFieldReader reader;
    HdbIpcField field;
    CHdbDatasetRegistry registry;
    CHdbQueryExecutor executor(&adapter, &registry);
    CHdbQueryAst ast;
    CHdbQueryResult queryResult;
    HdbIpcResultSet ipcResult;
    std::vector<unsigned char> body;
    std::vector<unsigned char> schemaData;
    std::vector<unsigned char> rowData;
    std::string astText;
    int hasField;
    int hasQueryAst;
    int ret;

    hasQueryAst = 0;
    ret = reader.Reset(requestFrame.body, requestFrame.bodyLength);
    if (ret != HDB_IPC_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_BUFFER,
            "invalid request body",
            responseFrame);
    }
    while (1)
    {
        ret = reader.Next(field, &hasField);
        if (ret != HDB_IPC_OK)
        {
            return BuildIpcErrorResponse(requestFrame.header.command,
                requestFrame.header.sequence,
                HDB_ERR_BUFFER,
                "invalid request field",
                responseFrame);
        }
        if (hasField == 0)
        {
            break;
        }
        if (field.type == HDB_IPC_FIELD_QUERY_AST)
        {
            ret = HdbIpcReadString(field, astText);
            if (ret != HDB_IPC_OK)
            {
                return BuildIpcErrorResponse(requestFrame.header.command,
                    requestFrame.header.sequence,
                    HDB_ERR_BUFFER,
                    "invalid query ast",
                    responseFrame);
            }
            hasQueryAst = 1;
        }
    }
    if (hasQueryAst == 0)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_PARAM,
            "query ast is missing",
            responseFrame);
    }

    ret = ast.Deserialize(astText.c_str());
    if (ret != HDB_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            ret,
            "query ast deserialize failed",
            responseFrame);
    }
    // SERVER 端重新校验 AST 并生成 SQL，DLL 不传递 SQL
    ret = executor.Execute(ast, queryResult);
    if (ret != HDB_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            ret,
            executor.GetLastError(),
            responseFrame);
    }

    ConvertQueryResultToIpcResult(queryResult, ipcResult);
    ret = HdbIpcEncodeResultSchema(ipcResult, schemaData);
    if (ret != HDB_IPC_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_BUFFER,
            "encode result schema failed",
            responseFrame);
    }
    ret = HdbIpcEncodeResultRows(ipcResult, rowData);
    if (ret != HDB_IPC_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_BUFFER,
            "encode result rows failed",
            responseFrame);
    }
    ret = HdbIpcAppendField(body,
        HDB_IPC_FIELD_RESULT_SCHEMA,
        schemaData.empty() ? NULL : &schemaData[0],
        (unsigned int)schemaData.size());
    if (ret != HDB_IPC_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_BUFFER,
            "append result schema failed",
            responseFrame);
    }
    ret = HdbIpcAppendField(body,
        HDB_IPC_FIELD_RESULT_ROWS,
        rowData.empty() ? NULL : &rowData[0],
        (unsigned int)rowData.size());
    if (ret != HDB_IPC_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_BUFFER,
            "append result rows failed",
            responseFrame);
    }
    return BuildIpcResponse(requestFrame.header.command,
        requestFrame.header.sequence,
        HDB_OK,
        body,
        responseFrame);
}

static int HandleIpcRequest(CHdbPgAdapter& adapter,
    const std::vector<unsigned char>& requestBytes,
    std::vector<unsigned char>& responseFrame)
{
    HdbIpcFrame requestFrame;
    std::vector<unsigned char> body;
    int ret;

    ret = HdbIpcParseFrame(GetVectorBuffer(requestBytes), (unsigned int)requestBytes.size(), requestFrame);
    if (ret != HDB_IPC_OK)
    {
        return BuildIpcErrorResponse(HDB_IPC_CMD_PING, 0, HDB_ERR_BUFFER, "invalid ipc frame", responseFrame);
    }
    if ((requestFrame.header.flags & HDB_IPC_FLAG_REQUEST) == 0)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_PARAM,
            "ipc frame is not request",
            responseFrame);
    }
    if (requestFrame.header.command == HDB_IPC_CMD_PING)
    {
        return BuildIpcResponse(requestFrame.header.command, requestFrame.header.sequence, HDB_OK, body, responseFrame);
    }
    if (requestFrame.header.command == HDB_IPC_CMD_DB_PING)
    {
        ret = adapter.Ping();
        if (ret != HDB_OK)
        {
            return BuildIpcErrorResponse(requestFrame.header.command,
                requestFrame.header.sequence,
                ret,
                adapter.GetLastError(),
                responseFrame);
        }
        return BuildIpcResponse(requestFrame.header.command, requestFrame.header.sequence, HDB_OK, body, responseFrame);
    }
    if (requestFrame.header.command == HDB_IPC_CMD_QUERY_EXECUTE)
    {
        return ExecuteIpcQuery(adapter, requestFrame, responseFrame);
    }
    return BuildIpcErrorResponse(requestFrame.header.command,
        requestFrame.header.sequence,
        HDB_ERR_NOT_IMPLEMENTED,
        "ipc command not implemented",
        responseFrame);
}

static int RunIpcServer(CHdbPgAdapter& adapter)
{
    CHdbIpcTcpServer server;
    const char* host;
    int port;
    int ret;

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
                ret = HandleIpcRequest(adapter, requestFrame, responseFrame);
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
