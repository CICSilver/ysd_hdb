#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "ysd_hdb_c.h"

#include "../common/HdbIpcProtocol.h"
#include "../common/HdbIpcResultCodec.h"
#include "../common/HdbIpcSocket.h"
#include "../common/HdbQueryAst.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

struct HdbSessionTag
{
    // DLL session 只保存 IPC 连接信息和最近错误，不保存数据库连接
    std::string profileName;
    std::string connInfo;
    std::string ipcHost;
    int ipcPort;
    unsigned int nextSequence;
    std::string lastError;
};

struct HdbQueryTag
{
    // query 句柄只累积逻辑查询 AST，真正 SQL 由 SERVER 生成
    HDB_SESSION session;
    CHdbQueryAst ast;
    std::vector<HDB_SOURCE> sources;
    std::string lastError;
};

struct HdbQuerySourceTag
{
    HDB_QUERY ownerQuery;
    int sourceId;
};

struct HdbResultCell
{
    std::string value;
    int isNull;
};

typedef HdbIpcResultColumn HdbResultColumn;

struct HdbResultTag
{
    // result 句柄缓存一次查询返回的完整结果，游标只在 DLL 内移动
    HDB_SESSION session;
    std::vector<HdbResultColumn> columns;
    std::vector< std::vector<HdbResultCell> > rows;
    int currentRow;
    std::string lastError;
};

class CHdbDllActiveSourceLock
{
public:
    CHdbDllActiveSourceLock()
    {
#ifdef _WIN32
        InitializeCriticalSection(&m_lock);
#else
        pthread_mutex_init(&m_lock, 0);
#endif
    }

    ~CHdbDllActiveSourceLock()
    {
#ifdef _WIN32
        DeleteCriticalSection(&m_lock);
#else
        pthread_mutex_destroy(&m_lock);
#endif
    }

    void Lock()
    {
#ifdef _WIN32
        EnterCriticalSection(&m_lock);
#else
        pthread_mutex_lock(&m_lock);
#endif
    }

    void Unlock()
    {
#ifdef _WIN32
        LeaveCriticalSection(&m_lock);
#else
        pthread_mutex_unlock(&m_lock);
#endif
    }

private:
    CHdbDllActiveSourceLock(const CHdbDllActiveSourceLock&);
    CHdbDllActiveSourceLock& operator=(const CHdbDllActiveSourceLock&);

private:
#ifdef _WIN32
    CRITICAL_SECTION m_lock;
#else
    pthread_mutex_t m_lock;
#endif
};

class CHdbDllActiveSourceGuard
{
public:
    explicit CHdbDllActiveSourceGuard(CHdbDllActiveSourceLock& lock)
        : m_lock(lock)
    {
        m_lock.Lock();
    }

    ~CHdbDllActiveSourceGuard()
    {
        m_lock.Unlock();
    }

private:
    CHdbDllActiveSourceGuard(const CHdbDllActiveSourceGuard&);
    CHdbDllActiveSourceGuard& operator=(const CHdbDllActiveSourceGuard&);

private:
    CHdbDllActiveSourceLock& m_lock;
};

static CHdbDllActiveSourceLock g_hdbActiveSourceLock;
static std::vector<HDB_SOURCE> g_hdbActiveSources;

static void HdbDllSetSessionError(HDB_SESSION session, const char* text)
{
    if (session == NULL)
    {
        return;
    }
    if (text == NULL || text[0] == '\0')
    {
        session->lastError = "unknown hdb dll error";
    }
    else
    {
        session->lastError = text;
    }
}

static void HdbDllSetQueryError(HDB_QUERY query, const char* text)
{
    if (query == NULL)
    {
        return;
    }
    if (text == NULL || text[0] == '\0')
    {
        query->lastError = "unknown hdb query error";
    }
    else
    {
        query->lastError = text;
    }
    HdbDllSetSessionError(query->session, query->lastError.c_str());
}

static void HdbDllSetResultError(HDB_RESULT result, const char* text)
{
    if (result == NULL)
    {
        return;
    }
    if (text == NULL || text[0] == '\0')
    {
        result->lastError = "unknown hdb result error";
    }
    else
    {
        result->lastError = text;
    }
    HdbDllSetSessionError(result->session, result->lastError.c_str());
}

static void HdbDllTrySetSessionError(HDB_SESSION session, const char* text)
{
    try
    {
        HdbDllSetSessionError(session, text);
    }
    catch (...)
    {
    }
}

static void HdbDllTrySetQueryError(HDB_QUERY query, const char* text)
{
    try
    {
        HdbDllSetQueryError(query, text);
    }
    catch (...)
    {
    }
}

static void HdbDllTrySetResultError(HDB_RESULT result, const char* text)
{
    try
    {
        HdbDllSetResultError(result, text);
    }
    catch (...)
    {
    }
}

static int HdbDllCopyText(const std::string& text, char* buffer, int bufferSize, int* requiredSize)
{
    int required;

    // C 接口返回字符串时同时告知所需长度，调用方可以二次分配
    required = (int)text.size() + 1;
    if (requiredSize != NULL)
    {
        *requiredSize = required;
    }
    if (buffer == NULL || bufferSize <= 0)
    {
        return HDB_ERR_BUFFER;
    }
    if (bufferSize < required)
    {
        if (bufferSize > 0)
        {
            buffer[0] = '\0';
        }
        return HDB_ERR_BUFFER;
    }
    memcpy(buffer, text.c_str(), required);
    return HDB_OK;
}

static int HdbDllFindColumn(HDB_RESULT result, const char* outputName)
{
    int i;

    if (result == NULL || outputName == NULL)
    {
        return -1;
    }
    for (i = 0; i < (int)result->columns.size(); ++i)
    {
        if (result->columns[i].name == outputName)
        {
            return i;
        }
    }
    return -1;
}

