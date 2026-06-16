#ifndef YSD_HDB_PG_ADAPTER_H
#define YSD_HDB_PG_ADAPTER_H

#include "HdbDbAdapter.h"

#include <string>

struct pg_conn;
typedef struct pg_conn PGconn;

// PostgreSQL libpq 适配器
class CHdbPgAdapter : public CHdbDbAdapter
{
public:
    CHdbPgAdapter();
    virtual ~CHdbPgAdapter();

    // PGconn 生命周期归 CHdbPgAdapter，Open 会先关闭旧连接
    virtual int Open(const char* connInfo);
    virtual int Close();
    virtual int Ping();
    // Begin/Commit/Rollback 给 batch insert 或 cursor 流程预留事务边界
    virtual int Begin();
    virtual int Commit();
    virtual int Rollback();
    virtual const char* GetLastError() const;

    // ExecParams 和 QueryParams 使用 PQexecParams，参数值当前都按文本传入
    virtual int ExecCommand(const char* sql, int* affectedRows);
    virtual int ExecParams(const char* sql,
        int paramCount,
        const char* const* paramValues,
        int* affectedRows);
    virtual int QueryParams(const char* sql,
        int paramCount,
        const char* const* paramValues,
        CHdbQueryResult& result);

private:
    int CheckConnected();
    void SetLastError(const char* message);
    int ReadAffectedRows(const char* rowText) const;

private:
    PGconn* m_conn;           // 当前 PGconn
    std::string m_lastError;  // 最近错误文本
};

#endif
