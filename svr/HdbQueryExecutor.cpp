#include "HdbQueryExecutor.h"

#include <vector>

CHdbQueryExecutor::CHdbQueryExecutor(CHdbDbAdapter* adapter, const CHdbDatasetRegistry* registry)
    : m_adapter(adapter),
      m_registry(registry)
{
}

int CHdbQueryExecutor::Execute(const CHdbQueryAst& ast, CHdbQueryResult& result)
{
    CHdbQuerySqlBuilder builder(m_registry);
    HdbBuiltQuery query;
    std::vector<const char*> params;
    int ret;
    int i;

    if (m_adapter == NULL)
    {
        SetLastError("database adapter is NULL");
        return HDB_ERR_PARAM;
    }
    ret = builder.BuildSelect(ast, query);
    if (ret != HDB_OK)
    {
        SetLastError(builder.GetLastError());
        return ret;
    }
    params.clear();
    for (i = 0; i < (int)query.params.size(); ++i)
    {
        params.push_back(query.params[i].c_str());
    }
    ret = m_adapter->QueryParams(query.sql.c_str(),
        (int)params.size(),
        params.empty() ? NULL : &params[0],
        result);
    if (ret != HDB_OK)
    {
        SetLastError(m_adapter->GetLastError());
        return ret;
    }
    for (i = 0; i < result.FieldCount() && i < (int)query.outputNames.size(); ++i)
    {
        result.SetColumnName(i, query.outputNames[i]);
    }
    return HDB_OK;
}

const char* CHdbQueryExecutor::GetLastError() const
{
    return m_lastError.c_str();
}

void CHdbQueryExecutor::SetLastError(const char* text)
{
    if (text == NULL || text[0] == '\0')
    {
        m_lastError = "unknown query executor error";
    }
    else
    {
        m_lastError = text;
    }
}
