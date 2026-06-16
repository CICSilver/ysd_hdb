#ifndef YSD_HDB_DB_ADAPTER_H
#define YSD_HDB_DB_ADAPTER_H

#include "HdbCommon.h"

#include <string>
#include <vector>

struct HdbQueryCell
{
    std::string value; // 数据库文本值
    int isNull;        // 1 表示数据库 NULL
};

// 查询结果容器
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
    // SetValue 也给 executor 后处理使用，例如 timestamp_ms 归一化
    void SetValue(int row, int field, const std::string& value, int isNull);
    void AddRow(const std::vector<HdbQueryCell>& row);

private:
    std::vector<std::string> m_columns;          // 输出列名
    std::vector< std::vector<HdbQueryCell> > m_rows; // 行数据
};

// 数据库适配器只执行已经生成好的 SQL
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

    // ExecCommand 保留给内部固定命令，动态业务 SQL 走 ExecParams
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
