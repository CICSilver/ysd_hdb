#ifndef YSD_HDB_DB_ADAPTER_H
#define YSD_HDB_DB_ADAPTER_H

#include "HdbCommon.h"

#include <string>
#include <vector>

struct HdbQueryCell
{
    std::string value;
    int isNull;
};

class CHdbQueryResult
{
public:
    void Clear();
    int RowCount() const;
    int FieldCount() const;
    const char* GetColumnName(int field) const;
    int FindColumn(const char* name) const;
    const char* GetValue(int row, int field) const;
    int IsNull(int row, int field) const;
    void AddColumn(const std::string& name);
    void SetColumnName(int field, const std::string& name);
    void SetValue(int row, int field, const std::string& value, int isNull);
    void AddRow(const std::vector<HdbQueryCell>& row);

private:
    std::vector<std::string> m_columns;
    std::vector< std::vector<HdbQueryCell> > m_rows;
};

class CHdbDbAdapter
{
public:
    virtual ~CHdbDbAdapter() {}

    virtual int Open(const char* connInfo) = 0;
    virtual int Close() = 0;
    virtual int Ping() = 0;
    virtual int Begin() = 0;
    virtual int Commit() = 0;
    virtual int Rollback() = 0;
    virtual const char* GetLastError() const = 0;

    virtual int ExecCommand(const char* sql, int* affectedRows) = 0;
    virtual int ExecParams(const char* sql,
        int paramCount,
        const char* const* paramValues,
        int* affectedRows) = 0;
    virtual int QueryParams(const char* sql,
        int paramCount,
        const char* const* paramValues,
        CHdbQueryResult& result) = 0;
};

#endif
