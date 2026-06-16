#include "HdbQueryExecutor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sstream>
#include <vector>

static HdbInt64 HdbExecutorParseInt64Text(const char* text, int* ok)
{
    char* endPtr;
    HdbInt64 value;

    if (ok != NULL)
    {
        *ok = 0;
    }
    if (text == NULL || text[0] == '\0')
    {
        return 0;
    }
    errno = 0;
    endPtr = NULL;
#ifdef _WIN32
    value = (HdbInt64)_strtoi64(text, &endPtr, 10);
#else
    value = (HdbInt64)strtoll(text, &endPtr, 10);
#endif
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        return 0;
    }
    if (ok != NULL)
    {
        *ok = 1;
    }
    return value;
}

static HdbInt64 HdbExecutorParseTimestampMs(const char* text, int* ok)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millis;
    struct tm tmValue;
    time_t seconds;

    if (ok != NULL)
    {
        *ok = 0;
    }
    if (text == NULL || text[0] == '\0')
    {
        return 0;
    }
    year = month = day = hour = minute = second = millis = 0;
    if (sscanf(text, "%d-%d-%d %d:%d:%d.%d", &year, &month, &day, &hour, &minute, &second, &millis) < 6)
    {
        return HdbExecutorParseInt64Text(text, ok);
    }

    memset(&tmValue, 0, sizeof(tmValue));
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = minute;
    tmValue.tm_sec = second;
    tmValue.tm_isdst = -1;
    seconds = mktime(&tmValue);
    if (seconds == (time_t)-1)
    {
        return 0;
    }
    if (ok != NULL)
    {
        *ok = 1;
    }
    return ((HdbInt64)seconds) * 1000 + millis;
}

static std::string HdbExecutorInt64ToString(HdbInt64 value)
{
    std::ostringstream out;

    out << value;
    return out.str();
}

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

    m_lastOutputTypes.clear();
    if (m_adapter == NULL)
    {
        SetLastError("database adapter is NULL");
        return HDB_ERR_PARAM;
    }
    // executor 是 AST 到数据库执行的唯一入口，DLL 不直接传 SQL
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
    // param 指针引用 query 的 params 字符串，只在本次同步调用内有效
    ret = m_adapter->QueryParams(query.sql.c_str(),
        (int)params.size(),
        params.empty() ? NULL : &params[0],
        result);
    if (ret != HDB_OK)
    {
        SetLastError(m_adapter->GetLastError());
        return ret;
    }
    // SQL 别名只用于数据库返回，最终列名恢复成调用方指定的 outputName
    for (i = 0; i < result.FieldCount() && i < (int)query.outputNames.size(); ++i)
    {
        result.SetColumnName(i, query.outputNames[i]);
    }
    ret = NormalizeTimestampMsColumns(result, query.outputTypes);
    if (ret != HDB_OK)
    {
        return ret;
    }
    m_lastOutputTypes = query.outputTypes;
    return HDB_OK;
}

const char* CHdbQueryExecutor::GetLastError() const
{
    return m_lastError.c_str();
}

const std::vector<int>& CHdbQueryExecutor::GetLastOutputTypes() const
{
    return m_lastOutputTypes;
}

int CHdbQueryExecutor::NormalizeTimestampMsColumns(CHdbQueryResult& result, const std::vector<int>& outputTypes)
{
    int field;
    int row;

    for (field = 0; field < result.FieldCount() && field < (int)outputTypes.size(); ++field)
    {
        if (outputTypes[field] != HDB_FT_TIMESTAMP_MS)
        {
            continue;
        }
        // timestamp 在数据库返回为文本，DLL 侧统一看到 epoch ms 字符串
        for (row = 0; row < result.RowCount(); ++row)
        {
            HdbInt64 ms;
            int ok;

            if (result.IsNull(row, field))
            {
                continue;
            }
            ok = 0;
            ms = HdbExecutorParseTimestampMs(result.GetValue(row, field), &ok);
            if (!ok)
            {
                SetLastError("timestamp result conversion failed");
                return HDB_ERR_TYPE_MISMATCH;
            }
            result.SetValue(row, field, HdbExecutorInt64ToString(ms), 0);
        }
    }
    return HDB_OK;
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