static int HdbDllFindActiveSourceIndexNoLock(HDB_SOURCE source)
{
    int i;

    if (source == NULL)
    {
        return -1;
    }
    for (i = 0; i < (int)g_hdbActiveSources.size(); ++i)
    {
        if (g_hdbActiveSources[i] == source)
        {
            return i;
        }
    }
    return -1;
}

static void HdbDllUnregisterSource(HDB_SOURCE source)
{
    int i;

    if (source == NULL)
    {
        return;
    }
    {
        CHdbDllActiveSourceGuard guard(g_hdbActiveSourceLock);

        i = HdbDllFindActiveSourceIndexNoLock(source);
        if (i >= 0)
        {
            g_hdbActiveSources.erase(g_hdbActiveSources.begin() + i);
        }
    }
}

static int HdbDllRegisterSource(HDB_SOURCE source)
{
    if (source == NULL)
    {
        return HDB_ERR_PARAM;
    }
    {
        CHdbDllActiveSourceGuard guard(g_hdbActiveSourceLock);

        g_hdbActiveSources.push_back(source);
    }
    return HDB_OK;
}

static int HdbDllCreateQuerySource(HDB_QUERY query, int sourceId, HDB_SOURCE* outSource)
{
    HDB_SOURCE source;

    if (query == NULL || outSource == NULL || sourceId < 0)
    {
        return HDB_ERR_PARAM;
    }
    *outSource = NULL;
    source = new(std::nothrow) HdbQuerySourceTag();
    if (source == NULL)
    {
        HdbDllSetQueryError(query, "allocate query source failed");
        return HDB_ERR_BUFFER;
    }
    try
    {
        source->ownerQuery = query;
        source->sourceId = sourceId;
        HdbDllRegisterSource(source);
        query->sources.push_back(source);
    }
    catch (...)
    {
        HdbDllUnregisterSource(source);
        delete source;
        HdbDllSetQueryError(query, "allocate query source failed");
        return HDB_ERR_BUFFER;
    }
    *outSource = source;
    return HDB_OK;
}

static int HdbDllValidateQuerySource(HDB_QUERY query, HDB_SOURCE source, int* outSourceId)
{
    int sourceId;

    if (outSourceId != NULL)
    {
        *outSourceId = -1;
    }
    if (query == NULL || source == NULL)
    {
        return HDB_ERR_PARAM;
    }
    sourceId = -1;
    {
        CHdbDllActiveSourceGuard guard(g_hdbActiveSourceLock);

        if (HdbDllFindActiveSourceIndexNoLock(source) < 0)
        {
            HdbDllSetQueryError(query, "query source is not active");
            return HDB_ERR_PARAM;
        }
        if (source->ownerQuery != query)
        {
            HdbDllSetQueryError(query, "query source belongs to another query");
            return HDB_ERR_PARAM;
        }
        sourceId = source->sourceId;
    }
    if (sourceId < 0)
    {
        HdbDllSetQueryError(query, "query source id is invalid");
        return HDB_ERR_PARAM;
    }
    if (query->ast.FindSourceIndex(sourceId) < 0)
    {
        HdbDllSetQueryError(query, "query source id is invalid");
        return HDB_ERR_PARAM;
    }
    if (outSourceId != NULL)
    {
        *outSourceId = sourceId;
    }
    return HDB_OK;
}

static void HdbDllFreeQuerySources(HDB_QUERY query)
{
    int i;

    if (query == NULL)
    {
        return;
    }
    for (i = 0; i < (int)query->sources.size(); ++i)
    {
        HDB_SOURCE source = query->sources[i];
        HdbDllUnregisterSource(source);
        if (source != NULL)
        {
            source->ownerQuery = NULL;
            source->sourceId = -1;
            delete source;
        }
    }
    query->sources.clear();
}

static int HdbDllGetCurrentCell(HDB_RESULT result,
    const char* outputName,
    const HdbResultCell** outCell,
    int* outFieldType)
{
    int column;

    if (result == NULL || outputName == NULL || outCell == NULL || outFieldType == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outCell = NULL;
    *outFieldType = HDB_FT_CHAR_ARRAY;
    // 所有取值接口先走当前行和列名定位，再按字段类型转换
    if (result->currentRow < 0 || result->currentRow >= (int)result->rows.size())
    {
        HdbDllSetResultError(result, "result cursor is not on row");
        return HDB_ERR_NO_RECORD;
    }
    column = HdbDllFindColumn(result, outputName);
    if (column < 0 || column >= (int)result->rows[result->currentRow].size())
    {
        HdbDllSetResultError(result, "result field is not found");
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    *outCell = &result->rows[result->currentRow][column];
    *outFieldType = result->columns[column].fieldType;
    return HDB_OK;
}

static int HdbDllIsInt32Type(int fieldType)
{
    return fieldType == HDB_FT_INT32 || fieldType == HDB_FT_SMALLINT;
}

static int HdbDllIsInt64Type(int fieldType)
{
    return fieldType == HDB_FT_INT64 || fieldType == HDB_FT_TIMESTAMP_MS;
}

static int HdbDllIsDoubleType(int fieldType)
{
    return fieldType == HDB_FT_DOUBLE;
}

static int HdbDllIsStringType(int fieldType)
{
    return fieldType == HDB_FT_CHAR_ARRAY;
}

static int HdbDllParseInt32Strict(const char* text, int* value)
{
    char* endPtr;
    long parsed;

    if (text == NULL || text[0] == '\0' || value == NULL)
    {
        return HDB_ERR_TYPE_MISMATCH;
    }
    errno = 0;
    endPtr = NULL;
    parsed = strtol(text, &endPtr, 10);
    if (errno != 0 || endPtr == NULL || *endPtr != '\0' || parsed < INT_MIN || parsed > INT_MAX)
    {
        return HDB_ERR_TYPE_MISMATCH;
    }
    *value = (int)parsed;
    return HDB_OK;
}

static int HdbDllParseInt64Strict(const char* text, HdbInt64* value)
{
    char* endPtr;

    if (text == NULL || text[0] == '\0' || value == NULL)
    {
        return HDB_ERR_TYPE_MISMATCH;
    }
    errno = 0;
    endPtr = NULL;
#ifdef _WIN32
    *value = (HdbInt64)_strtoi64(text, &endPtr, 10);
#else
    *value = (HdbInt64)strtoll(text, &endPtr, 10);
#endif
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        return HDB_ERR_TYPE_MISMATCH;
    }
    return HDB_OK;
}

static int HdbDllParseDoubleStrict(const char* text, double* value)
{
    char* endPtr;

    if (text == NULL || text[0] == '\0' || value == NULL)
    {
        return HDB_ERR_TYPE_MISMATCH;
    }
    errno = 0;
    endPtr = NULL;
    *value = strtod(text, &endPtr);
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        return HDB_ERR_TYPE_MISMATCH;
    }
    return HDB_OK;
}

