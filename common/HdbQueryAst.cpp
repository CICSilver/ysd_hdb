#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "HdbQueryAst.h"

#include <stdio.h>
#include <stdlib.h>

#include <sstream>

#ifdef _WIN32
#define HDB_QUERY_SNPRINTF _snprintf
#else
#define HDB_QUERY_SNPRINTF snprintf
#endif

static HdbQueryInt64 HdbQueryTextToInt64(const char* text)
{
    if (text == NULL)
    {
        return 0;
    }
#ifdef _WIN32
    return (HdbQueryInt64)_strtoi64(text, NULL, 10);
#else
    return (HdbQueryInt64)strtoll(text, NULL, 10);
#endif
}

static int HdbQueryTextEmpty(const char* text)
{
    return text == NULL || text[0] == '\0';
}

static int HdbQueryContainsUnsafeChar(const std::string& text)
{
    size_t i;
    for (i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\n' || text[i] == '\r' || text[i] == '|')
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
    size_t i;
    std::ostringstream out;

    if (rootDataset.empty() || HdbQueryContainsUnsafeChar(rootDataset))
    {
        return -1;
    }
    out << "root=" << rootDataset << "\n";
    if (hasTimeRange)
    {
        out << "time=" << beginMs << "," << endMs << "\n";
    }
    for (i = 0; i < selects.size(); ++i)
    {
        if (HdbQueryContainsUnsafeChar(selects[i].fieldPath) ||
            HdbQueryContainsUnsafeChar(selects[i].outputName))
        {
            return -1;
        }
        out << "select=" << selects[i].fieldPath << "|" << selects[i].outputName << "\n";
    }
    for (i = 0; i < wheres.size(); ++i)
    {
        if (HdbQueryContainsUnsafeChar(wheres[i].fieldPath) ||
            HdbQueryContainsUnsafeChar(wheres[i].valueText))
        {
            return -1;
        }
        out << "where=" << wheres[i].fieldPath << "|"
            << wheres[i].op << "|"
            << wheres[i].valueType << "|"
            << wheres[i].valueText << "\n";
    }
    for (i = 0; i < orders.size(); ++i)
    {
        if (HdbQueryContainsUnsafeChar(orders[i].fieldPath))
        {
            return -1;
        }
        out << "order=" << orders[i].fieldPath << "|" << orders[i].orderType << "\n";
    }
    out << "limit=" << limit << "," << offset << "\n";
    text = out.str();
    return 0;
}

int CHdbQueryAst::Deserialize(const char* text)
{
    std::string all;
    std::string line;
    size_t pos;
    size_t next;

    if (text == NULL)
    {
        return -1;
    }
    Clear();
    all = text;
    pos = 0;
    while (pos <= all.size())
    {
        next = all.find('\n', pos);
        if (next == std::string::npos)
        {
            line = all.substr(pos);
            pos = all.size() + 1;
        }
        else
        {
            line = all.substr(pos, next - pos);
            pos = next + 1;
        }
        if (line.empty())
        {
            continue;
        }
        if (line.find("root=") == 0)
        {
            if (SetRootDataset(line.substr(5).c_str()) != 0)
            {
                return -1;
            }
        }
        else if (line.find("time=") == 0)
        {
            size_t comma = line.find(',', 5);
            if (comma == std::string::npos)
            {
                return -1;
            }
            if (SetTimeRange(HdbQueryTextToInt64(line.substr(5, comma - 5).c_str()),
                HdbQueryTextToInt64(line.substr(comma + 1).c_str())) != 0)
            {
                return -1;
            }
        }
        else if (line.find("select=") == 0)
        {
            size_t sep = line.find('|', 7);
            if (sep == std::string::npos ||
                AddSelect(line.substr(7, sep - 7).c_str(), line.substr(sep + 1).c_str()) != 0)
            {
                return -1;
            }
        }
        else if (line.find("where=") == 0)
        {
            size_t p1 = line.find('|', 6);
            size_t p2 = p1 == std::string::npos ? std::string::npos : line.find('|', p1 + 1);
            size_t p3 = p2 == std::string::npos ? std::string::npos : line.find('|', p2 + 1);
            if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
            {
                return -1;
            }
            if (AddWhereText(line.substr(6, p1 - 6).c_str(),
                atoi(line.substr(p1 + 1, p2 - p1 - 1).c_str()),
                atoi(line.substr(p2 + 1, p3 - p2 - 1).c_str()),
                line.substr(p3 + 1)) != 0)
            {
                return -1;
            }
        }
        else if (line.find("order=") == 0)
        {
            size_t sep = line.find('|', 6);
            if (sep == std::string::npos ||
                AddOrder(line.substr(6, sep - 6).c_str(), atoi(line.substr(sep + 1).c_str())) != 0)
            {
                return -1;
            }
        }
        else if (line.find("limit=") == 0)
        {
            size_t comma = line.find(',', 6);
            if (comma == std::string::npos ||
                SetLimit(atoi(line.substr(6, comma - 6).c_str()), atoi(line.substr(comma + 1).c_str())) != 0)
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
    return rootDataset.empty() ? -1 : 0;
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
