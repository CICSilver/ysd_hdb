#ifndef YSD_HDB_QUERY_BUILDER_H
#define YSD_HDB_QUERY_BUILDER_H

#include "ysd_hdb_c.h"
#include "../common/HdbQueryAst.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <string>
#include <vector>

class CHdbDslCondition;

static inline std::string HdbDslIntToText(int value)
{
    std::ostringstream out;

    out << value;
    return out.str();
}

static inline std::string HdbDslInt64ToText(HdbInt64 value)
{
    std::ostringstream out;

    out << value;
    return out.str();
}

static inline std::string HdbDslDoubleToText(double value)
{
    std::ostringstream out;

    out.precision(17);
    out << value;
    return out.str();
}

static inline HdbInt64 HdbDslTextToInt64(const char* text)
{
#ifdef _WIN32
    return (HdbInt64)_atoi64(text);
#else
    return (HdbInt64)strtoll(text, NULL, 10);
#endif
}

class CHdbDslField
{
public:
    CHdbDslField();
    CHdbDslField(const char* datasetName, const char* fieldName, int fieldType);

    int IsValid() const;
    const char* DatasetName() const;
    const char* FieldName() const;
    int FieldType() const;
    CHdbDslCondition eq(int value) const;
    CHdbDslCondition eq(HdbInt64 value) const;
    CHdbDslCondition eq(double value) const;
    CHdbDslCondition eq(const char* value) const;
    CHdbDslCondition eq(const std::string& value) const;
    CHdbDslCondition eq(const CHdbDslField& right) const;
    int SameIdentity(const CHdbDslField& other) const;

private:
    const char* m_datasetName;
    const char* m_fieldName;
    int m_fieldType;
};

class CHdbDslCondition
{
public:
    CHdbDslCondition()
        : m_left(),
          m_right(),
          m_op(0),
          m_valueType(0),
          m_valueText(),
          m_isFieldCompare(0)
    {
    }

    int IsValid() const
    {
        return m_left.IsValid() && m_op != 0 &&
            (m_isFieldCompare ? m_right.IsValid() : m_valueType != 0);
    }

    int IsFieldCompare() const
    {
        return m_isFieldCompare;
    }

    const CHdbDslField& LeftField() const
    {
        return m_left;
    }

    const CHdbDslField& RightField() const
    {
        return m_right;
    }

    int Op() const
    {
        return m_op;
    }

    int ValueType() const
    {
        return m_valueType;
    }

    const char* ValueText() const
    {
        return m_valueText.c_str();
    }

private:
    CHdbDslCondition(const CHdbDslField& left, int op, int valueType, const std::string& valueText);
    CHdbDslCondition(const CHdbDslField& left, int op, const CHdbDslField& right);

private:
    CHdbDslField m_left;
    CHdbDslField m_right;
    int m_op;
    int m_valueType;
    std::string m_valueText;
    int m_isFieldCompare;

    friend class CHdbDslField;
};

inline CHdbDslField::CHdbDslField()
    : m_datasetName(""),
      m_fieldName(""),
      m_fieldType(HDB_FT_CHAR_ARRAY)
{
}

inline CHdbDslField::CHdbDslField(const char* datasetName, const char* fieldName, int fieldType)
    : m_datasetName(datasetName != NULL ? datasetName : ""),
      m_fieldName(fieldName != NULL ? fieldName : ""),
      m_fieldType(fieldType)
{
}

inline int CHdbDslField::IsValid() const
{
    return m_datasetName[0] != '\0' && m_fieldName[0] != '\0';
}

inline const char* CHdbDslField::DatasetName() const
{
    return m_datasetName;
}

inline const char* CHdbDslField::FieldName() const
{
    return m_fieldName;
}

inline int CHdbDslField::FieldType() const
{
    return m_fieldType;
}

