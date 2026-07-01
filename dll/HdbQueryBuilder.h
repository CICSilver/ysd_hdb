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
class CHdbDslSortField;
class CHdbDslValueList;

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
    CHdbDslCondition ne(int value) const;
    CHdbDslCondition ne(HdbInt64 value) const;
    CHdbDslCondition ne(double value) const;
    CHdbDslCondition ne(const char* value) const;
    CHdbDslCondition ne(const std::string& value) const;
    CHdbDslCondition ne(const CHdbDslField& right) const;
    CHdbDslCondition gt(int value) const;
    CHdbDslCondition gt(HdbInt64 value) const;
    CHdbDslCondition gt(double value) const;
    CHdbDslCondition gt(const CHdbDslField& right) const;
    CHdbDslCondition ge(int value) const;
    CHdbDslCondition ge(HdbInt64 value) const;
    CHdbDslCondition ge(double value) const;
    CHdbDslCondition ge(const CHdbDslField& right) const;
    CHdbDslCondition lt(int value) const;
    CHdbDslCondition lt(HdbInt64 value) const;
    CHdbDslCondition lt(double value) const;
    CHdbDslCondition lt(const CHdbDslField& right) const;
    CHdbDslCondition le(int value) const;
    CHdbDslCondition le(HdbInt64 value) const;
    CHdbDslCondition le(double value) const;
    CHdbDslCondition le(const CHdbDslField& right) const;
    CHdbDslCondition like(const char* value) const;
    CHdbDslCondition like(const std::string& value) const;
    CHdbDslCondition isNull() const;
    CHdbDslCondition isNotNull() const;
    CHdbDslCondition between(int beginValue, int endValue) const;
    CHdbDslCondition between(HdbInt64 beginValue, HdbInt64 endValue) const;
    CHdbDslCondition between(double beginValue, double endValue) const;
    CHdbDslCondition in(const int* values, int count) const;
    CHdbDslCondition in(const HdbInt64* values, int count) const;
    CHdbDslCondition in(const double* values, int count) const;
    CHdbDslCondition in(const char* const* values, int count) const;
    CHdbDslCondition in(const CHdbDslValueList& values) const;
    CHdbDslSortField asc() const;
    CHdbDslSortField desc() const;
    int SameIdentity(const CHdbDslField& other) const;

private:
    const char* m_datasetName;
    const char* m_fieldName;
    int m_fieldType;
};

class CHdbDslValueList
{
public:
    CHdbDslValueList()
        : m_valueType(0),
          m_values()
    {
    }

    CHdbDslValueList& add(int value)
    {
        AddValue(HDB_QVT_INT32, HdbDslIntToText(value));
        return *this;
    }

    CHdbDslValueList& add(HdbInt64 value)
    {
        AddValue(HDB_QVT_INT64, HdbDslInt64ToText(value));
        return *this;
    }

    CHdbDslValueList& add(double value)
    {
        AddValue(HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
        return *this;
    }

    CHdbDslValueList& add(const char* value)
    {
        AddValue(HDB_QVT_STRING, value != NULL ? value : "");
        return *this;
    }

    CHdbDslValueList& add(const std::string& value)
    {
        AddValue(HDB_QVT_STRING, value);
        return *this;
    }

    int IsValid() const
    {
        return m_valueType != 0 && !m_values.empty();
    }

    int ValueType() const
    {
        return m_valueType;
    }

    const std::vector<std::string>& Values() const
    {
        return m_values;
    }

private:
    void AddValue(int valueType, const std::string& valueText)
    {
        if (m_valueType == 0)
        {
            m_valueType = valueType;
        }
        if (m_valueType == valueType)
        {
            m_values.push_back(valueText);
        }
    }

private:
    int m_valueType;
    std::vector<std::string> m_values;
};

class CHdbDslSortField
{
public:
    CHdbDslSortField()
        : m_field(),
          m_orderType(0)
    {
    }

    CHdbDslSortField(const CHdbDslField& field, int orderType)
        : m_field(field),
          m_orderType(orderType)
    {
    }

    int IsValid() const
    {
        return m_field.IsValid() && (m_orderType == HDB_ORDER_ASC || m_orderType == HDB_ORDER_DESC);
    }

    const CHdbDslField& Field() const
    {
        return m_field;
    }

    int OrderType() const
    {
        return m_orderType;
    }

private:
    CHdbDslField m_field;
    int m_orderType;
};

class CHdbDslCondition
{
public:
    enum ConditionKind
    {
        HDB_DSL_CONDITION_EMPTY = 0,
        HDB_DSL_CONDITION_COMPARE = 1,
        HDB_DSL_CONDITION_FIELD_COMPARE = 2,
        HDB_DSL_CONDITION_NULL = 3,
        HDB_DSL_CONDITION_BETWEEN = 4,
        HDB_DSL_CONDITION_IN = 5,
        HDB_DSL_CONDITION_GROUP = 6
    };

    CHdbDslCondition()
        : m_kind(HDB_DSL_CONDITION_EMPTY),
          m_left(),
          m_right(),
          m_op(0),
          m_valueType(0),
          m_valueText(),
          m_secondValueText(),
          m_values(),
          m_logic(0),
          m_children()
    {
    }

