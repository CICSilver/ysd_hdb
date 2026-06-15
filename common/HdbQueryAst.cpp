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

CHdbQueryAst::CHdbQueryAst()
{
    Clear();
}

void CHdbQueryAst::Clear()
{
    rootDataset.clear();
    hasTimeRange = 0;
    beginMs = 0;
    endMs = 0;
    selects.clear();
    wheres.clear();
    orders.clear();
    limit = 0;
    offset = 0;
}

int CHdbQueryAst::SetRootDataset(const char* datasetName)
{
    if (HdbQueryTextEmpty(datasetName))
    {
        return -1;
    }
    rootDataset = datasetName;
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

int CHdbQueryAst::AddSelect(const char* fieldPath, const char* outputName)
{
    HdbQuerySelectItem item;

    if (HdbQueryTextEmpty(fieldPath) || HdbQueryTextEmpty(outputName))
    {
        return -1;
    }
    item.fieldPath = fieldPath;
    item.outputName = outputName;
    selects.push_back(item);
    return 0;
}

int CHdbQueryAst::AddWhereInt32(const char* fieldPath, int op, int value)
{
    char buffer[32];
    HDB_QUERY_SNPRINTF(buffer, sizeof(buffer), "%d", value);
    buffer[sizeof(buffer) - 1] = '\0';
    return AddWhereText(fieldPath, op, HDB_QVT_INT32, buffer);
}

int CHdbQueryAst::AddWhereInt64(const char* fieldPath, int op, HdbQueryInt64 value)
{
    return AddWhereText(fieldPath, op, HDB_QVT_INT64, HdbQueryInt64ToText(value));
}

int CHdbQueryAst::AddWhereDouble(const char* fieldPath, int op, double value)
{
    return AddWhereText(fieldPath, op, HDB_QVT_DOUBLE, HdbQueryDoubleToText(value));
}

int CHdbQueryAst::AddWhereString(const char* fieldPath, int op, const char* value)
{
    if (value == NULL)
    {
        return -1;
    }
    return AddWhereText(fieldPath, op, HDB_QVT_STRING, value);
}

int CHdbQueryAst::AddOrder(const char* fieldPath, int orderType)
{
    HdbQueryOrderItem item;

    if (HdbQueryTextEmpty(fieldPath))
    {
        return -1;
    }
    item.fieldPath = fieldPath;
    item.orderType = orderType;
    orders.push_back(item);
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

int CHdbQueryAst::AddWhereText(const char* fieldPath, int op, int valueType, const std::string& valueText)
{
    HdbQueryWhereItem item;

    if (HdbQueryTextEmpty(fieldPath))
    {
        return -1;
    }
    item.fieldPath = fieldPath;
    item.op = op;
    item.valueType = valueType;
    item.valueText = valueText;
    wheres.push_back(item);
    return 0;
}
