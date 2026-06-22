#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "HdbQueryAst.h"
#include "HdbQueryAstCodec.h"

#include <stdio.h>
#include <sstream>

#ifdef _WIN32
#define HDB_QUERY_SNPRINTF _snprintf
#else
#define HDB_QUERY_SNPRINTF snprintf
#endif

static int HdbQueryTextEmpty(const char* text)
{
    return text == NULL || text[0] == '\0';
}

static int HdbQueryTextContainsDot(const char* text)
{
    int i;

    if (text == NULL)
    {
        return 0;
    }
    for (i = 0; text[i] != '\0'; ++i)
    {
        if (text[i] == '.')
        {
            return 1;
        }
    }
    return 0;
}

static std::string HdbQueryInt64ToText(HdbQueryInt64 value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

static std::string HdbQueryDoubleToText(double value)
{
    char buffer[64];
    HDB_QUERY_SNPRINTF(buffer, sizeof(buffer), "%.17g", value);
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
}

static int HdbQueryIsValidJoinType(int joinType)
{
    return joinType == HDB_JOIN_INNER || joinType == HDB_JOIN_LEFT;
}

CHdbQueryAst::CHdbQueryAst()
{
    Clear();
}

void CHdbQueryAst::Clear()
{
    sources.clear();
    hasTimeRange = 0;
    beginMs = 0;
    endMs = 0;
    selects.clear();
    wheres.clear();
    orders.clear();
    limit = 0;
    offset = 0;
}

int CHdbQueryAst::AddRootSource(const char* datasetName, int* outSourceId)
{
    HdbQuerySourceItem item;

    if (outSourceId != NULL)
    {
        *outSourceId = -1;
    }
    if (HdbQueryTextEmpty(datasetName) || HdbQueryTextContainsDot(datasetName))
    {
        return -1;
    }
    if (!sources.empty())
    {
        return -1;
    }
    item.sourceId = 0;
    item.sourceType = HDB_SOURCE_ROOT;
    item.parentSourceId = -1;
    item.datasetName = datasetName;
    item.associationName.clear();
    item.joinType = 0;
    sources.push_back(item);
    if (outSourceId != NULL)
    {
        *outSourceId = item.sourceId;
    }
    return 0;
}

int CHdbQueryAst::AddJoinSource(int parentSourceId, const char* associationName, int joinType, int* outSourceId)
{
    HdbQuerySourceItem item;

    if (outSourceId != NULL)
    {
        *outSourceId = -1;
    }
    if (HdbQueryTextEmpty(associationName) || HdbQueryTextContainsDot(associationName))
    {
        return -1;
    }
    if (!HdbQueryIsValidJoinType(joinType) ||
        FindSourceIndex(parentSourceId) < 0 ||
        sources.size() >= HDB_QUERY_MAX_SOURCE_COUNT)
    {
        return -1;
    }
    item.sourceId = (int)sources.size();
    item.sourceType = HDB_SOURCE_JOIN;
    item.parentSourceId = parentSourceId;
    item.datasetName.clear();
    item.associationName = associationName;
    item.joinType = joinType;
    sources.push_back(item);
    if (outSourceId != NULL)
    {
        *outSourceId = item.sourceId;
    }
    return 0;
}

int CHdbQueryAst::AddSelect(int sourceId, const char* fieldName, const char* outputName)
{
    HdbQuerySelectItem item;

    if (HdbQueryTextEmpty(outputName))
    {
        return -1;
    }
    if (AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.outputName = outputName;
    selects.push_back(item);
    return 0;
}

int CHdbQueryAst::AddWhereInt32(int sourceId, const char* fieldName, int op, int value)
{
    char buffer[32];
    HDB_QUERY_SNPRINTF(buffer, sizeof(buffer), "%d", value);
    buffer[sizeof(buffer) - 1] = '\0';
    return AddWhereText(sourceId, fieldName, op, HDB_QVT_INT32, buffer);
}

int CHdbQueryAst::AddWhereInt64(int sourceId, const char* fieldName, int op, HdbQueryInt64 value)
{
    return AddWhereText(sourceId, fieldName, op, HDB_QVT_INT64, HdbQueryInt64ToText(value));
}

int CHdbQueryAst::AddWhereDouble(int sourceId, const char* fieldName, int op, double value)
{
    return AddWhereText(sourceId, fieldName, op, HDB_QVT_DOUBLE, HdbQueryDoubleToText(value));
}

int CHdbQueryAst::AddWhereString(int sourceId, const char* fieldName, int op, const char* value)
{
    if (value == NULL)
    {
        return -1;
    }
    return AddWhereText(sourceId, fieldName, op, HDB_QVT_STRING, value);
}

int CHdbQueryAst::AddOrder(int sourceId, const char* fieldName, int orderType)
{
    HdbQueryOrderItem item;

    if (AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.orderType = orderType;
    orders.push_back(item);
    return 0;
}

int CHdbQueryAst::SetTimeRange(HdbQueryInt64 beginValue, HdbQueryInt64 endValue)
{
    if (beginValue >= endValue)
    {
        return -1;
    }
    hasTimeRange = 1;
    beginMs = beginValue;
    endMs = endValue;
    return 0;
}

int CHdbQueryAst::SetLimit(int limitValue, int offsetValue)
{
    if (limitValue < 0 || offsetValue < 0)
    {
        return -1;
    }
    limit = limitValue;
    offset = offsetValue;
    return 0;
}

int CHdbQueryAst::HasRootSource() const
{
    return !sources.empty() &&
        sources[0].sourceId == 0 &&
        sources[0].sourceType == HDB_SOURCE_ROOT ? 1 : 0;
}

int CHdbQueryAst::FindSourceIndex(int sourceId) const
{
    size_t i;

    for (i = 0; i < sources.size(); ++i)
    {
        if (sources[i].sourceId == sourceId)
        {
            return (int)i;
        }
    }
    return -1;
}

int CHdbQueryAst::Serialize(std::string& text) const
{
    CHdbQueryAstCodec codec;
    return codec.Encode(*this, text);
}

int CHdbQueryAst::Deserialize(const char* text)
{
    CHdbQueryAstCodec codec;
    return codec.Decode(text, *this);
}

int CHdbQueryAst::AddWhereText(int sourceId,
    const char* fieldName,
    int op,
    int valueType,
    const std::string& valueText)
{
    HdbQueryWhereItem item;

    if (AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.op = op;
    item.valueType = valueType;
    item.valueText = valueText;
    wheres.push_back(item);
    return 0;
}

int CHdbQueryAst::AssignFieldRef(HdbQueryFieldRef& field, int sourceId, const char* fieldName) const
{
    if (HdbQueryTextEmpty(fieldName) ||
        HdbQueryTextContainsDot(fieldName) ||
        FindSourceIndex(sourceId) < 0)
    {
        return -1;
    }
    field.sourceId = sourceId;
    field.fieldName = fieldName;
    return 0;
}