static const char* HdbDllReadIpcHost()
{
    const char* envHost;

    envHost = getenv("HDB_IPC_HOST");
    if (envHost != NULL && envHost[0] != '\0')
    {
        return envHost;
    }
    return HDB_IPC_DEFAULT_HOST;
}

static int HdbDllReadIpcPort()
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

static unsigned int HdbDllNextSequence(HDB_SESSION session)
{
    unsigned int current;

    if (session == NULL)
    {
        return 0;
    }
    current = session->nextSequence++;
    if (session->nextSequence == 0)
    {
        // sequence 用来匹配请求和响应，0 保留不用
        session->nextSequence = 1;
    }
    return current;
}

static int HdbDllMapIpcError(int ipcError)
{
    if (ipcError == HDB_IPC_ERR_BODY_SIZE ||
        ipcError == HDB_IPC_ERR_BUFFER ||
        ipcError == HDB_IPC_ERR_INCOMPLETE)
    {
        return HDB_ERR_BUFFER;
    }
    return HDB_ERR_DB_EXEC;
}

static int HdbDllMapResponseStatus(int status)
{
    if (status <= HDB_ERR_PARAM && status >= HDB_ERR_INTERNAL)
    {
        return status;
    }
    if (status <= HDB_IPC_ERR_PARAM)
    {
        return HDB_ERR_BUFFER;
    }
    if (status == HDB_OK)
    {
        return HDB_OK;
    }
    return HDB_ERR_DB_EXEC;
}

static int HdbDllReadErrorText(const HdbIpcFrame& frame, std::string& errorText)
{
    CHdbIpcFieldReader reader;
    HdbIpcField field;
    int hasField;
    int ret;

    errorText.clear();
    ret = reader.Reset(frame.body, frame.bodyLength);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    while (1)
    {
        ret = reader.Next(field, &hasField);
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
        if (hasField == 0)
        {
            return HDB_IPC_OK;
        }
        if (field.type == HDB_IPC_FIELD_ERROR_TEXT)
        {
            return HdbIpcReadString(field, errorText);
        }
    }
}

static int HdbDllRequest(HDB_SESSION session,
    unsigned int command,
    const std::vector<unsigned char>& body,
    HdbIpcFrame& responseFrame,
    std::vector<unsigned char>& responseBytes)
{
    CHdbIpcTcpClient client;
    std::vector<unsigned char> requestBytes;
    std::string errorText;
    unsigned int sequence;
    int ret;

    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    sequence = HdbDllNextSequence(session);
    // DLL 只把命令和 TLV body 发给 SERVER，不携带 SQL
    ret = HdbIpcBuildRequest(command,
        sequence,
        body.empty() ? NULL : &body[0],
        (unsigned int)body.size(),
        requestBytes);
    if (ret != HDB_IPC_OK)
    {
        HdbDllSetSessionError(session, "build ipc request failed");
        return HdbDllMapIpcError(ret);
    }
    ret = client.Request(session->ipcHost.c_str(), session->ipcPort, requestBytes, responseBytes);
    if (ret != HDB_IPC_OK)
    {
        HdbDllSetSessionError(session, client.GetLastError());
        return HDB_ERR_NOT_CONNECTED;
    }
    ret = HdbIpcParseFrame(responseBytes.empty() ? NULL : &responseBytes[0],
        (unsigned int)responseBytes.size(),
        responseFrame);
    if (ret != HDB_IPC_OK)
    {
        HdbDllSetSessionError(session, "parse ipc response failed");
        return HdbDllMapIpcError(ret);
    }
    if ((responseFrame.header.flags & HDB_IPC_FLAG_RESPONSE) == 0 ||
        responseFrame.header.command != command ||
        responseFrame.header.sequence != sequence)
    {
        // 响应回到同一个命令和 sequence，避免短连接异常串帧
        HdbDllSetSessionError(session, "invalid ipc response");
        return HDB_ERR_BUFFER;
    }
    if (responseFrame.header.status != HDB_OK)
    {
        ret = HdbDllReadErrorText(responseFrame, errorText);
        if (ret == HDB_IPC_OK && !errorText.empty())
        {
            HdbDllSetSessionError(session, errorText.c_str());
        }
        else
        {
            HdbDllSetSessionError(session, "server returned error");
        }
        return HdbDllMapResponseStatus(responseFrame.header.status);
    }
    return HDB_OK;
}

