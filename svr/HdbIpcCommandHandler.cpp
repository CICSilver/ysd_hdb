#include "HdbIpcCommandHandler.h"

#include "HdbQueryExecutor.h"
#include "../common/HdbIpcResultCodec.h"

#include <new>
#include <string>

static const unsigned char* HdbHandlerGetVectorBuffer(const std::vector<unsigned char>& data)
{
    if (data.empty())
    {
        return NULL;
    }
    return &data[0];
}

static void HdbHandlerConvertQueryResult(const CHdbQueryResult& queryResult,
    const std::vector<int>& outputTypes,
    HdbIpcResultSet& ipcResult)
{
    int rowIndex;
    int fieldIndex;

    // SERVER 查询结果转成 IPC 结构时保留输出类型，DLL 读取时再做类型检查
    ipcResult.Clear();
    for (fieldIndex = 0; fieldIndex < queryResult.FieldCount(); ++fieldIndex)
    {
        HdbIpcResultColumn column;

        column.name = queryResult.GetColumnName(fieldIndex);
        column.fieldType = fieldIndex < (int)outputTypes.size() ? outputTypes[fieldIndex] : HDB_FT_CHAR_ARRAY;
        ipcResult.columns.push_back(column);
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

CHdbIpcCommandHandler::CHdbIpcCommandHandler(CHdbIpcServerContext* context)
    : m_context(context)
{
}

int CHdbIpcCommandHandler::HandleRequest(const std::vector<unsigned char>& requestBytes,
    std::vector<unsigned char>& responseFrame)
{
    HdbIpcFrame requestFrame;
    int hasRequestFrame;
    int ret;

    hasRequestFrame = 0;
    try
    {
        ret = HdbIpcParseFrame(HdbHandlerGetVectorBuffer(requestBytes),
            (unsigned int)requestBytes.size(),
            requestFrame);
        if (ret != HDB_IPC_OK)
        {
            // 请求帧解析失败时还没有命令号，只能回一个通用 PING 错误响应
            return BuildIpcErrorResponse(HDB_IPC_CMD_PING, 0, HDB_ERR_BUFFER, "invalid ipc frame", responseFrame);
        }
        hasRequestFrame = 1;
        if ((requestFrame.header.flags & HDB_IPC_FLAG_REQUEST) == 0)
        {
            return BuildIpcErrorResponse(requestFrame.header.command,
                requestFrame.header.sequence,
                HDB_ERR_PARAM,
                "ipc frame is not request",
                responseFrame);
        }
        return DispatchRequest(requestFrame, responseFrame);
    }
    catch (const std::bad_alloc&)
    {
        if (hasRequestFrame != 0)
        {
            return BuildIpcErrorResponse(requestFrame.header.command,
                requestFrame.header.sequence,
                HDB_ERR_BUFFER,
                "memory allocation failed in ipc handler",
                responseFrame);
        }
        return BuildIpcErrorResponse(HDB_IPC_CMD_PING,
            0,
            HDB_ERR_BUFFER,
            "memory allocation failed in ipc handler",
            responseFrame);
    }
    catch (...)
    {
        if (hasRequestFrame != 0)
        {
            return BuildIpcErrorResponse(requestFrame.header.command,
                requestFrame.header.sequence,
                HDB_ERR_INTERNAL,
                "unhandled exception in ipc handler",
                responseFrame);
        }
        return BuildIpcErrorResponse(HDB_IPC_CMD_PING,
            0,
            HDB_ERR_INTERNAL,
            "unhandled exception in ipc handler",
            responseFrame);
    }
}

int CHdbIpcCommandHandler::DispatchRequest(const HdbIpcFrame& requestFrame,
    std::vector<unsigned char>& responseFrame)
{
    // 分发层只认协议命令，具体 SQL 和数据库访问交给后面的执行器
    if (requestFrame.header.command == HDB_IPC_CMD_PING)
    {
        return HandlePing(requestFrame, responseFrame);
    }
    if (requestFrame.header.command == HDB_IPC_CMD_DB_PING)
    {
        return HandleDbPing(requestFrame, responseFrame);
    }
    if (requestFrame.header.command == HDB_IPC_CMD_QUERY_EXECUTE)
    {
        return HandleQueryExecute(requestFrame, responseFrame);
    }
    return BuildIpcErrorResponse(requestFrame.header.command,
        requestFrame.header.sequence,
        HDB_ERR_NOT_IMPLEMENTED,
        "ipc command not implemented",
        responseFrame);
}

int CHdbIpcCommandHandler::HandlePing(const HdbIpcFrame& requestFrame,
    std::vector<unsigned char>& responseFrame)
{
    std::vector<unsigned char> body;

    return BuildIpcResponse(requestFrame.header.command,
        requestFrame.header.sequence,
        HDB_OK,
        body,
        responseFrame);
}

int CHdbIpcCommandHandler::HandleDbPing(const HdbIpcFrame& requestFrame,
    std::vector<unsigned char>& responseFrame)
{
    std::vector<unsigned char> body;
    int ret;

    if (m_context == NULL || m_context->adapter == NULL)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            HDB_ERR_NOT_CONNECTED,
            "database adapter is not ready",
            responseFrame);
    }
    ret = m_context->adapter->Ping();
    if (ret != HDB_OK)
    {
        return BuildIpcErrorResponse(requestFrame.header.command,
            requestFrame.header.sequence,
            ret,
            m_context->adapter->GetLastError(),
            responseFrame);
    }
    return BuildIpcResponse(requestFrame.header.command,
        requestFrame.header.sequence,
        HDB_OK,
        body,
        responseFrame);
}

int CHdbIpcCommandHandler::HandleQueryExecute(const HdbIpcFrame& requestFrame,
    std::vector<unsigned char>& responseFrame)
{
    CHdbIpcFieldReader reader;
    HdbIpcField field;
    CHdbQueryExecutor executor(m_context == NULL ? NULL : m_context->adapter,
        m_context == NULL ? NULL : &m_context->registry);
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
            // 请求体只接受 QUERY_AST，SERVER 不信任 DLL 侧生成 SQL
            if (field.length > HDB_IPC_MAX_QUERY_AST_BYTES)
            {
                return BuildIpcErrorResponse(requestFrame.header.command,
                    requestFrame.header.sequence,
                    HDB_ERR_BUFFER,
                    "query ast exceeds ipc limit",
                    responseFrame);
            }
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
    HdbHandlerConvertQueryResult(queryResult, executor.GetLastOutputTypes(), ipcResult);
    // schema 和 rows 分两个 TLV 字段，便于后续分页或游标协议扩展
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

int CHdbIpcCommandHandler::BuildIpcResponse(unsigned int command,
    unsigned int sequence,
    int status,
    const std::vector<unsigned char>& body,
    std::vector<unsigned char>& responseFrame)
{
    return HdbIpcBuildResponse(command,
        sequence,
        status,
        HdbHandlerGetVectorBuffer(body),
        (unsigned int)body.size(),
        responseFrame);
}

int CHdbIpcCommandHandler::BuildIpcErrorResponse(unsigned int command,
    unsigned int sequence,
    int status,
    const char* errorText,
    std::vector<unsigned char>& responseFrame)
{
    std::vector<unsigned char> body;
    int ret;

    if (status == HDB_OK)
    {
        // 错误响应不能带成功码，避免调用方把错误文本当成功结果
        status = HDB_ERR_DB_EXEC;
    }
    ret = HdbIpcAppendString(body, HDB_IPC_FIELD_ERROR_TEXT, errorText == NULL ? "" : errorText);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    return BuildIpcResponse(command, sequence, status, body, responseFrame);
}
