#ifndef YSD_HDB_QUERY_BUILDER_H
#define YSD_HDB_QUERY_BUILDER_H

#include "ysd_hdb_c.h"

#include <stddef.h>
#include <string>

class CHdbFieldRef;

class CHdbSource
{
public:
    CHdbSource()
        : m_ownerQuery(NULL),
          m_source(NULL)
    {
    }

    int IsValid() const
    {
        return m_ownerQuery != NULL && m_source != NULL;
    }

    CHdbFieldRef Field(const char* fieldName) const;

private:
    CHdbSource(HDB_QUERY ownerQuery, HDB_SOURCE source)
        : m_ownerQuery(ownerQuery),
          m_source(source)
    {
    }

private:
    HDB_QUERY m_ownerQuery;
    HDB_SOURCE m_source;

    friend class CHdbQueryBuilder;
};

class CHdbFieldRef
{
public:
    CHdbFieldRef()
        : m_ownerQuery(NULL),
          m_source(NULL),
          m_fieldName()
    {
    }

    int IsValid() const
    {
        return m_ownerQuery != NULL && m_source != NULL && !m_fieldName.empty();
    }

private:
    CHdbFieldRef(HDB_QUERY ownerQuery, HDB_SOURCE source, const char* fieldName)
        : m_ownerQuery(ownerQuery),
          m_source(source),
          m_fieldName(fieldName != NULL ? fieldName : "")
    {
    }

private:
    HDB_QUERY m_ownerQuery;
    HDB_SOURCE m_source;
    std::string m_fieldName;

    friend class CHdbSource;
    friend class CHdbQueryBuilder;
};

inline CHdbFieldRef CHdbSource::Field(const char* fieldName) const
{
    return CHdbFieldRef(m_ownerQuery, m_source, fieldName);
}

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

    CHdbSource From(const char* datasetName)
    {
        HDB_SOURCE source;

        source = NULL;
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryFrom(m_query, datasetName, &source);
        }
        if (m_error != HDB_OK)
        {
            return CHdbSource();
        }
        return CHdbSource(m_query, source);
    }

    CHdbSource LeftJoin(const CHdbSource& fromSource, const char* associationName)
    {
        return Join(fromSource, associationName, HDB_JOIN_LEFT);
    }

    CHdbSource InnerJoin(const CHdbSource& fromSource, const char* associationName)
    {
        return Join(fromSource, associationName, HDB_JOIN_INNER);
    }

    CHdbQueryBuilder& Select(const CHdbFieldRef& field, const char* outputName)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQuerySelect(m_query, field.m_source, field.m_fieldName.c_str(), outputName);
            }
        }
        return *this;
    }

    CHdbQueryBuilder& WhereInt32(const CHdbFieldRef& field, int op, int value)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryWhereInt32(m_query, field.m_source, field.m_fieldName.c_str(), op, value);
            }
        }
        return *this;
    }

    CHdbQueryBuilder& WhereInt64(const CHdbFieldRef& field, int op, HdbInt64 value)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryWhereInt64(m_query, field.m_source, field.m_fieldName.c_str(), op, value);
            }
        }
        return *this;
    }

    CHdbQueryBuilder& WhereDouble(const CHdbFieldRef& field, int op, double value)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryWhereDouble(m_query, field.m_source, field.m_fieldName.c_str(), op, value);
            }
        }
        return *this;
    }

    CHdbQueryBuilder& WhereStringEq(const CHdbFieldRef& field, const char* value)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryWhereStringEq(m_query, field.m_source, field.m_fieldName.c_str(), value);
            }
        }
        return *this;
    }

    CHdbQueryBuilder& WhereStringLike(const CHdbFieldRef& field, const char* pattern)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryWhereStringLike(m_query, field.m_source, field.m_fieldName.c_str(), pattern);
            }
        }
        return *this;
    }

    CHdbQueryBuilder& OrderBy(const CHdbFieldRef& field, int orderType)
    {
        if (m_error == HDB_OK)
        {
            m_error = ValidateField(field);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryOrderBy(m_query, field.m_source, field.m_fieldName.c_str(), orderType);
            }
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

private:
    CHdbSource Join(const CHdbSource& fromSource, const char* associationName, int joinType)
    {
        HDB_SOURCE targetSource;

        targetSource = NULL;
        if (m_error == HDB_OK)
        {
            m_error = ValidateSource(fromSource);
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryJoin(m_query, fromSource.m_source, associationName, joinType, &targetSource);
            }
        }
        if (m_error != HDB_OK)
        {
            return CHdbSource();
        }
        return CHdbSource(m_query, targetSource);
    }

    int ValidateSource(const CHdbSource& source) const
    {
        if (m_query == NULL || source.m_ownerQuery != m_query || source.m_source == NULL)
        {
            return HDB_ERR_PARAM;
        }
        return HDB_OK;
    }

    int ValidateField(const CHdbFieldRef& field) const
    {
        if (m_query == NULL ||
            field.m_ownerQuery != m_query ||
            field.m_source == NULL ||
            field.m_fieldName.empty())
        {
            return HDB_ERR_PARAM;
        }
        return HDB_OK;
    }

private:
    CHdbQueryBuilder(const CHdbQueryBuilder&);
    CHdbQueryBuilder& operator=(const CHdbQueryBuilder&);

private:
    HDB_QUERY m_query;
    int m_error;
};

#endif
