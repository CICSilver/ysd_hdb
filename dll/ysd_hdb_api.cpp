#include "ysd_hdb.h"

#include "../common/HdbQueryAst.h"

#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

struct HdbSessionTag
{
    std::string profileName;
    std::string connInfo;
    std::string lastError;
};

struct HdbQueryTag
{
    HDB_SESSION session;
    CHdbQueryAst ast;
    std::string lastError;
};

struct HdbResultCell
{
    std::string value;
    int isNull;
};

struct HdbResultTag
{
    HDB_SESSION session;
    std::vector<std::string> columns;
    std::vector< std::vector<HdbResultCell> > rows;
    int currentRow;
    std::string lastError;
};

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

static int HdbDllCopyText(const std::string& text, char* buffer, int bufferSize, int* requiredSize)
{
    int required;

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
        if (result->columns[i] == outputName)
        {
            return i;
        }
    }
    return -1;
}

static const HdbResultCell* HdbDllGetCurrentCell(HDB_RESULT result, const char* outputName)
{
    int column;

    if (result == NULL || outputName == NULL)
    {
        return NULL;
    }
    if (result->currentRow < 0 || result->currentRow >= (int)result->rows.size())
    {
        return NULL;
    }
    column = HdbDllFindColumn(result, outputName);
    if (column < 0 || column >= (int)result->rows[result->currentRow].size())
    {
        return NULL;
    }
    return &result->rows[result->currentRow][column];
}