static int HdbDllFillResult(HDB_SESSION session, const HdbIpcResultSet& ipcResult, HDB_RESULT* outResult)
{
    HDB_RESULT result;
    int rowIndex;

    if (outResult == NULL)
    {
        return HDB_ERR_PARAM;
    }
    result = new(std::nothrow) HdbResultTag();
    if (result == NULL)
    {
        HdbDllSetSessionError(session, "allocate result failed");
        return HDB_ERR_BUFFER;
    }
    // IPC result 拷贝到 DLL 自有句柄，responseBytes 释放后仍可遍历
    try
    {
        result->session = session;
        result->columns = ipcResult.columns;
        result->currentRow = -1;
        for (rowIndex = 0; rowIndex < (int)ipcResult.rows.size(); ++rowIndex)
        {
            std::vector<HdbResultCell> row;
            int fieldIndex;

            for (fieldIndex = 0; fieldIndex < (int)ipcResult.rows[rowIndex].size(); ++fieldIndex)
            {
                HdbResultCell cell;

                cell.value = ipcResult.rows[rowIndex][fieldIndex].value;
                cell.isNull = ipcResult.rows[rowIndex][fieldIndex].isNull;
                row.push_back(cell);
            }
            result->rows.push_back(row);
        }
    }
    catch (...)
    {
        delete result;
        throw;
    }
    *outResult = result;
    return HDB_OK;
}

