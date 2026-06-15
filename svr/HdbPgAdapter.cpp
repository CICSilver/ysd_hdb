#include "HdbPgAdapter.h"

// PG 细节集中在 adapter，common 和 DLL 不包含 libpq
#include "libpq-fe.h"

#include <stdlib.h>

CHdbPgAdapter::CHdbPgAdapter()
    : m_conn(NULL)
{
}

CHdbPgAdapter::~CHdbPgAdapter()
{
    Close();
}

int CHdbPgAdapter::Open(const char* connInfo)
{
    // Open 先关闭旧连接，SERVER 侧只保留当前 adapter 连接
    Close();
    m_lastError.clear();

    if (connInfo == NULL || connInfo[0] == '\0')
    {
        SetLastError("empty postgres connection string");
        return HDB_ERR_PARAM;
    }

    m_conn = PQconnectdb(connInfo);
    if (m_conn == NULL)
    {
        SetLastError("PQconnectdb returned NULL");
        return HDB_ERR_DB_CONNECT;
    }

    if (PQstatus(m_conn) != CONNECTION_OK)
    {
        SetLastError(PQerrorMessage(m_conn));
        Close();
        return HDB_ERR_DB_CONNECT;
    }

    return HDB_OK;
}

int CHdbPgAdapter::Close()
{
    if (m_conn != NULL)
    {
        PQfinish(m_conn);
        m_conn = NULL;
    }
    return HDB_OK;
}

int CHdbPgAdapter::Ping()
{
    CHdbQueryResult result;
    return QueryParams("select 1", 0, NULL, result);
}

int CHdbPgAdapter::Begin()
{
    return ExecCommand("begin", NULL);
}

int CHdbPgAdapter::Commit()
{
    return ExecCommand("commit", NULL);
}

int CHdbPgAdapter::Rollback()
{
    return ExecCommand("rollback", NULL);
}

const char* CHdbPgAdapter::GetLastError() const
{
    return m_lastError.c_str();
}

int CHdbPgAdapter::ExecCommand(const char* sql, int* affectedRows)
{
    PGresult* res;
    ExecStatusType status;

    if (affectedRows != NULL)
    {
        *affectedRows = 0;
    }
    if (sql == NULL || sql[0] == '\0')
    {
        SetLastError("empty sql");
        return HDB_ERR_PARAM;
    }
    if (CheckConnected() != HDB_OK)
    {
        return HDB_ERR_NOT_CONNECTED;
    }

    res = PQexec(m_conn, sql);
    if (res == NULL)
    {
        SetLastError(PQerrorMessage(m_conn));
        return HDB_ERR_DB_EXEC;
    }

    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
    {
        SetLastError(PQresultErrorMessage(res));
        PQclear(res);
        return HDB_ERR_DB_EXEC;
    }

    if (affectedRows != NULL)
    {
        *affectedRows = ReadAffectedRows(PQcmdTuples(res));
    }
    PQclear(res);
    return HDB_OK;
}

int CHdbPgAdapter::ExecParams(const char* sql,
    int paramCount,
    const char* const* paramValues,
    int* affectedRows)
{
    PGresult* res;
    ExecStatusType status;

    if (affectedRows != NULL)
    {
        *affectedRows = 0;
    }
    if (sql == NULL || sql[0] == '\0' || paramCount < 0)
    {
        SetLastError("invalid sql parameters");
        return HDB_ERR_PARAM;
    }
    if (paramCount > 0 && paramValues == NULL)
    {
        SetLastError("missing sql parameter values");
        return HDB_ERR_PARAM;
    }
    if (CheckConnected() != HDB_OK)
    {
        return HDB_ERR_NOT_CONNECTED;
    }

    // 所有自动生成 SQL 走参数化执行，参数均按文本传给 libpq
    res = PQexecParams(m_conn, sql, paramCount, NULL, paramValues, NULL, NULL, 0);
    if (res == NULL)
    {
        SetLastError(PQerrorMessage(m_conn));
        return HDB_ERR_DB_EXEC;
    }

    status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK)
    {
        SetLastError(PQresultErrorMessage(res));
        PQclear(res);
        return HDB_ERR_DB_EXEC;
    }

    if (affectedRows != NULL)
    {
        *affectedRows = ReadAffectedRows(PQcmdTuples(res));
    }
    PQclear(res);
    return HDB_OK;
}

int CHdbPgAdapter::QueryParams(const char* sql,
    int paramCount,
    const char* const* paramValues,
    CHdbQueryResult& result)
{
    PGresult* res;
    ExecStatusType status;
    int row;
    int field;
    int rows;
    int fields;

    result.Clear();
    if (sql == NULL || sql[0] == '\0' || paramCount < 0)
    {
        SetLastError("invalid query parameters");
        return HDB_ERR_PARAM;
    }
    if (paramCount > 0 && paramValues == NULL)
    {
        SetLastError("missing query parameter values");
        return HDB_ERR_PARAM;
    }
    if (CheckConnected() != HDB_OK)
    {
        return HDB_ERR_NOT_CONNECTED;
    }

    res = PQexecParams(m_conn, sql, paramCount, NULL, paramValues, NULL, NULL, 0);
    if (res == NULL)
    {
        SetLastError(PQerrorMessage(m_conn));
        return HDB_ERR_DB_EXEC;
    }

    status = PQresultStatus(res);
    if (status != PGRES_TUPLES_OK)
    {
        SetLastError(PQresultErrorMessage(res));
        PQclear(res);
        return HDB_ERR_DB_EXEC;
    }

    // 查询结果立即拷贝成通用结构，避免 PGresult 生命周期泄露到上层
    rows = PQntuples(res);
    fields = PQnfields(res);
    for (field = 0; field < fields; ++field)
    {
        result.AddColumn(PQfname(res, field));
    }
    for (row = 0; row < rows; ++row)
    {
        std::vector<HdbQueryCell> values;
        for (field = 0; field < fields; ++field)
        {
            HdbQueryCell cell;
            if (PQgetisnull(res, row, field))
            {
                cell.value = "";
                cell.isNull = 1;
            }
            else
            {
                cell.value = PQgetvalue(res, row, field);
                cell.isNull = 0;
            }
            values.push_back(cell);
        }
        result.AddRow(values);
    }

    PQclear(res);
    return HDB_OK;
}

int CHdbPgAdapter::CheckConnected()
{
    // 每次执行前检查连接状态，避免上层拿到陈旧连接
    if (m_conn == NULL)
    {
        SetLastError("postgres connection is not open");
        return HDB_ERR_NOT_CONNECTED;
    }
    if (PQstatus(m_conn) != CONNECTION_OK)
    {
        SetLastError(PQerrorMessage(m_conn));
        return HDB_ERR_NOT_CONNECTED;
    }
    return HDB_OK;
}

void CHdbPgAdapter::SetLastError(const char* message)
{
    if (message == NULL || message[0] == '\0')
    {
        m_lastError = "unknown postgres error";
    }
    else
    {
        m_lastError = message;
    }
}

int CHdbPgAdapter::ReadAffectedRows(const char* rowText) const
{
    if (rowText == NULL || rowText[0] == '\0')
    {
        return 0;
    }
    return atoi(rowText);
}