inline CHdbDslCondition CHdbDslField::eq(int value) const
{
    if (m_fieldType == HDB_FT_INT64 || m_fieldType == HDB_FT_TIMESTAMP_MS)
    {
        return CHdbDslCondition(*this, HDB_OP_EQ, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
    }
    return CHdbDslCondition(*this, HDB_OP_EQ, HDB_QVT_INT32, HdbDslIntToText(value));
}

inline CHdbDslCondition CHdbDslField::eq(HdbInt64 value) const
{
    return CHdbDslCondition(*this, HDB_OP_EQ, HDB_QVT_INT64, HdbDslInt64ToText(value));
}

inline CHdbDslCondition CHdbDslField::eq(double value) const
{
    return CHdbDslCondition(*this, HDB_OP_EQ, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
}

inline CHdbDslCondition CHdbDslField::eq(const char* value) const
{
    return CHdbDslCondition(*this, HDB_OP_EQ, HDB_QVT_STRING, value != NULL ? value : "");
}

inline CHdbDslCondition CHdbDslField::eq(const std::string& value) const
{
    return CHdbDslCondition(*this, HDB_OP_EQ, HDB_QVT_STRING, value);
}

inline CHdbDslCondition CHdbDslField::eq(const CHdbDslField& right) const
{
    return CHdbDslCondition(*this, HDB_OP_EQ, right);
}

inline int CHdbDslField::SameIdentity(const CHdbDslField& other) const
{
    return strcmp(m_datasetName, other.m_datasetName) == 0 &&
        strcmp(m_fieldName, other.m_fieldName) == 0;
}

inline CHdbDslCondition::CHdbDslCondition(const CHdbDslField& left,
    int op,
    int valueType,
    const std::string& valueText)
    : m_left(left),
      m_right(),
      m_op(op),
      m_valueType(valueType),
      m_valueText(valueText),
      m_isFieldCompare(0)
{
}

inline CHdbDslCondition::CHdbDslCondition(const CHdbDslField& left,
    int op,
    const CHdbDslField& right)
    : m_left(left),
      m_right(right),
      m_op(op),
      m_valueType(0),
      m_valueText(),
      m_isFieldCompare(1)
{
}

class CHdbDslTable
{
public:
    explicit CHdbDslTable(const char* datasetName)
        : m_datasetName(datasetName != NULL ? datasetName : "")
    {
    }

    const char* DatasetName() const
    {
        return m_datasetName;
    }

private:
    const char* m_datasetName;
};

struct HdbDslSelectedField
{
    CHdbDslField field;
    std::string outputName;
};

struct HdbDslSourceBinding
{
    const char* datasetName;
    HDB_SOURCE source;
};

class CHdbDslResult
{
public:
    CHdbDslResult()
        : m_result(NULL)
    {
    }

    ~CHdbDslResult()
    {
        Clear();
    }

    void Clear()
    {
        if (m_result != NULL)
        {
            HdbResultFree(m_result);
            m_result = NULL;
        }
        m_fields.clear();
    }

    int Next(int* hasRow)
    {
        if (m_result == NULL)
        {
            return HDB_ERR_PARAM;
        }
        return HdbResultNext(m_result, hasRow);
    }

    int IsNull(const CHdbDslField& field, int* isNull)
    {
        const char* outputName;

        outputName = FindOutputName(field);
        if (outputName == NULL)
        {
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        return HdbResultIsNull(m_result, outputName, isNull);
    }

    int Get(const CHdbDslField& field, int& value)
    {
        const char* outputName;

        outputName = FindOutputName(field);
        if (outputName == NULL)
        {
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        return HdbResultGetInt32(m_result, outputName, &value);
    }

    int Get(const CHdbDslField& field, HdbInt64& value)
    {
        const char* outputName;

        outputName = FindOutputName(field);
        if (outputName == NULL)
        {
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        return HdbResultGetInt64(m_result, outputName, &value);
    }

    int Get(const CHdbDslField& field, double& value)
    {
        const char* outputName;

        outputName = FindOutputName(field);
        if (outputName == NULL)
        {
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        return HdbResultGetDouble(m_result, outputName, &value);
    }

    int Get(const CHdbDslField& field, char* buffer, int bufferSize, int* requiredSize)
    {
        const char* outputName;

        outputName = FindOutputName(field);
        if (outputName == NULL)
        {
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        return HdbResultGetString(m_result, outputName, buffer, bufferSize, requiredSize);
    }

    int Get(const CHdbDslField& field, std::string& value)
    {
        const char* outputName;
        std::vector<char> buffer;
        int requiredSize;
        int ret;

        value.clear();
        outputName = FindOutputName(field);
        if (outputName == NULL)
        {
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        requiredSize = 0;
        ret = HdbResultGetString(m_result, outputName, NULL, 0, &requiredSize);
        if (ret != HDB_ERR_BUFFER)
        {
            return ret;
        }
        if (requiredSize <= 0)
        {
            return HDB_ERR_BUFFER;
        }
        buffer.resize(requiredSize);
        ret = HdbResultGetString(m_result, outputName, &buffer[0], (int)buffer.size(), &requiredSize);
        if (ret == HDB_OK)
        {
            value = &buffer[0];
        }
        return ret;
    }

    void Attach(HDB_RESULT result, const std::vector<HdbDslSelectedField>& fields)
    {
        Clear();
        m_result = result;
        m_fields = fields;
    }

private:
    const char* FindOutputName(const CHdbDslField& field) const
    {
        int i;

        for (i = 0; i < (int)m_fields.size(); ++i)
        {
            if (m_fields[i].field.SameIdentity(field))
            {
                return m_fields[i].outputName.c_str();
            }
        }
        return NULL;
    }

private:
    CHdbDslResult(const CHdbDslResult&);
    CHdbDslResult& operator=(const CHdbDslResult&);

private:
    HDB_RESULT m_result;
    std::vector<HdbDslSelectedField> m_fields;
};

class CHdbDslJoinStep;

class CHdbDslQuery
{
public:
    explicit CHdbDslQuery(HDB_SESSION session)
        : m_session(session),
          m_query(NULL),
          m_error(HDB_OK),
          m_hasRoot(0),
          m_hasWhere(0),
          m_selectsAdded(0)
    {
    }

    ~CHdbDslQuery()
    {
        FreeQuery();
    }

    void Reset()
    {
        FreeQuery();
        m_error = HDB_OK;
        m_hasRoot = 0;
        m_hasWhere = 0;
        m_selectsAdded = 0;
        m_selectedFields.clear();
        m_sources.clear();
        m_where = CHdbDslCondition();
    }

    CHdbDslQuery& select(const CHdbDslField& field)
    {
        HdbDslSelectedField selected;

        if (m_error == HDB_OK)
        {
            if (!field.IsValid())
            {
                m_error = HDB_ERR_PARAM;
            }
            else
            {
                selected.field = field;
                selected.outputName = BuildOutputName((int)m_selectedFields.size());
                m_selectedFields.push_back(selected);
            }
        }
        return *this;
    }

    CHdbDslQuery& from(const CHdbDslTable& table)
    {
        HDB_SOURCE source;
        int ret;

        source = NULL;
        if (m_error == HDB_OK)
        {
            ret = EnsureQuery();
            if (ret == HDB_OK)
            {
                ret = HdbQueryFrom(m_query, table.DatasetName(), &source);
            }
            if (ret == HDB_OK)
            {
                AddSource(table.DatasetName(), source);
                m_hasRoot = 1;
            }
            m_error = ret;
        }
        return *this;
    }

    CHdbDslJoinStep leftJoin(const CHdbDslTable& table);
    CHdbDslJoinStep innerJoin(const CHdbDslTable& table);

    CHdbDslQuery& where(const CHdbDslCondition& condition)
    {
        if (m_error == HDB_OK)
        {
            if (!condition.IsValid() || condition.IsFieldCompare() || m_hasWhere)
            {
                m_error = HDB_ERR_QUERY_RANGE;
            }
            else
            {
                m_where = condition;
                m_hasWhere = 1;
            }
        }
        return *this;
    }

    CHdbDslQuery& timeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        if (m_error == HDB_OK)
        {
            m_error = EnsureQuery();
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryTimeRange(m_query, beginMs, endMs);
            }
        }
        return *this;
    }

    CHdbDslQuery& limit(int limitValue, int offsetValue)
    {
        if (m_error == HDB_OK)
        {
            m_error = EnsureQuery();
            if (m_error == HDB_OK)
            {
                m_error = HdbQueryLimit(m_query, limitValue, offsetValue);
            }
        }
        return *this;
    }

    int fetch(CHdbDslResult* result)
    {
        HDB_RESULT rawResult;
        int ret;

        if (result == NULL)
        {
            return HDB_ERR_PARAM;
        }
        result->Clear();
        if (m_error != HDB_OK)
        {
            return m_error;
        }
        if (!m_hasRoot || m_selectedFields.empty())
        {
            return HDB_ERR_PARAM;
        }
        ret = EnsureQuery();
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        ret = AddSelectedFieldsToQuery();
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        ret = AddWhereToQuery();
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        rawResult = NULL;
        ret = HdbQueryExecute(m_query, &rawResult);
        if (ret == HDB_OK)
        {
            result->Attach(rawResult, m_selectedFields);
        }
        m_error = ret;
        return ret;
    }

    int GetError() const
    {
        return m_error;
    }

private:
    void FreeQuery()
    {
        if (m_query != NULL)
        {
            HdbQueryFree(m_query);
            m_query = NULL;
        }
    }

    int EnsureQuery()
    {
        if (m_query != NULL)
        {
            return HDB_OK;
        }
        if (m_session == NULL)
        {
            return HDB_ERR_PARAM;
        }
        return HdbQueryCreate(m_session, &m_query);
    }

    std::string BuildOutputName(int index) const
    {
        std::ostringstream out;

        out << "__hdb_dsl_" << (index + 1);
        return out.str();
    }

    void AddSource(const char* datasetName, HDB_SOURCE source)
    {
        HdbDslSourceBinding binding;

        binding.datasetName = datasetName;
        binding.source = source;
        m_sources.push_back(binding);
    }

    HDB_SOURCE FindSource(const char* datasetName) const
    {
        int i;

        for (i = 0; i < (int)m_sources.size(); ++i)
        {
            if (strcmp(m_sources[i].datasetName, datasetName) == 0)
            {
                return m_sources[i].source;
            }
        }
        return NULL;
    }

    int AddJoinOn(const CHdbDslTable& table, int joinType, const CHdbDslCondition& condition)
    {
        const CHdbDslField* localField;
        const CHdbDslField* targetField;
        HDB_SOURCE localSource;
        HDB_SOURCE targetSource;
        int ret;

        if (m_error != HDB_OK)
        {
            return m_error;
        }
        if (!m_hasRoot || !condition.IsValid() || !condition.IsFieldCompare())
        {
            m_error = HDB_ERR_QUERY_RANGE;
            return m_error;
        }
        localField = NULL;
        targetField = NULL;
        if (strcmp(condition.RightField().DatasetName(), table.DatasetName()) == 0)
        {
            localField = &condition.LeftField();
            targetField = &condition.RightField();
        }
        else if (strcmp(condition.LeftField().DatasetName(), table.DatasetName()) == 0)
        {
            localField = &condition.RightField();
            targetField = &condition.LeftField();
        }
        if (localField == NULL || targetField == NULL)
        {
            m_error = HDB_ERR_FIELD_REF;
            return m_error;
        }
        localSource = FindSource(localField->DatasetName());
        if (localSource == NULL || FindSource(table.DatasetName()) != NULL)
        {
            m_error = HDB_ERR_QUERY_RANGE;
            return m_error;
        }
        targetSource = NULL;
        ret = HdbQueryJoinOn(m_query,
            localSource,
            table.DatasetName(),
            joinType,
            localField->FieldName(),
            targetField->FieldName(),
            &targetSource);
        if (ret == HDB_OK)
        {
            AddSource(table.DatasetName(), targetSource);
        }
        m_error = ret;
        return ret;
    }

    int AddSelectedFieldsToQuery()
    {
        int i;

        if (m_selectsAdded)
        {
            return HDB_OK;
        }
        for (i = 0; i < (int)m_selectedFields.size(); ++i)
        {
            HDB_SOURCE source;

            source = FindSource(m_selectedFields[i].field.DatasetName());
            if (source == NULL)
            {
                return HDB_ERR_FIELD_REF;
            }
            if (HdbQuerySelect(m_query,
                source,
                m_selectedFields[i].field.FieldName(),
                m_selectedFields[i].outputName.c_str()) != HDB_OK)
            {
                return HDB_ERR_FIELD_REF;
            }
        }
        m_selectsAdded = 1;
        return HDB_OK;
    }

    int AddWhereToQuery()
    {
        HDB_SOURCE source;

        if (!m_hasWhere)
        {
            return HDB_OK;
        }
        source = FindSource(m_where.LeftField().DatasetName());
        if (source == NULL)
        {
            return HDB_ERR_FIELD_REF;
        }
        switch (m_where.ValueType())
        {
        case HDB_QVT_INT32:
            return HdbQueryWhereInt32(m_query,
                source,
                m_where.LeftField().FieldName(),
                m_where.Op(),
                atoi(m_where.ValueText()));
        case HDB_QVT_INT64:
            return HdbQueryWhereInt64(m_query,
                source,
                m_where.LeftField().FieldName(),
                m_where.Op(),
                HdbDslTextToInt64(m_where.ValueText()));
        case HDB_QVT_DOUBLE:
            return HdbQueryWhereDouble(m_query,
                source,
                m_where.LeftField().FieldName(),
                m_where.Op(),
                atof(m_where.ValueText()));
        case HDB_QVT_STRING:
            return HdbQueryWhereStringEq(m_query,
                source,
                m_where.LeftField().FieldName(),
                m_where.ValueText());
        default:
            return HDB_ERR_QUERY_RANGE;
        }
    }

private:
    CHdbDslQuery(const CHdbDslQuery&);
    CHdbDslQuery& operator=(const CHdbDslQuery&);

private:
    HDB_SESSION m_session;
    HDB_QUERY m_query;
    int m_error;
    int m_hasRoot;
    int m_hasWhere;
    int m_selectsAdded;
    std::vector<HdbDslSelectedField> m_selectedFields;
    std::vector<HdbDslSourceBinding> m_sources;
    CHdbDslCondition m_where;

    friend class CHdbDslJoinStep;
};

class CHdbDslJoinStep
{
public:
    CHdbDslJoinStep(CHdbDslQuery* query, const CHdbDslTable& table, int joinType)
        : m_query(query),
          m_table(table),
          m_joinType(joinType)
    {
    }

    CHdbDslQuery& on(const CHdbDslCondition& condition)
    {
        m_query->AddJoinOn(m_table, m_joinType, condition);
        return *m_query;
    }

private:
    CHdbDslQuery* m_query;
    CHdbDslTable m_table;
    int m_joinType;
};

inline CHdbDslJoinStep CHdbDslQuery::leftJoin(const CHdbDslTable& table)
{
    return CHdbDslJoinStep(this, table, HDB_JOIN_LEFT);
}

inline CHdbDslJoinStep CHdbDslQuery::innerJoin(const CHdbDslTable& table)
{
    return CHdbDslJoinStep(this, table, HDB_JOIN_INNER);
}

class CHdbCreate
{
public:
    explicit CHdbCreate(HDB_SESSION session)
        : m_query(session)
    {
    }

    CHdbDslQuery& select(const CHdbDslField& field)
    {
        m_query.Reset();
        return m_query.select(field);
    }

private:
    CHdbCreate(const CHdbCreate&);
    CHdbCreate& operator=(const CHdbCreate&);

private:
    CHdbDslQuery m_query;
};

#endif