static int HdbOpenImpl(const char* profileName, HDB_SESSION* outSession)
{
    HDB_SESSION session;

    if (outSession == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outSession = NULL;
    session = new(std::nothrow) HdbSessionTag();
    if (session == NULL)
    {
        return HDB_ERR_BUFFER;
    }
    try
    {
        if (profileName != NULL)
        {
            session->profileName = profileName;
        }
        session->ipcHost = HdbDllReadIpcHost();
        session->ipcPort = HdbDllReadIpcPort();
        // 当前打开 session 只建立 DLL 侧状态，真实 DB 连接由 SERVER 进程管理
        session->nextSequence = 1;
    }
    catch (...)
    {
        delete session;
        throw;
    }
    *outSession = session;
    return HDB_OK;
}

static int HdbOpenByConnInfoImpl(const char* connInfo, HDB_SESSION* outSession)
{
    if (outSession == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outSession = NULL;
    (void)connInfo;
    return HDB_ERR_NOT_IMPLEMENTED;
}

static int HdbCloseImpl(HDB_SESSION session)
{
    if (session != NULL)
    {
        delete session;
    }
    return HDB_OK;
}

static int HdbPingImpl(HDB_SESSION session)
{
    HdbIpcFrame responseFrame;
    std::vector<unsigned char> body;
    std::vector<unsigned char> responseBytes;

    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    return HdbDllRequest(session, HDB_IPC_CMD_DB_PING, body, responseFrame, responseBytes);
}

static int HdbGetLastErrorImpl(HDB_SESSION session, char* buffer, int bufferSize, int* requiredSize)
{
    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    return HdbDllCopyText(session->lastError, buffer, bufferSize, requiredSize);
}

static int HdbInsertRowImpl(HDB_SESSION session, const char* datasetName, const void* row, int rowSize)
{
    (void)datasetName;
    (void)row;
    (void)rowSize;
    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    HdbDllSetSessionError(session, "dataset insert is not implemented");
    return HDB_ERR_NOT_IMPLEMENTED;
}

static int HdbBatchInsertRowsImpl(HDB_SESSION session,
    const char* datasetName,
    const void* rows,
    int rowSize,
    int rowCount)
{
    (void)datasetName;
    (void)rows;
    (void)rowSize;
    (void)rowCount;
    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    HdbDllSetSessionError(session, "batch insert is not implemented");
    return HDB_ERR_NOT_IMPLEMENTED;
}

static int HdbQueryCreateImpl(HDB_SESSION session, HDB_QUERY* outQuery)
{
    HDB_QUERY query;

    if (session == NULL || outQuery == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outQuery = NULL;
    query = new(std::nothrow) HdbQueryTag();
    if (query == NULL)
    {
        HdbDllSetSessionError(session, "allocate query failed");
        return HDB_ERR_BUFFER;
    }
    try
    {
        query->session = session;
    }
    catch (...)
    {
        delete query;
        throw;
    }
    *outQuery = query;
    return HDB_OK;
}

static int HdbQueryFreeImpl(HDB_QUERY query)
{
    if (query != NULL)
    {
        HdbDllFreeQuerySources(query);
        delete query;
    }
    return HDB_OK;
}

static int HdbQueryFromImpl(HDB_QUERY query, const char* datasetName, HDB_SOURCE* outRootSource)
{
    int sourceId;
    int ret;

    if (outRootSource != NULL)
    {
        *outRootSource = NULL;
    }
    if (query == NULL || datasetName == NULL || datasetName[0] == '\0' || outRootSource == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.HasRootSource())
    {
        HdbDllSetQueryError(query, "query root source already exists");
        return HDB_ERR_QUERY_RANGE;
    }
    sourceId = -1;
    if (query->ast.AddRootSource(datasetName, &sourceId) != 0)
    {
        HdbDllSetQueryError(query, "invalid root dataset name");
        return HDB_ERR_PARAM;
    }
    ret = HdbDllCreateQuerySource(query, sourceId, outRootSource);
    if (ret != HDB_OK)
    {
        query->ast.Clear();
    }
    return ret;
}

static int HdbQueryJoinImpl(HDB_QUERY query,
    HDB_SOURCE fromSource,
    const char* associationName,
    int joinType,
    HDB_SOURCE* outTargetSource)
{
    int parentSourceId;
    int targetSourceId;
    int ret;

    if (outTargetSource != NULL)
    {
        *outTargetSource = NULL;
    }
    if (query == NULL || fromSource == NULL || associationName == NULL || associationName[0] == '\0' ||
        outTargetSource == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, fromSource, &parentSourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddJoinSource(parentSourceId, associationName, joinType, &targetSourceId) != 0)
    {
        HdbDllSetQueryError(query, "invalid query join source");
        return HDB_ERR_QUERY_RANGE;
    }
    ret = HdbDllCreateQuerySource(query, targetSourceId, outTargetSource);
    if (ret != HDB_OK && !query->ast.sources.empty())
    {
        query->ast.sources.pop_back();
    }
    return ret;
}

static int HdbQueryTimeRangeImpl(HDB_QUERY query, HdbInt64 beginMs, HdbInt64 endMs)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.SetTimeRange((HdbQueryInt64)beginMs, (HdbQueryInt64)endMs) != 0)
    {
        HdbDllSetQueryError(query, "invalid query time range");
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

static int HdbQuerySelectImpl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* outputName)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddSelect(sourceId, fieldName, outputName) != 0)
    {
        HdbDllSetQueryError(query, "invalid select field");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryWhereInt32Impl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, int value)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddWhereInt32(sourceId, fieldName, op, value) != 0)
    {
        HdbDllSetQueryError(query, "invalid int32 where condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryWhereInt64Impl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, HdbInt64 value)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddWhereInt64(sourceId, fieldName, op, (HdbQueryInt64)value) != 0)
    {
        HdbDllSetQueryError(query, "invalid int64 where condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryWhereDoubleImpl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, double value)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddWhereDouble(sourceId, fieldName, op, value) != 0)
    {
        HdbDllSetQueryError(query, "invalid double where condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryWhereStringEqImpl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* value)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddWhereString(sourceId, fieldName, HDB_OP_EQ, value) != 0)
    {
        HdbDllSetQueryError(query, "invalid string equal condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryWhereStringLikeImpl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* pattern)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddWhereString(sourceId, fieldName, HDB_OP_LIKE, pattern) != 0)
    {
        HdbDllSetQueryError(query, "invalid string like condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryOrderByImpl(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int orderType)
{
    int sourceId;
    int ret;

    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (orderType != HDB_ORDER_ASC && orderType != HDB_ORDER_DESC)
    {
        HdbDllSetQueryError(query, "invalid order type");
        return HDB_ERR_QUERY_RANGE;
    }
    ret = HdbDllValidateQuerySource(query, source, &sourceId);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (query->ast.AddOrder(sourceId, fieldName, orderType) != 0)
    {
        HdbDllSetQueryError(query, "invalid order field");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

static int HdbQueryLimitImpl(HDB_QUERY query, int limit, int offset)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.SetLimit(limit, offset) != 0)
    {
        HdbDllSetQueryError(query, "invalid query limit");
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

static int HdbQueryExecuteImpl(HDB_QUERY query, HDB_RESULT* outResult)
{
    HdbIpcFrame responseFrame;
    CHdbIpcFieldReader reader;
    HdbIpcField field;
    HdbIpcResultSet ipcResult;
    std::vector<unsigned char> body;
    std::vector<unsigned char> responseBytes;
    std::string astText;
    int hasField;
    int hasSchema;
    int hasRows;
    int ret;

    if (query == NULL || outResult == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outResult = NULL;
    // 执行时把 AST 序列化成文本字段，SERVER 会重新解析和校验
    ret = query->ast.Serialize(astText);
    if (ret != HDB_OK)
    {
        HdbDllSetQueryError(query, "serialize query ast failed");
        return ret;
    }
    if (astText.size() > HDB_IPC_MAX_QUERY_AST_BYTES)
    {
        HdbDllSetQueryError(query, "query ast exceeds ipc limit");
        return HDB_ERR_BUFFER;
    }
    ret = HdbIpcAppendString(body, HDB_IPC_FIELD_QUERY_AST, astText.c_str());
    if (ret != HDB_IPC_OK)
    {
        HdbDllSetQueryError(query, "build query ipc body failed");
        return HdbDllMapIpcError(ret);
    }
    ret = HdbDllRequest(query->session,
        HDB_IPC_CMD_QUERY_EXECUTE,
        body,
        responseFrame,
        responseBytes);
    if (ret != HDB_OK)
    {
        HdbDllSetQueryError(query, query->session->lastError.c_str());
        return ret;
    }

    hasSchema = 0;
    hasRows = 0;
    // 当前响应按 schema 再 rows 发送，列定义用于行列数校验
    ret = reader.Reset(responseFrame.body, responseFrame.bodyLength);
    if (ret != HDB_IPC_OK)
    {
        HdbDllSetQueryError(query, "invalid query response body");
        return HDB_ERR_BUFFER;
    }
    while (1)
    {
        ret = reader.Next(field, &hasField);
        if (ret != HDB_IPC_OK)
        {
            HdbDllSetQueryError(query, "invalid query response field");
            return HDB_ERR_BUFFER;
        }
        if (hasField == 0)
        {
            break;
        }
        if (field.type == HDB_IPC_FIELD_RESULT_SCHEMA)
        {
            ret = HdbIpcDecodeResultSchema(field.data, field.length, ipcResult);
            if (ret != HDB_IPC_OK)
            {
                HdbDllSetQueryError(query, "decode query result schema failed");
                return HDB_ERR_BUFFER;
            }
            hasSchema = 1;
        }
        else if (field.type == HDB_IPC_FIELD_RESULT_ROWS)
        {
            ret = HdbIpcDecodeResultRows(field.data, field.length, ipcResult);
            if (ret != HDB_IPC_OK)
            {
                HdbDllSetQueryError(query, "decode query result rows failed");
                return HDB_ERR_BUFFER;
            }
            hasRows = 1;
        }
    }
    if (hasSchema == 0 || hasRows == 0)
    {
        HdbDllSetQueryError(query, "query response missing result data");
        return HDB_ERR_BUFFER;
    }
    return HdbDllFillResult(query->session, ipcResult, outResult);
}

static int HdbResultFreeImpl(HDB_RESULT result)
{
    if (result != NULL)
    {
        delete result;
    }
    return HDB_OK;
}

static int HdbResultNextImpl(HDB_RESULT result, int* hasRow)
{
    if (result == NULL || hasRow == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *hasRow = 0;
    if (result->currentRow + 1 < (int)result->rows.size())
    {
        ++result->currentRow;
        *hasRow = 1;
    }
    return HDB_OK;
}

static int HdbResultIsNullImpl(HDB_RESULT result, const char* outputName, int* isNull)
{
    const HdbResultCell* cell;
    int fieldType;
    int ret;

    if (result == NULL || outputName == NULL || isNull == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllGetCurrentCell(result, outputName, &cell, &fieldType);
    if (ret != HDB_OK)
    {
        return ret;
    }
    *isNull = cell->isNull != 0 ? 1 : 0;
    return HDB_OK;
}

static int HdbResultGetInt32Impl(HDB_RESULT result, const char* outputName, int* value)
{
    const HdbResultCell* cell;
    int fieldType;
    int ret;

    if (value == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllGetCurrentCell(result, outputName, &cell, &fieldType);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (cell->isNull)
    {
        HdbDllSetResultError(result, "result value is null");
        return HDB_ERR_NULL_VALUE;
    }
    if (!HdbDllIsInt32Type(fieldType))
    {
        HdbDllSetResultError(result, "result field type is not int32");
        return HDB_ERR_TYPE_MISMATCH;
    }
    ret = HdbDllParseInt32Strict(cell->value.c_str(), value);
    if (ret != HDB_OK)
    {
        HdbDllSetResultError(result, "result int32 parse failed");
    }
    return ret;
}

static int HdbResultGetInt64Impl(HDB_RESULT result, const char* outputName, HdbInt64* value)
{
    const HdbResultCell* cell;
    int fieldType;
    int ret;

    if (value == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllGetCurrentCell(result, outputName, &cell, &fieldType);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (cell->isNull)
    {
        HdbDllSetResultError(result, "result value is null");
        return HDB_ERR_NULL_VALUE;
    }
    if (!HdbDllIsInt64Type(fieldType))
    {
        HdbDllSetResultError(result, "result field type is not int64");
        return HDB_ERR_TYPE_MISMATCH;
    }
    ret = HdbDllParseInt64Strict(cell->value.c_str(), value);
    if (ret != HDB_OK)
    {
        HdbDllSetResultError(result, "result int64 parse failed");
    }
    return ret;
}

static int HdbResultGetDoubleImpl(HDB_RESULT result, const char* outputName, double* value)
{
    const HdbResultCell* cell;
    int fieldType;
    int ret;

    if (value == NULL)
    {
        return HDB_ERR_PARAM;
    }
    ret = HdbDllGetCurrentCell(result, outputName, &cell, &fieldType);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (cell->isNull)
    {
        HdbDllSetResultError(result, "result value is null");
        return HDB_ERR_NULL_VALUE;
    }
    if (!HdbDllIsDoubleType(fieldType))
    {
        HdbDllSetResultError(result, "result field type is not double");
        return HDB_ERR_TYPE_MISMATCH;
    }
    ret = HdbDllParseDoubleStrict(cell->value.c_str(), value);
    if (ret != HDB_OK)
    {
        HdbDllSetResultError(result, "result double parse failed");
    }
    return ret;
}

static int HdbResultGetStringImpl(HDB_RESULT result,
    const char* outputName,
    char* buffer,
    int bufferSize,
    int* requiredSize)
{
    const HdbResultCell* cell;
    int fieldType;
    int ret;

    ret = HdbDllGetCurrentCell(result, outputName, &cell, &fieldType);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (cell->isNull)
    {
        if (requiredSize != NULL)
        {
            *requiredSize = 0;
        }
        HdbDllSetResultError(result, "result value is null");
        return HDB_ERR_NULL_VALUE;
    }
    if (!HdbDllIsStringType(fieldType))
    {
        HdbDllSetResultError(result, "result field type is not string");
        return HDB_ERR_TYPE_MISMATCH;
    }
    return HdbDllCopyText(cell->value, buffer, bufferSize, requiredSize);
}

static int HdbDllReturnSessionBadAlloc(HDB_SESSION session)
{
    HdbDllTrySetSessionError(session, "memory allocation failed");
    return HDB_ERR_BUFFER;
}

static int HdbDllReturnSessionException(HDB_SESSION session)
{
    HdbDllTrySetSessionError(session, "unexpected dll exception");
    return HDB_ERR_INTERNAL;
}

static int HdbDllReturnQueryBadAlloc(HDB_QUERY query)
{
    HdbDllTrySetQueryError(query, "memory allocation failed");
    return HDB_ERR_BUFFER;
}

static int HdbDllReturnQueryException(HDB_QUERY query)
{
    HdbDllTrySetQueryError(query, "unexpected dll exception");
    return HDB_ERR_INTERNAL;
}

static int HdbDllReturnResultBadAlloc(HDB_RESULT result)
{
    HdbDllTrySetResultError(result, "memory allocation failed");
    return HDB_ERR_BUFFER;
}

static int HdbDllReturnResultException(HDB_RESULT result)
{
    HdbDllTrySetResultError(result, "unexpected dll exception");
    return HDB_ERR_INTERNAL;
}

// 导出函数不让 C++ 异常穿过 DLL ABI 边界
int HDB_CALL HdbOpen(const char* profileName, HDB_SESSION* outSession)
{
    if (outSession != NULL)
    {
        *outSession = NULL;
    }
    try
    {
        return HdbOpenImpl(profileName, outSession);
    }
    catch (const std::bad_alloc&)
    {
        if (outSession != NULL)
        {
            *outSession = NULL;
        }
        return HDB_ERR_BUFFER;
    }
    catch (...)
    {
        if (outSession != NULL)
        {
            *outSession = NULL;
        }
        return HDB_ERR_INTERNAL;
    }
}

int HDB_CALL HdbOpenByConnInfo(const char* connInfo, HDB_SESSION* outSession)
{
    if (outSession != NULL)
    {
        *outSession = NULL;
    }
    try
    {
        return HdbOpenByConnInfoImpl(connInfo, outSession);
    }
    catch (const std::bad_alloc&)
    {
        if (outSession != NULL)
        {
            *outSession = NULL;
        }
        return HDB_ERR_BUFFER;
    }
    catch (...)
    {
        if (outSession != NULL)
        {
            *outSession = NULL;
        }
        return HDB_ERR_INTERNAL;
    }
}

int HDB_CALL HdbClose(HDB_SESSION session)
{
    try
    {
        return HdbCloseImpl(session);
    }
    catch (const std::bad_alloc&)
    {
        return HDB_ERR_BUFFER;
    }
    catch (...)
    {
        return HDB_ERR_INTERNAL;
    }
}

int HDB_CALL HdbPing(HDB_SESSION session)
{
    try
    {
        return HdbPingImpl(session);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnSessionBadAlloc(session);
    }
    catch (...)
    {
        return HdbDllReturnSessionException(session);
    }
}

int HDB_CALL HdbGetLastError(HDB_SESSION session, char* buffer, int bufferSize, int* requiredSize)
{
    try
    {
        return HdbGetLastErrorImpl(session, buffer, bufferSize, requiredSize);
    }
    catch (const std::bad_alloc&)
    {
        if (buffer != NULL && bufferSize > 0)
        {
            buffer[0] = '\0';
        }
        if (requiredSize != NULL)
        {
            *requiredSize = 0;
        }
        return HDB_ERR_BUFFER;
    }
    catch (...)
    {
        if (buffer != NULL && bufferSize > 0)
        {
            buffer[0] = '\0';
        }
        if (requiredSize != NULL)
        {
            *requiredSize = 0;
        }
        return HDB_ERR_INTERNAL;
    }
}

int HDB_CALL HdbInsertRow(HDB_SESSION session, const char* datasetName, const void* row, int rowSize)
{
    try
    {
        return HdbInsertRowImpl(session, datasetName, row, rowSize);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnSessionBadAlloc(session);
    }
    catch (...)
    {
        return HdbDllReturnSessionException(session);
    }
}

int HDB_CALL HdbBatchInsertRows(HDB_SESSION session,
    const char* datasetName,
    const void* rows,
    int rowSize,
    int rowCount)
{
    try
    {
        return HdbBatchInsertRowsImpl(session, datasetName, rows, rowSize, rowCount);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnSessionBadAlloc(session);
    }
    catch (...)
    {
        return HdbDllReturnSessionException(session);
    }
}

int HDB_CALL HdbQueryCreate(HDB_SESSION session, HDB_QUERY* outQuery)
{
    if (outQuery != NULL)
    {
        *outQuery = NULL;
    }
    try
    {
        return HdbQueryCreateImpl(session, outQuery);
    }
    catch (const std::bad_alloc&)
    {
        if (outQuery != NULL)
        {
            *outQuery = NULL;
        }
        return HdbDllReturnSessionBadAlloc(session);
    }
    catch (...)
    {
        if (outQuery != NULL)
        {
            *outQuery = NULL;
        }
        return HdbDllReturnSessionException(session);
    }
}

int HDB_CALL HdbQueryFree(HDB_QUERY query)
{
    try
    {
        return HdbQueryFreeImpl(query);
    }
    catch (const std::bad_alloc&)
    {
        return HDB_ERR_BUFFER;
    }
    catch (...)
    {
        return HDB_ERR_INTERNAL;
    }
}

int HDB_CALL HdbQueryFrom(HDB_QUERY query, const char* datasetName, HDB_SOURCE* outRootSource)
{
    if (outRootSource != NULL)
    {
        *outRootSource = NULL;
    }
    try
    {
        return HdbQueryFromImpl(query, datasetName, outRootSource);
    }
    catch (const std::bad_alloc&)
    {
        if (outRootSource != NULL)
        {
            *outRootSource = NULL;
        }
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        if (outRootSource != NULL)
        {
            *outRootSource = NULL;
        }
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryJoin(HDB_QUERY query,
    HDB_SOURCE fromSource,
    const char* associationName,
    int joinType,
    HDB_SOURCE* outTargetSource)
{
    if (outTargetSource != NULL)
    {
        *outTargetSource = NULL;
    }
    try
    {
        return HdbQueryJoinImpl(query, fromSource, associationName, joinType, outTargetSource);
    }
    catch (const std::bad_alloc&)
    {
        if (outTargetSource != NULL)
        {
            *outTargetSource = NULL;
        }
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        if (outTargetSource != NULL)
        {
            *outTargetSource = NULL;
        }
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryTimeRange(HDB_QUERY query, HdbInt64 beginMs, HdbInt64 endMs)
{
    try
    {
        return HdbQueryTimeRangeImpl(query, beginMs, endMs);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQuerySelect(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* outputName)
{
    try
    {
        return HdbQuerySelectImpl(query, source, fieldName, outputName);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryWhereInt32(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, int value)
{
    try
    {
        return HdbQueryWhereInt32Impl(query, source, fieldName, op, value);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryWhereInt64(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, HdbInt64 value)
{
    try
    {
        return HdbQueryWhereInt64Impl(query, source, fieldName, op, value);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryWhereDouble(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, double value)
{
    try
    {
        return HdbQueryWhereDoubleImpl(query, source, fieldName, op, value);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryWhereStringEq(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* value)
{
    try
    {
        return HdbQueryWhereStringEqImpl(query, source, fieldName, value);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryWhereStringLike(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* pattern)
{
    try
    {
        return HdbQueryWhereStringLikeImpl(query, source, fieldName, pattern);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryOrderBy(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int orderType)
{
    try
    {
        return HdbQueryOrderByImpl(query, source, fieldName, orderType);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryLimit(HDB_QUERY query, int limit, int offset)
{
    try
    {
        return HdbQueryLimitImpl(query, limit, offset);
    }
    catch (const std::bad_alloc&)
    {
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbQueryExecute(HDB_QUERY query, HDB_RESULT* outResult)
{
    if (outResult != NULL)
    {
        *outResult = NULL;
    }
    try
    {
        return HdbQueryExecuteImpl(query, outResult);
    }
    catch (const std::bad_alloc&)
    {
        if (outResult != NULL)
        {
            *outResult = NULL;
        }
        return HdbDllReturnQueryBadAlloc(query);
    }
    catch (...)
    {
        if (outResult != NULL)
        {
            *outResult = NULL;
        }
        return HdbDllReturnQueryException(query);
    }
}

int HDB_CALL HdbResultFree(HDB_RESULT result)
{
    try
    {
        return HdbResultFreeImpl(result);
    }
    catch (const std::bad_alloc&)
    {
        return HDB_ERR_BUFFER;
    }
    catch (...)
    {
        return HDB_ERR_INTERNAL;
    }
}

int HDB_CALL HdbResultNext(HDB_RESULT result, int* hasRow)
{
    if (hasRow != NULL)
    {
        *hasRow = 0;
    }
    try
    {
        return HdbResultNextImpl(result, hasRow);
    }
    catch (const std::bad_alloc&)
    {
        if (hasRow != NULL)
        {
            *hasRow = 0;
        }
        return HdbDllReturnResultBadAlloc(result);
    }
    catch (...)
    {
        if (hasRow != NULL)
        {
            *hasRow = 0;
        }
        return HdbDllReturnResultException(result);
    }
}

int HDB_CALL HdbResultIsNull(HDB_RESULT result, const char* outputName, int* isNull)
{
    if (isNull != NULL)
    {
        *isNull = 0;
    }
    try
    {
        return HdbResultIsNullImpl(result, outputName, isNull);
    }
    catch (const std::bad_alloc&)
    {
        if (isNull != NULL)
        {
            *isNull = 0;
        }
        return HdbDllReturnResultBadAlloc(result);
    }
    catch (...)
    {
        if (isNull != NULL)
        {
            *isNull = 0;
        }
        return HdbDllReturnResultException(result);
    }
}

int HDB_CALL HdbResultGetInt32(HDB_RESULT result, const char* outputName, int* value)
{
    if (value != NULL)
    {
        *value = 0;
    }
    try
    {
        return HdbResultGetInt32Impl(result, outputName, value);
    }
    catch (const std::bad_alloc&)
    {
        if (value != NULL)
        {
            *value = 0;
        }
        return HdbDllReturnResultBadAlloc(result);
    }
    catch (...)
    {
        if (value != NULL)
        {
            *value = 0;
        }
        return HdbDllReturnResultException(result);
    }
}

int HDB_CALL HdbResultGetInt64(HDB_RESULT result, const char* outputName, HdbInt64* value)
{
    if (value != NULL)
    {
        *value = 0;
    }
    try
    {
        return HdbResultGetInt64Impl(result, outputName, value);
    }
    catch (const std::bad_alloc&)
    {
        if (value != NULL)
        {
            *value = 0;
        }
        return HdbDllReturnResultBadAlloc(result);
    }
    catch (...)
    {
        if (value != NULL)
        {
            *value = 0;
        }
        return HdbDllReturnResultException(result);
    }
}

int HDB_CALL HdbResultGetDouble(HDB_RESULT result, const char* outputName, double* value)
{
    if (value != NULL)
    {
        *value = 0.0;
    }
    try
    {
        return HdbResultGetDoubleImpl(result, outputName, value);
    }
    catch (const std::bad_alloc&)
    {
        if (value != NULL)
        {
            *value = 0.0;
        }
        return HdbDllReturnResultBadAlloc(result);
    }
    catch (...)
    {
        if (value != NULL)
        {
            *value = 0.0;
        }
        return HdbDllReturnResultException(result);
    }
}

int HDB_CALL HdbResultGetString(HDB_RESULT result,
    const char* outputName,
    char* buffer,
    int bufferSize,
    int* requiredSize)
{
    if (buffer != NULL && bufferSize > 0)
    {
        buffer[0] = '\0';
    }
    if (requiredSize != NULL)
    {
        *requiredSize = 0;
    }
    try
    {
        return HdbResultGetStringImpl(result, outputName, buffer, bufferSize, requiredSize);
    }
    catch (const std::bad_alloc&)
    {
        if (buffer != NULL && bufferSize > 0)
        {
            buffer[0] = '\0';
        }
        if (requiredSize != NULL)
        {
            *requiredSize = 0;
        }
        return HdbDllReturnResultBadAlloc(result);
    }
    catch (...)
    {
        if (buffer != NULL && bufferSize > 0)
        {
            buffer[0] = '\0';
        }
        if (requiredSize != NULL)
        {
            *requiredSize = 0;
        }
        return HdbDllReturnResultException(result);
    }
}