    int IsValid() const
    {
        if (m_kind == HDB_DSL_CONDITION_COMPARE)
        {
            return m_left.IsValid() && m_op != 0 && m_valueType != 0;
        }
        if (m_kind == HDB_DSL_CONDITION_FIELD_COMPARE)
        {
            return m_left.IsValid() && m_right.IsValid() && m_op != 0;
        }
        if (m_kind == HDB_DSL_CONDITION_NULL)
        {
            return m_left.IsValid();
        }
        if (m_kind == HDB_DSL_CONDITION_BETWEEN)
        {
            return m_left.IsValid() && m_valueType != 0;
        }
        if (m_kind == HDB_DSL_CONDITION_IN)
        {
            return m_left.IsValid() && m_valueType != 0 && !m_values.empty();
        }
        if (m_kind == HDB_DSL_CONDITION_GROUP)
        {
            return (m_logic == HDB_QCL_AND || m_logic == HDB_QCL_OR) && m_children.size() >= 2;
        }
        return 0;
    }

    int IsFieldCompare() const
    {
        return m_kind == HDB_DSL_CONDITION_FIELD_COMPARE;
    }

    int Kind() const
    {
        return m_kind;
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

    const char* SecondValueText() const
    {
        return m_secondValueText.c_str();
    }

    const std::vector<std::string>& Values() const
    {
        return m_values;
    }

    int Logic() const
    {
        return m_logic;
    }

    const std::vector<CHdbDslCondition>& Children() const
    {
        return m_children;
    }

    CHdbDslCondition And(const CHdbDslCondition& other) const
    {
        return CHdbDslCondition(HDB_QCL_AND, *this, other);
    }

    CHdbDslCondition Or(const CHdbDslCondition& other) const
    {
        return CHdbDslCondition(HDB_QCL_OR, *this, other);
    }

private:
    CHdbDslCondition(const CHdbDslField& left, int op, int valueType, const std::string& valueText);
    CHdbDslCondition(const CHdbDslField& left, int op, const CHdbDslField& right);
    CHdbDslCondition(const CHdbDslField& left, int isNotNull);
    CHdbDslCondition(const CHdbDslField& left,
        int valueType,
        const std::string& beginText,
        const std::string& endText);
    CHdbDslCondition(const CHdbDslField& left,
        int valueType,
        const std::vector<std::string>& values);
    CHdbDslCondition(int logic, const CHdbDslCondition& left, const CHdbDslCondition& right);

private:
    int m_kind;
    CHdbDslField m_left;
    CHdbDslField m_right;
    int m_op;
    int m_valueType;
    std::string m_valueText;
    std::string m_secondValueText;
    std::vector<std::string> m_values;
    int m_logic;
    std::vector<CHdbDslCondition> m_children;

    friend class CHdbDslField;
};

static inline int HdbDslIntegerValueTypeForField(const CHdbDslField& field)
{
    return field.FieldType() == HDB_FT_INT64 || field.FieldType() == HDB_FT_TIMESTAMP_MS ?
        HDB_QVT_INT64 :
        HDB_QVT_INT32;
}

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

inline CHdbDslCondition CHdbDslField::ne(int value) const
{
    if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
    {
        return CHdbDslCondition(*this, HDB_OP_NE, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
    }
    return CHdbDslCondition(*this, HDB_OP_NE, HDB_QVT_INT32, HdbDslIntToText(value));
}

inline CHdbDslCondition CHdbDslField::ne(HdbInt64 value) const
{
    return CHdbDslCondition(*this, HDB_OP_NE, HDB_QVT_INT64, HdbDslInt64ToText(value));
}

inline CHdbDslCondition CHdbDslField::ne(double value) const
{
    return CHdbDslCondition(*this, HDB_OP_NE, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
}

inline CHdbDslCondition CHdbDslField::ne(const char* value) const
{
    return CHdbDslCondition(*this, HDB_OP_NE, HDB_QVT_STRING, value != NULL ? value : "");
}

inline CHdbDslCondition CHdbDslField::ne(const std::string& value) const
{
    return CHdbDslCondition(*this, HDB_OP_NE, HDB_QVT_STRING, value);
}

inline CHdbDslCondition CHdbDslField::ne(const CHdbDslField& right) const
{
    return CHdbDslCondition(*this, HDB_OP_NE, right);
}

inline CHdbDslCondition CHdbDslField::gt(int value) const
{
    if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
    {
        return CHdbDslCondition(*this, HDB_OP_GT, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
    }
    return CHdbDslCondition(*this, HDB_OP_GT, HDB_QVT_INT32, HdbDslIntToText(value));
}

inline CHdbDslCondition CHdbDslField::gt(HdbInt64 value) const
{
    return CHdbDslCondition(*this, HDB_OP_GT, HDB_QVT_INT64, HdbDslInt64ToText(value));
}

inline CHdbDslCondition CHdbDslField::gt(double value) const
{
    return CHdbDslCondition(*this, HDB_OP_GT, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
}

inline CHdbDslCondition CHdbDslField::gt(const CHdbDslField& right) const
{
    return CHdbDslCondition(*this, HDB_OP_GT, right);
}

inline CHdbDslCondition CHdbDslField::ge(int value) const
{
    if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
    {
        return CHdbDslCondition(*this, HDB_OP_GE, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
    }
    return CHdbDslCondition(*this, HDB_OP_GE, HDB_QVT_INT32, HdbDslIntToText(value));
}

inline CHdbDslCondition CHdbDslField::ge(HdbInt64 value) const
{
    return CHdbDslCondition(*this, HDB_OP_GE, HDB_QVT_INT64, HdbDslInt64ToText(value));
}

inline CHdbDslCondition CHdbDslField::ge(double value) const
{
    return CHdbDslCondition(*this, HDB_OP_GE, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
}

inline CHdbDslCondition CHdbDslField::ge(const CHdbDslField& right) const
{
    return CHdbDslCondition(*this, HDB_OP_GE, right);
}

inline CHdbDslCondition CHdbDslField::lt(int value) const
{
    if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
    {
        return CHdbDslCondition(*this, HDB_OP_LT, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
    }
    return CHdbDslCondition(*this, HDB_OP_LT, HDB_QVT_INT32, HdbDslIntToText(value));
}

inline CHdbDslCondition CHdbDslField::lt(HdbInt64 value) const
{
    return CHdbDslCondition(*this, HDB_OP_LT, HDB_QVT_INT64, HdbDslInt64ToText(value));
}

inline CHdbDslCondition CHdbDslField::lt(double value) const
{
    return CHdbDslCondition(*this, HDB_OP_LT, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
}

inline CHdbDslCondition CHdbDslField::lt(const CHdbDslField& right) const
{
    return CHdbDslCondition(*this, HDB_OP_LT, right);
}

inline CHdbDslCondition CHdbDslField::le(int value) const
{
    if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
    {
        return CHdbDslCondition(*this, HDB_OP_LE, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
    }
    return CHdbDslCondition(*this, HDB_OP_LE, HDB_QVT_INT32, HdbDslIntToText(value));
}

inline CHdbDslCondition CHdbDslField::le(HdbInt64 value) const
{
    return CHdbDslCondition(*this, HDB_OP_LE, HDB_QVT_INT64, HdbDslInt64ToText(value));
}

inline CHdbDslCondition CHdbDslField::le(double value) const
{
    return CHdbDslCondition(*this, HDB_OP_LE, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
}

inline CHdbDslCondition CHdbDslField::le(const CHdbDslField& right) const
{
    return CHdbDslCondition(*this, HDB_OP_LE, right);
}

inline CHdbDslCondition CHdbDslField::like(const char* value) const
{
    return CHdbDslCondition(*this, HDB_OP_LIKE, HDB_QVT_STRING, value != NULL ? value : "");
}

inline CHdbDslCondition CHdbDslField::like(const std::string& value) const
{
    return CHdbDslCondition(*this, HDB_OP_LIKE, HDB_QVT_STRING, value);
}

inline CHdbDslCondition CHdbDslField::isNull() const
{
    return CHdbDslCondition(*this, 0);
}

inline CHdbDslCondition CHdbDslField::isNotNull() const
{
    return CHdbDslCondition(*this, 1);
}

inline CHdbDslCondition CHdbDslField::between(int beginValue, int endValue) const
{
    if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
    {
        return CHdbDslCondition(*this,
            HDB_QVT_INT64,
            HdbDslInt64ToText((HdbInt64)beginValue),
            HdbDslInt64ToText((HdbInt64)endValue));
    }
    return CHdbDslCondition(*this, HDB_QVT_INT32, HdbDslIntToText(beginValue), HdbDslIntToText(endValue));
}

inline CHdbDslCondition CHdbDslField::between(HdbInt64 beginValue, HdbInt64 endValue) const
{
    return CHdbDslCondition(*this, HDB_QVT_INT64, HdbDslInt64ToText(beginValue), HdbDslInt64ToText(endValue));
}

inline CHdbDslCondition CHdbDslField::between(double beginValue, double endValue) const
{
    return CHdbDslCondition(*this, HDB_QVT_DOUBLE, HdbDslDoubleToText(beginValue), HdbDslDoubleToText(endValue));
}

inline CHdbDslCondition CHdbDslField::in(const int* values, int count) const
{
    std::vector<std::string> texts;
    int i;

    for (i = 0; values != NULL && i < count; ++i)
    {
        if (HdbDslIntegerValueTypeForField(*this) == HDB_QVT_INT64)
        {
            texts.push_back(HdbDslInt64ToText((HdbInt64)values[i]));
        }
        else
        {
            texts.push_back(HdbDslIntToText(values[i]));
        }
    }
    return CHdbDslCondition(*this, HdbDslIntegerValueTypeForField(*this), texts);
}

inline CHdbDslCondition CHdbDslField::in(const HdbInt64* values, int count) const
{
    std::vector<std::string> texts;
    int i;

    for (i = 0; values != NULL && i < count; ++i)
    {
        texts.push_back(HdbDslInt64ToText(values[i]));
    }
    return CHdbDslCondition(*this, HDB_QVT_INT64, texts);
}

inline CHdbDslCondition CHdbDslField::in(const double* values, int count) const
{
    std::vector<std::string> texts;
    int i;

    for (i = 0; values != NULL && i < count; ++i)
    {
        texts.push_back(HdbDslDoubleToText(values[i]));
    }
    return CHdbDslCondition(*this, HDB_QVT_DOUBLE, texts);
}

inline CHdbDslCondition CHdbDslField::in(const char* const* values, int count) const
{
    std::vector<std::string> texts;
    int i;

    for (i = 0; values != NULL && i < count; ++i)
    {
        texts.push_back(values[i] != NULL ? values[i] : "");
    }
    return CHdbDslCondition(*this, HDB_QVT_STRING, texts);
}

inline CHdbDslCondition CHdbDslField::in(const CHdbDslValueList& values) const
{
    return CHdbDslCondition(*this, values.ValueType(), values.Values());
}

inline CHdbDslSortField CHdbDslField::asc() const
{
    return CHdbDslSortField(*this, HDB_ORDER_ASC);
}

inline CHdbDslSortField CHdbDslField::desc() const
{
    return CHdbDslSortField(*this, HDB_ORDER_DESC);
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
    : m_kind(HDB_DSL_CONDITION_COMPARE),
      m_left(left),
      m_right(),
      m_op(op),
      m_valueType(valueType),
      m_valueText(valueText),
      m_secondValueText(),
      m_values(),
      m_logic(0),
      m_children()
{
}

inline CHdbDslCondition::CHdbDslCondition(const CHdbDslField& left,
    int op,
    const CHdbDslField& right)
    : m_kind(HDB_DSL_CONDITION_FIELD_COMPARE),
      m_left(left),
      m_right(right),
      m_op(op),
      m_valueType(0),
      m_valueText(),
      m_secondValueText(),
      m_values(),
      m_logic(0),
      m_children()
{
}

inline CHdbDslCondition::CHdbDslCondition(const CHdbDslField& left, int isNotNull)
    : m_kind(HDB_DSL_CONDITION_NULL),
      m_left(left),
      m_right(),
      m_op(isNotNull ? 1 : 0),
      m_valueType(0),
      m_valueText(),
      m_secondValueText(),
      m_values(),
      m_logic(0),
      m_children()
{
}

inline CHdbDslCondition::CHdbDslCondition(const CHdbDslField& left,
    int valueType,
    const std::string& beginText,
    const std::string& endText)
    : m_kind(HDB_DSL_CONDITION_BETWEEN),
      m_left(left),
      m_right(),
      m_op(0),
      m_valueType(valueType),
      m_valueText(beginText),
      m_secondValueText(endText),
      m_values(),
      m_logic(0),
      m_children()
{
}

inline CHdbDslCondition::CHdbDslCondition(const CHdbDslField& left,
    int valueType,
    const std::vector<std::string>& values)
    : m_kind(HDB_DSL_CONDITION_IN),
      m_left(left),
      m_right(),
      m_op(0),
      m_valueType(valueType),
      m_valueText(),
      m_secondValueText(),
      m_values(values),
      m_logic(0),
      m_children()
{
}

inline CHdbDslCondition::CHdbDslCondition(int logic,
    const CHdbDslCondition& left,
    const CHdbDslCondition& right)
    : m_kind(HDB_DSL_CONDITION_GROUP),
      m_left(),
      m_right(),
      m_op(0),
      m_valueType(0),
      m_valueText(),
      m_secondValueText(),
      m_values(),
      m_logic(logic),
      m_children()
{
    m_children.push_back(left);
    m_children.push_back(right);
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
          m_selectsAdded(0),
          m_ordersAdded(0)
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
        m_ordersAdded = 0;
        m_selectedFields.clear();
        m_orderFields.clear();
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
            if (!condition.IsValid() || m_hasWhere)
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

    CHdbDslQuery& orderBy(const CHdbDslSortField& field)
    {
        if (m_error == HDB_OK)
        {
            if (!field.IsValid())
            {
                m_error = HDB_ERR_PARAM;
            }
            else
            {
                m_orderFields.push_back(field);
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
        ret = AddOrdersToQuery();
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

    template <class RowT>
    int fetchInto(std::vector<RowT>& rows)
    {
        CHdbDslResult result;
        int hasRow;
        int ret;

        rows.clear();
        ret = fetch(&result);
        if (ret != HDB_OK)
        {
            return ret;
        }
        while (1)
        {
            RowT row;

            hasRow = 0;
            ret = result.Next(&hasRow);
            if (ret != HDB_OK)
            {
                return ret;
            }
            if (!hasRow)
            {
                break;
            }
            ret = row.FillFrom(result);
            if (ret != HDB_OK)
            {
                return ret;
            }
            rows.push_back(row);
        }
        return HDB_OK;
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

    int ResolveOnField(const CHdbDslField& field,
        const char* targetDatasetName,
        int* outIsTarget,
        HDB_SOURCE* outSource) const
    {
        HDB_SOURCE source;

        if (outIsTarget != NULL)
        {
            *outIsTarget = 0;
        }
        if (outSource != NULL)
        {
            *outSource = NULL;
        }
        if (!field.IsValid() || targetDatasetName == NULL)
        {
            return HDB_ERR_FIELD_REF;
        }
        if (strcmp(field.DatasetName(), targetDatasetName) == 0)
        {
            if (outIsTarget != NULL)
            {
                *outIsTarget = 1;
            }
            return HDB_OK;
        }
        source = FindSource(field.DatasetName());
        if (source == NULL)
        {
            return HDB_ERR_FIELD_REF;
        }
        if (outSource != NULL)
        {
            *outSource = source;
        }
        return HDB_OK;
    }

    int BuildOnBranchAnchors(const CHdbDslCondition& condition,
        const char* targetDatasetName,
        std::vector<int>& branchAnchors,
        HDB_SOURCE* outParentSource) const
    {
        int leftIsTarget;
        int rightIsTarget;
        HDB_SOURCE leftSource;
        HDB_SOURCE rightSource;
        int anchored;
        int ret;
        int i;

        branchAnchors.clear();
        if (!condition.IsValid())
        {
            return HDB_ERR_QUERY_RANGE;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_GROUP)
        {
            std::vector<int> currentAnchors;

            for (i = 0; i < (int)condition.Children().size(); ++i)
            {
                std::vector<int> childAnchors;

                ret = BuildOnBranchAnchors(condition.Children()[i],
                    targetDatasetName,
                    childAnchors,
                    outParentSource);
                if (ret != HDB_OK)
                {
                    return ret;
                }
                if (condition.Logic() == HDB_QCL_OR)
                {
                    branchAnchors.insert(branchAnchors.end(), childAnchors.begin(), childAnchors.end());
                }
                else if (currentAnchors.empty())
                {
                    currentAnchors = childAnchors;
                }
                else
                {
                    std::vector<int> mergedAnchors;
                    int leftIndex;
                    int rightIndex;

                    for (leftIndex = 0; leftIndex < (int)currentAnchors.size(); ++leftIndex)
                    {
                        for (rightIndex = 0; rightIndex < (int)childAnchors.size(); ++rightIndex)
                        {
                            mergedAnchors.push_back(currentAnchors[leftIndex] || childAnchors[rightIndex]);
                        }
                    }
                    currentAnchors = mergedAnchors;
                }
            }
            if (condition.Logic() == HDB_QCL_AND)
            {
                branchAnchors = currentAnchors;
            }
            return branchAnchors.empty() ? HDB_ERR_QUERY_RANGE : HDB_OK;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_NULL ||
            condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_BETWEEN ||
            condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_IN)
        {
            return HDB_ERR_QUERY_RANGE;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_COMPARE)
        {
            ret = ResolveOnField(condition.LeftField(), targetDatasetName, &leftIsTarget, &leftSource);
            if (ret != HDB_OK)
            {
                return ret;
            }
            branchAnchors.push_back(0);
            return HDB_OK;
        }
        if (condition.Kind() != CHdbDslCondition::HDB_DSL_CONDITION_FIELD_COMPARE)
        {
            return HDB_ERR_QUERY_RANGE;
        }
        ret = ResolveOnField(condition.LeftField(), targetDatasetName, &leftIsTarget, &leftSource);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ResolveOnField(condition.RightField(), targetDatasetName, &rightIsTarget, &rightSource);
        if (ret != HDB_OK)
        {
            return ret;
        }
        anchored = 0;
        if (leftIsTarget && rightSource != NULL)
        {
            anchored = 1;
            if (outParentSource != NULL && *outParentSource == NULL)
            {
                *outParentSource = rightSource;
            }
        }
        else if (rightIsTarget && leftSource != NULL)
        {
            anchored = 1;
            if (outParentSource != NULL && *outParentSource == NULL)
            {
                *outParentSource = leftSource;
            }
        }
        branchAnchors.push_back(anchored);
        return HDB_OK;
    }

    int ValidateJoinOnCondition(const CHdbDslCondition& condition,
        const char* targetDatasetName,
        HDB_SOURCE* outParentSource) const
    {
        std::vector<int> branchAnchors;
        int ret;
        int i;

        if (outParentSource != NULL)
        {
            *outParentSource = NULL;
        }
        ret = BuildOnBranchAnchors(condition, targetDatasetName, branchAnchors, outParentSource);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (outParentSource == NULL || *outParentSource == NULL)
        {
            return HDB_ERR_QUERY_RANGE;
        }
        for (i = 0; i < (int)branchAnchors.size(); ++i)
        {
            if (!branchAnchors[i])
            {
                return HDB_ERR_QUERY_RANGE;
            }
        }
        return HDB_OK;
    }

    int AddJoinOn(const CHdbDslTable& table, int joinType, const CHdbDslCondition& condition)
    {
        HDB_SOURCE parentSource;
        HDB_SOURCE targetSource;
        int conditionId;
        int ret;

        if (m_error != HDB_OK)
        {
            return m_error;
        }
        if (!m_hasRoot || !condition.IsValid() || FindSource(table.DatasetName()) != NULL)
        {
            m_error = HDB_ERR_QUERY_RANGE;
            return m_error;
        }
        parentSource = NULL;
        ret = ValidateJoinOnCondition(condition, table.DatasetName(), &parentSource);
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        ret = EnsureQuery();
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        targetSource = NULL;
        ret = HdbQueryJoin(m_query, parentSource, table.DatasetName(), joinType, &targetSource);
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        AddSource(table.DatasetName(), targetSource);
        conditionId = -1;
        ret = AddConditionToQuery(condition, &conditionId);
        if (ret == HDB_OK)
        {
            ret = HdbQueryJoinOnCondition(m_query, targetSource, conditionId);
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
        int conditionId;
        int ret;

        if (!m_hasWhere)
        {
            return HDB_OK;
        }
        conditionId = -1;
        ret = AddConditionToQuery(m_where, &conditionId);
        if (ret != HDB_OK)
        {
            return ret;
        }
        return HdbQueryWhereCondition(m_query, conditionId);
    }

    int AddConditionToQuery(const CHdbDslCondition& condition, int* outConditionId)
    {
        HDB_SOURCE source;
        std::vector<const char*> valueTexts;
        std::vector<int> childIds;
        int ret;
        int i;

        if (outConditionId != NULL)
        {
            *outConditionId = -1;
        }
        if (outConditionId == NULL || !condition.IsValid())
        {
            return HDB_ERR_QUERY_RANGE;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_GROUP)
        {
            for (i = 0; i < (int)condition.Children().size(); ++i)
            {
                int childId;

                childId = -1;
                ret = AddConditionToQuery(condition.Children()[i], &childId);
                if (ret != HDB_OK)
                {
                    return ret;
                }
                childIds.push_back(childId);
            }
            return HdbQueryConditionGroup(m_query,
                condition.Logic(),
                childIds.empty() ? NULL : &childIds[0],
                (int)childIds.size(),
                outConditionId);
        }
        source = FindSource(condition.LeftField().DatasetName());
        if (source == NULL)
        {
            return HDB_ERR_FIELD_REF;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_COMPARE)
        {
            return HdbQueryConditionValue(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.Op(),
                condition.ValueType(),
                condition.ValueText(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_FIELD_COMPARE)
        {
            HDB_SOURCE rightSource;

            rightSource = FindSource(condition.RightField().DatasetName());
            if (rightSource == NULL)
            {
                return HDB_ERR_FIELD_REF;
            }
            return HdbQueryConditionField(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.Op(),
                rightSource,
                condition.RightField().FieldName(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_NULL)
        {
            return HdbQueryConditionNull(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.Op(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_BETWEEN)
        {
            return HdbQueryConditionBetween(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.ValueType(),
                condition.ValueText(),
                condition.SecondValueText(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_IN)
        {
            for (i = 0; i < (int)condition.Values().size(); ++i)
            {
                valueTexts.push_back(condition.Values()[i].c_str());
            }
            return HdbQueryConditionIn(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.ValueType(),
                valueTexts.empty() ? NULL : &valueTexts[0],
                (int)valueTexts.size(),
                outConditionId);
        }
        return HDB_ERR_QUERY_RANGE;
    }

    int AddOrdersToQuery()
    {
        int i;

        if (m_ordersAdded)
        {
            return HDB_OK;
        }
        for (i = 0; i < (int)m_orderFields.size(); ++i)
        {
            HDB_SOURCE source;

            source = FindSource(m_orderFields[i].Field().DatasetName());
            if (source == NULL)
            {
                return HDB_ERR_FIELD_REF;
            }
            if (HdbQueryOrderBy(m_query,
                source,
                m_orderFields[i].Field().FieldName(),
                m_orderFields[i].OrderType()) != HDB_OK)
            {
                return HDB_ERR_FIELD_REF;
            }
        }
        m_ordersAdded = 1;
        return HDB_OK;
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
    int m_ordersAdded;
    std::vector<HdbDslSelectedField> m_selectedFields;
    std::vector<CHdbDslSortField> m_orderFields;
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

class CHdbDslDmlCore;
class CHdbDslInsert;
class CHdbDslUpdateStart;
class CHdbDslUpdateNeedWhere;
class CHdbDslDeleteNeedWhere;
class CHdbDslDmlExecutable;

class CHdbDslDmlCore
{
public:
    ~CHdbDslDmlCore()
    {
        FreeQuery();
    }

private:
    explicit CHdbDslDmlCore(HDB_SESSION session)
        : m_session(session),
          m_query(NULL),
          m_rootSource(NULL),
          m_datasetName(""),
          m_error(HDB_OK),
          m_hasWhere(0),
          m_whereAdded(0),
          m_where()
    {
    }

    CHdbDslDmlCore& Reset(const CHdbDslTable& table, int statementType)
    {
        FreeQuery();
        m_rootSource = NULL;
        m_datasetName = table.DatasetName();
        m_error = HDB_OK;
        m_hasWhere = 0;
        m_whereAdded = 0;
        m_where = CHdbDslCondition();
        if (m_session == NULL)
        {
            m_error = HDB_ERR_PARAM;
            return *this;
        }
        m_error = HdbQueryCreate(m_session, &m_query);
        if (m_error == HDB_OK)
        {
            m_error = HdbQuerySetStatementType(m_query, statementType);
        }
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryFrom(m_query, table.DatasetName(), &m_rootSource);
        }
        return *this;
    }

    CHdbDslDmlCore& Set(const CHdbDslField& field, int value)
    {
        if (HdbDslIntegerValueTypeForField(field) == HDB_QVT_INT64)
        {
            return SetValue(field, HDB_QVT_INT64, HdbDslInt64ToText((HdbInt64)value));
        }
        return SetValue(field, HDB_QVT_INT32, HdbDslIntToText(value));
    }

    CHdbDslDmlCore& Set(const CHdbDslField& field, HdbInt64 value)
    {
        return SetValue(field, HDB_QVT_INT64, HdbDslInt64ToText(value));
    }

    CHdbDslDmlCore& Set(const CHdbDslField& field, double value)
    {
        return SetValue(field, HDB_QVT_DOUBLE, HdbDslDoubleToText(value));
    }

    CHdbDslDmlCore& Set(const CHdbDslField& field, const char* value)
    {
        return SetValue(field, HDB_QVT_STRING, value != NULL ? value : "");
    }

    CHdbDslDmlCore& Set(const CHdbDslField& field, const std::string& value)
    {
        return SetValue(field, HDB_QVT_STRING, value);
    }

    CHdbDslDmlCore& Where(const CHdbDslCondition& condition)
    {
        if (m_error == HDB_OK)
        {
            if (!condition.IsValid() || m_hasWhere)
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

    CHdbDslDmlCore& TimeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        if (m_error == HDB_OK)
        {
            m_error = HdbQueryTimeRange(m_query, beginMs, endMs);
        }
        return *this;
    }

    int Execute(int* affectedRows)
    {
        int ret;

        if (affectedRows != NULL)
        {
            *affectedRows = 0;
        }
        if (affectedRows == NULL)
        {
            return HDB_ERR_PARAM;
        }
        if (m_error != HDB_OK)
        {
            return m_error;
        }
        ret = AddWhereToQuery();
        if (ret != HDB_OK)
        {
            m_error = ret;
            return ret;
        }
        ret = HdbQueryExecuteAffected(m_query, affectedRows);
        m_error = ret;
        return ret;
    }

    int GetError() const
    {
        return m_error;
    }

    CHdbDslDmlCore& SetValue(const CHdbDslField& field, int valueType, const std::string& valueText)
    {
        if (m_error == HDB_OK)
        {
            if (!field.IsValid() || strcmp(field.DatasetName(), m_datasetName) != 0)
            {
                m_error = HDB_ERR_FIELD_REF;
            }
            else
            {
                m_error = HdbQuerySetValue(m_query,
                    m_rootSource,
                    field.FieldName(),
                    valueType,
                    valueText.c_str());
            }
        }
        return *this;
    }

    void FreeQuery()
    {
        if (m_query != NULL)
        {
            HdbQueryFree(m_query);
            m_query = NULL;
        }
    }

    HDB_SOURCE FindSource(const char* datasetName) const
    {
        if (datasetName != NULL && strcmp(datasetName, m_datasetName) == 0)
        {
            return m_rootSource;
        }
        return NULL;
    }

    int AddWhereToQuery()
    {
        int conditionId;
        int ret;

        if (!m_hasWhere || m_whereAdded)
        {
            return HDB_OK;
        }
        conditionId = -1;
        ret = AddConditionToQuery(m_where, &conditionId);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = HdbQueryWhereCondition(m_query, conditionId);
        if (ret == HDB_OK)
        {
            m_whereAdded = 1;
        }
        return ret;
    }

    int AddConditionToQuery(const CHdbDslCondition& condition, int* outConditionId)
    {
        HDB_SOURCE source;
        std::vector<const char*> valueTexts;
        std::vector<int> childIds;
        int ret;
        int i;

        if (outConditionId != NULL)
        {
            *outConditionId = -1;
        }
        if (outConditionId == NULL || !condition.IsValid())
        {
            return HDB_ERR_QUERY_RANGE;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_GROUP)
        {
            for (i = 0; i < (int)condition.Children().size(); ++i)
            {
                int childId;

                childId = -1;
                ret = AddConditionToQuery(condition.Children()[i], &childId);
                if (ret != HDB_OK)
                {
                    return ret;
                }
                childIds.push_back(childId);
            }
            return HdbQueryConditionGroup(m_query,
                condition.Logic(),
                childIds.empty() ? NULL : &childIds[0],
                (int)childIds.size(),
                outConditionId);
        }
        source = FindSource(condition.LeftField().DatasetName());
        if (source == NULL)
        {
            return HDB_ERR_FIELD_REF;
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_COMPARE)
        {
            return HdbQueryConditionValue(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.Op(),
                condition.ValueType(),
                condition.ValueText(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_FIELD_COMPARE)
        {
            HDB_SOURCE rightSource;

            rightSource = FindSource(condition.RightField().DatasetName());
            if (rightSource == NULL)
            {
                return HDB_ERR_FIELD_REF;
            }
            return HdbQueryConditionField(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.Op(),
                rightSource,
                condition.RightField().FieldName(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_NULL)
        {
            return HdbQueryConditionNull(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.Op(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_BETWEEN)
        {
            return HdbQueryConditionBetween(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.ValueType(),
                condition.ValueText(),
                condition.SecondValueText(),
                outConditionId);
        }
        if (condition.Kind() == CHdbDslCondition::HDB_DSL_CONDITION_IN)
        {
            for (i = 0; i < (int)condition.Values().size(); ++i)
            {
                valueTexts.push_back(condition.Values()[i].c_str());
            }
            return HdbQueryConditionIn(m_query,
                source,
                condition.LeftField().FieldName(),
                condition.ValueType(),
                valueTexts.empty() ? NULL : &valueTexts[0],
                (int)valueTexts.size(),
                outConditionId);
        }
        return HDB_ERR_QUERY_RANGE;
    }

private:
    CHdbDslDmlCore(const CHdbDslDmlCore&);
    CHdbDslDmlCore& operator=(const CHdbDslDmlCore&);

private:
    HDB_SESSION m_session;
    HDB_QUERY m_query;
    HDB_SOURCE m_rootSource;
    const char* m_datasetName;
    int m_error;
    int m_hasWhere;
    int m_whereAdded;
    CHdbDslCondition m_where;

    friend class CHdbCreate;
    friend class CHdbDslInsert;
    friend class CHdbDslUpdateStart;
    friend class CHdbDslUpdateNeedWhere;
    friend class CHdbDslDeleteNeedWhere;
    friend class CHdbDslDmlExecutable;
};

class CHdbDslDmlExecutable
{
public:
    CHdbDslDmlExecutable timeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        m_dml->TimeRange(beginMs, endMs);
        return *this;
    }

    int execute(int* affectedRows)
    {
        return m_dml->Execute(affectedRows);
    }

    int GetError() const
    {
        return m_dml->GetError();
    }

private:
    explicit CHdbDslDmlExecutable(CHdbDslDmlCore* dml)
        : m_dml(dml)
    {
    }

private:
    CHdbDslDmlCore* m_dml;

    friend class CHdbDslUpdateNeedWhere;
    friend class CHdbDslDeleteNeedWhere;
};

class CHdbDslInsert
{
public:
    CHdbDslInsert set(const CHdbDslField& field, int value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslInsert set(const CHdbDslField& field, HdbInt64 value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslInsert set(const CHdbDslField& field, double value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslInsert set(const CHdbDslField& field, const char* value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslInsert set(const CHdbDslField& field, const std::string& value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslInsert timeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        m_dml->TimeRange(beginMs, endMs);
        return *this;
    }

    int execute(int* affectedRows)
    {
        return m_dml->Execute(affectedRows);
    }

    int GetError() const
    {
        return m_dml->GetError();
    }

private:
    explicit CHdbDslInsert(CHdbDslDmlCore* dml)
        : m_dml(dml)
    {
    }

private:
    CHdbDslDmlCore* m_dml;

    friend class CHdbCreate;
};

class CHdbDslUpdateNeedWhere
{
public:
    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, int value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, HdbInt64 value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, double value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, const char* value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, const std::string& value)
    {
        m_dml->Set(field, value);
        return *this;
    }

    CHdbDslUpdateNeedWhere timeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        m_dml->TimeRange(beginMs, endMs);
        return *this;
    }

    CHdbDslDmlExecutable where(const CHdbDslCondition& condition)
    {
        m_dml->Where(condition);
        return CHdbDslDmlExecutable(m_dml);
    }

    int GetError() const
    {
        return m_dml->GetError();
    }

private:
    explicit CHdbDslUpdateNeedWhere(CHdbDslDmlCore* dml)
        : m_dml(dml)
    {
    }

private:
    CHdbDslDmlCore* m_dml;

    friend class CHdbDslUpdateStart;
};

class CHdbDslUpdateStart
{
public:
    CHdbDslUpdateStart timeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        m_dml->TimeRange(beginMs, endMs);
        return *this;
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, int value)
    {
        m_dml->Set(field, value);
        return CHdbDslUpdateNeedWhere(m_dml);
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, HdbInt64 value)
    {
        m_dml->Set(field, value);
        return CHdbDslUpdateNeedWhere(m_dml);
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, double value)
    {
        m_dml->Set(field, value);
        return CHdbDslUpdateNeedWhere(m_dml);
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, const char* value)
    {
        m_dml->Set(field, value);
        return CHdbDslUpdateNeedWhere(m_dml);
    }

    CHdbDslUpdateNeedWhere set(const CHdbDslField& field, const std::string& value)
    {
        m_dml->Set(field, value);
        return CHdbDslUpdateNeedWhere(m_dml);
    }

    int GetError() const
    {
        return m_dml->GetError();
    }

private:
    explicit CHdbDslUpdateStart(CHdbDslDmlCore* dml)
        : m_dml(dml)
    {
    }

private:
    CHdbDslDmlCore* m_dml;

    friend class CHdbCreate;
};

class CHdbDslDeleteNeedWhere
{
public:
    CHdbDslDeleteNeedWhere timeRange(HdbInt64 beginMs, HdbInt64 endMs)
    {
        m_dml->TimeRange(beginMs, endMs);
        return *this;
    }

    CHdbDslDmlExecutable where(const CHdbDslCondition& condition)
    {
        m_dml->Where(condition);
        return CHdbDslDmlExecutable(m_dml);
    }

    int GetError() const
    {
        return m_dml->GetError();
    }

private:
    explicit CHdbDslDeleteNeedWhere(CHdbDslDmlCore* dml)
        : m_dml(dml)
    {
    }

private:
    CHdbDslDmlCore* m_dml;

    friend class CHdbCreate;
};

class CHdbCreate
{
public:
    explicit CHdbCreate(HDB_SESSION session)
        : m_query(session),
          m_dml(session)
    {
    }

    CHdbDslQuery& select(const CHdbDslField& field)
    {
        m_query.Reset();
        return m_query.select(field);
    }

    CHdbDslInsert insertInto(const CHdbDslTable& table)
    {
        m_dml.Reset(table, HDB_QST_INSERT);
        return CHdbDslInsert(&m_dml);
    }

    CHdbDslUpdateStart update(const CHdbDslTable& table)
    {
        m_dml.Reset(table, HDB_QST_UPDATE);
        return CHdbDslUpdateStart(&m_dml);
    }

    CHdbDslDeleteNeedWhere deleteFrom(const CHdbDslTable& table)
    {
        m_dml.Reset(table, HDB_QST_DELETE);
        return CHdbDslDeleteNeedWhere(&m_dml);
    }

private:
    CHdbCreate(const CHdbCreate&);
    CHdbCreate& operator=(const CHdbCreate&);

private:
    CHdbDslQuery m_query;
    CHdbDslDmlCore m_dml;
};

#endif
