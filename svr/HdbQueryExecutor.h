#ifndef YSD_HDB_QUERY_EXECUTOR_H
#define YSD_HDB_QUERY_EXECUTOR_H

#include "HdbDbAdapter.h"
#include "HdbQuerySqlBuilder.h"

#include <string>
#include <vector>

// 查询执行器串起 SQL builder 和 DB adapter
class CHdbQueryExecutor
{
public:
    CHdbQueryExecutor(CHdbDbAdapter* adapter, const CHdbDatasetRegistry* registry);

    // result 列顺序和 outputTypes 顺序保持一致
    int Execute(const CHdbQueryAst& ast, CHdbQueryResult& result);
    int ExecuteAffected(const CHdbQueryAst& ast, int* affectedRows);
    const char* GetLastError() const;
    const std::vector<int>& GetLastOutputTypes() const;

private:
    // 当前把 timestamp_ms 查询结果归一化成 epoch ms 文本，便于 DLL GetInt64 读取
    int NormalizeTimestampMsColumns(CHdbQueryResult& result, const std::vector<int>& outputTypes);
    void SetLastError(const char* text);

private:
    CHdbDbAdapter* m_adapter;              // 数据库适配器
    const CHdbDatasetRegistry* m_registry; // 元数据注册表
    std::string m_lastError;               // 最近错误文本
    std::vector<int> m_lastOutputTypes;    // 最近一次 select 输出类型
};

#endif
