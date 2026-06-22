#ifndef YSD_HDB_QUERY_BUILDER_H
#define YSD_HDB_QUERY_BUILDER_H

#include "ysd_hdb_c.h"

#include <stddef.h>

class CHdbQueryBuilder
{
public:
    explicit CHdbQueryBuilder(HDB_SESSION session)
        : m_query(NULL),
          m_error(HDB_OK)
    {
        m_error = HdbQueryCreate(session, &m_query);
    }

    ~CHdbQueryBuilder()
    {
        if (m_query != NULL)
        {
            HdbQueryFree(m_query);
            m_query = NULL;
        }
    }

    CHdbQueryBuilder& From(const char* datasetName, HDB_SOURCE& outRootSource)
    {
        outRootSource = NULL;
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryFrom(m_query, datasetName, &outRootSource);
        }
        return *this;
    }

    CHdbQueryBuilder& LeftJoin(HDB_SOURCE fromSource, const char* associationName, HDB_SOURCE& outTargetSource)
    {
        outTargetSource = NULL;
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryJoin(m_query, fromSource, associationName, HDB_JOIN_LEFT, &outTargetSource);
        }
        return *this;
    }

    CHdbQueryBuilder& InnerJoin(HDB_SOURCE fromSource, const char* associationName, HDB_SOURCE& outTargetSource)
    {
        outTargetSource = NULL;
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryJoin(m_query, fromSource, associationName, HDB_JOIN_INNER, &outTargetSource);
        }
        return *this;
    }

    CHdbQueryBuilder& Select(HDB_SOURCE source, const char* fieldName, const char* outputName)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQuerySelect(m_query, source, fieldName, outputName);
        }
        return *this;
    }

    CHdbQueryBuilder& WhereInt32(HDB_SOURCE source, const char* fieldName, int op, int value)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryWhereInt32(m_query, source, fieldName, op, value);
        }
        return *this;
    }

    CHdbQueryBuilder& WhereInt64(HDB_SOURCE source, const char* fieldName, int op, HdbInt64 value)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryWhereInt64(m_query, source, fieldName, op, value);
        }
        return *this;
    }

    CHdbQueryBuilder& WhereDouble(HDB_SOURCE source, const char* fieldName, int op, double value)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryWhereDouble(m_query, source, fieldName, op, value);
        }
        return *this;
    }

    CHdbQueryBuilder& WhereStringEq(HDB_SOURCE source, const char* fieldName, const char* value)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryWhereStringEq(m_query, source, fieldName, value);
        }
        return *this;
    }

    CHdbQueryBuilder& WhereStringLike(HDB_SOURCE source, const char* fieldName, const char* pattern)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryWhereStringLike(m_query, source, fieldName, pattern);
        }
        return *this;
    }

    CHdbQueryBuilder& OrderBy(HDB_SOURCE source, const char* fieldName, int orderType)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryOrderBy(m_query, source, fieldName, orderType);
        }
        return *this;
    }

    CHdbQueryBuilder& TimeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryTimeRange(m_query, beginMs, endMs);
        }
        return *this;
    }

    CHdbQueryBuilder& Limit(int limit, int offset)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryLimit(m_query, limit, offset);
        }
        return *this;
    }

    int Execute(HDB_RESULT* outResult)
    {
        if (outResult != NULL)
        {
            *outResult = NULL;
        }
        if (m_error != HDB_OK)
        {
            return m_error;
        }
        m_error = HdbQueryExecute(m_query, outResult);
        return m_error;
    }

    int GetError() const
    {
        return m_error;
    }

    HDB_QUERY GetQuery() const
    {
        return m_query;
    }

private:
    CHdbQueryBuilder(const CHdbQueryBuilder&);
    CHdbQueryBuilder& operator=(const CHdbQueryBuilder&);

private:
    HDB_QUERY m_query;
    int m_error;
};

#endif
