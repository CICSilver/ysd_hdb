#ifndef YSD_HDB_PG_ADAPTER_H
#define YSD_HDB_PG_ADAPTER_H

#include "HdbDbAdapter.h"

#include <string>

struct pg_conn;
typedef struct pg_conn PGconn;

class CHdbPgAdapter : public CHdbDbAdapter
{
public:
    CHdbPgAdapter();
    virtual ~CHdbPgAdapter();

    virtual int Open(const char* connInfo);
    virtual int Close();
    virtual int Ping();
    virtual int Begin();
    virtual int Commit();
    virtual int Rollback();
    virtual const char* GetLastError() const;

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
    PGconn* m_conn;
    std::string m_lastError;
};

#endif
