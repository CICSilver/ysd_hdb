#ifndef YSD_HDB_QUERY_EXECUTOR_H
#define YSD_HDB_QUERY_EXECUTOR_H

#include "HdbDbAdapter.h"
#include "HdbQuerySqlBuilder.h"

#include <string>
#include <vector>

class CHdbQueryExecutor
{
public:
    CHdbQueryExecutor(CHdbDbAdapter* adapter, const CHdbDatasetRegistry* registry);

    int Execute(const CHdbQueryAst& ast, CHdbQueryResult& result);
    const char* GetLastError() const;
    const std::vector<int>& GetLastOutputTypes() const;

private:
    int NormalizeTimestampMsColumns(CHdbQueryResult& result, const std::vector<int>& outputTypes);
    void SetLastError(const char* text);

private:
    CHdbDbAdapter* m_adapter;
    const CHdbDatasetRegistry* m_registry;
    std::string m_lastError;
    std::vector<int> m_lastOutputTypes;
};

#endif