int HDB_CALL HdbOpen(const char* profileName, HDB_SESSION* outSession)
{
    HDB_SESSION session;

    if (outSession == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outSession = NULL;
    session = new HdbSessionTag();
    if (profileName != NULL)
    {
        session->profileName = profileName;
    }
    *outSession = session;
    return HDB_OK;
}

int HDB_CALL HdbOpenByConnInfo(const char* connInfo, HDB_SESSION* outSession)
{
    HDB_SESSION session;
    int ret;

    ret = HdbOpen("conninfo", &session);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (connInfo != NULL)
    {
        session->connInfo = connInfo;
    }
    *outSession = session;
    return HDB_OK;
}

int HDB_CALL HdbClose(HDB_SESSION session)
{
    if (session != NULL)
    {
        delete session;
    }
    return HDB_OK;
}

int HDB_CALL HdbPing(HDB_SESSION session)
{
    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    HdbDllSetSessionError(session, "IPC transport is not implemented");
    return HDB_ERR_NOT_IMPLEMENTED;
}

int HDB_CALL HdbGetLastError(HDB_SESSION session, char* buffer, int bufferSize, int* requiredSize)
{
    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    return HdbDllCopyText(session->lastError, buffer, bufferSize, requiredSize);
}

int HDB_CALL HdbInsertRow(HDB_SESSION session, const char* datasetName, const void* row, int rowSize)
{
    (void)datasetName;
    (void)row;
    (void)rowSize;
    if (session == NULL)
    {
        return HDB_ERR_PARAM;
    }
    HdbDllSetSessionError(session, "dataset insert requires IPC transport");
    return HDB_ERR_NOT_IMPLEMENTED;
}

int HDB_CALL HdbBatchInsertRows(HDB_SESSION session,
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

int HDB_CALL HdbQueryCreate(HDB_SESSION session, const char* datasetName, HDB_QUERY* outQuery)
{
    HDB_QUERY query;

    if (session == NULL || datasetName == NULL || datasetName[0] == '\0' || outQuery == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outQuery = NULL;
    query = new HdbQueryTag();
    query->session = session;
    if (query->ast.SetRootDataset(datasetName) != 0)
    {
        delete query;
        HdbDllSetSessionError(session, "invalid dataset name");
        return HDB_ERR_PARAM;
    }
    *outQuery = query;
    return HDB_OK;
}

int HDB_CALL HdbQueryFree(HDB_QUERY query)
{
    if (query != NULL)
    {
        delete query;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryTimeRange(HDB_QUERY query, HdbInt64 beginMs, HdbInt64 endMs)
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

int HDB_CALL HdbQuerySelectPath(HDB_QUERY query, const char* fieldPath, const char* outputName)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.AddSelect(fieldPath, outputName) != 0)
    {
        HdbDllSetQueryError(query, "invalid select path");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryWhereInt32(HDB_QUERY query, const char* fieldPath, int op, int value)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.AddWhereInt32(fieldPath, op, value) != 0)
    {
        HdbDllSetQueryError(query, "invalid int32 where condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryWhereInt64(HDB_QUERY query, const char* fieldPath, int op, HdbInt64 value)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.AddWhereInt64(fieldPath, op, (HdbQueryInt64)value) != 0)
    {
        HdbDllSetQueryError(query, "invalid int64 where condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryWhereDouble(HDB_QUERY query, const char* fieldPath, int op, double value)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.AddWhereDouble(fieldPath, op, value) != 0)
    {
        HdbDllSetQueryError(query, "invalid double where condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryWhereStringEq(HDB_QUERY query, const char* fieldPath, const char* value)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.AddWhereString(fieldPath, HDB_OP_EQ, value) != 0)
    {
        HdbDllSetQueryError(query, "invalid string equal condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryWhereStringLike(HDB_QUERY query, const char* fieldPath, const char* pattern)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (query->ast.AddWhereString(fieldPath, HDB_OP_LIKE, pattern) != 0)
    {
        HdbDllSetQueryError(query, "invalid string like condition");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryOrderBy(HDB_QUERY query, const char* fieldPath, int orderType)
{
    if (query == NULL)
    {
        return HDB_ERR_PARAM;
    }
    if (orderType != HDB_ORDER_ASC && orderType != HDB_ORDER_DESC)
    {
        HdbDllSetQueryError(query, "invalid order type");
        return HDB_ERR_QUERY_RANGE;
    }
    if (query->ast.AddOrder(fieldPath, orderType) != 0)
    {
        HdbDllSetQueryError(query, "invalid order path");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int HDB_CALL HdbQueryLimit(HDB_QUERY query, int limit, int offset)
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

int HDB_CALL HdbQueryExecute(HDB_QUERY query, HDB_RESULT* outResult)
{
    if (query == NULL || outResult == NULL)
    {
        return HDB_ERR_PARAM;
    }
    *outResult = NULL;
    HdbDllSetQueryError(query, "query execute requires IPC transport");
    return HDB_ERR_NOT_IMPLEMENTED;
}

int HDB_CALL HdbResultFree(HDB_RESULT result)
{
    if (result != NULL)
    {
        delete result;
    }
    return HDB_OK;
}

int HDB_CALL HdbResultNext(HDB_RESULT result, int* hasRow)
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

int HDB_CALL HdbResultIsNull(HDB_RESULT result, const char* outputName, int* isNull)
{
    const HdbResultCell* cell;

    if (result == NULL || outputName == NULL || isNull == NULL)
    {
        return HDB_ERR_PARAM;
    }
    cell = HdbDllGetCurrentCell(result, outputName);
    if (cell == NULL)
    {
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    *isNull = cell->isNull != 0 ? 1 : 0;
    return HDB_OK;
}

int HDB_CALL HdbResultGetInt32(HDB_RESULT result, const char* outputName, int* value)
{
    const HdbResultCell* cell;

    if (value == NULL)
    {
        return HDB_ERR_PARAM;
    }
    cell = HdbDllGetCurrentCell(result, outputName);
    if (cell == NULL)
    {
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    *value = atoi(cell->value.c_str());
    return HDB_OK;
}

int HDB_CALL HdbResultGetInt64(HDB_RESULT result, const char* outputName, HdbInt64* value)
{
    const HdbResultCell* cell;

    if (value == NULL)
    {
        return HDB_ERR_PARAM;
    }
    cell = HdbDllGetCurrentCell(result, outputName);
    if (cell == NULL)
    {
        return HDB_ERR_FIELD_NOT_FOUND;
    }
#ifdef _WIN32
    *value = (HdbInt64)_strtoi64(cell->value.c_str(), NULL, 10);
#else
    *value = (HdbInt64)strtoll(cell->value.c_str(), NULL, 10);
#endif
    return HDB_OK;
}

int HDB_CALL HdbResultGetDouble(HDB_RESULT result, const char* outputName, double* value)
{
    const HdbResultCell* cell;

    if (value == NULL)
    {
        return HDB_ERR_PARAM;
    }
    cell = HdbDllGetCurrentCell(result, outputName);
    if (cell == NULL)
    {
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    *value = atof(cell->value.c_str());
    return HDB_OK;
}

int HDB_CALL HdbResultGetString(HDB_RESULT result,
    const char* outputName,
    char* buffer,
    int bufferSize,
    int* requiredSize)
{
    const HdbResultCell* cell;

    cell = HdbDllGetCurrentCell(result, outputName);
    if (cell == NULL)
    {
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    return HdbDllCopyText(cell->value, buffer, bufferSize, requiredSize);
}
