#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "HdbQueryAstCodec.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include <sstream>

static int HdbQueryCodecTextEmpty(const char* text)
{
    return text == NULL || text[0] == '\0';
}

static int HdbQueryCodecContainsUnsafeChar(const std::string& text)
{
    size_t index;

    for (index = 0; index < text.size(); ++index)
    {
        if (text[index] == '\n' || text[index] == '\r' || text[index] == '|')
        {
            return 1;
        }
    }
    return 0;
}

CHdbQueryAstCodec::CHdbQueryAstCodec()
{
}

int CHdbQueryAstCodec::Encode(const CHdbQueryAst& ast, std::string& outText)
{
    std::ostringstream out;
    size_t index;
    int ret;

    outText.clear();
    ret = ValidateText(ast.rootDataset, "root dataset");
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (ast.selects.size() > HDB_QUERY_MAX_SELECT_COUNT ||
        ast.wheres.size() > HDB_QUERY_MAX_WHERE_COUNT ||
        ast.orders.size() > HDB_QUERY_MAX_ORDER_COUNT)
    {
        SetLastError("query ast item count exceeds limit");
        return HDB_ERR_QUERY_RANGE;
    }
    out << "ast_version=" << HDB_QUERY_AST_VERSION << "\n";
    out << "root=" << ast.rootDataset << "\n";
    if (ast.hasTimeRange)
    {
        if (ast.beginMs >= ast.endMs)
        {
            SetLastError("invalid query time range");
            return HDB_ERR_QUERY_RANGE;
        }
        out << "time=" << ast.beginMs << "," << ast.endMs << "\n";
    }
    for (index = 0; index < ast.selects.size(); ++index)
    {
        ret = ValidateText(ast.selects[index].fieldPath, "select field path");
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateText(ast.selects[index].outputName, "select output name");
        if (ret != HDB_OK)
        {
            return ret;
        }
        out << "select=" << ast.selects[index].fieldPath << "|" << ast.selects[index].outputName << "\n";
    }
    for (index = 0; index < ast.wheres.size(); ++index)
    {
        ret = ValidateText(ast.wheres[index].fieldPath, "where field path");
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateCompareOp(ast.wheres[index].op);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateValueType(ast.wheres[index].valueType);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateText(ast.wheres[index].valueText, "where value");
        if (ret != HDB_OK)
        {
            return ret;
        }
        out << "where=" << ast.wheres[index].fieldPath << "|"
            << ast.wheres[index].op << "|"
            << ast.wheres[index].valueType << "|"
            << ast.wheres[index].valueText << "\n";
    }
    for (index = 0; index < ast.orders.size(); ++index)
    {
        ret = ValidateText(ast.orders[index].fieldPath, "order field path");
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateOrderType(ast.orders[index].orderType);
        if (ret != HDB_OK)
        {
            return ret;
        }
        out << "order=" << ast.orders[index].fieldPath << "|" << ast.orders[index].orderType << "\n";
    }
    if (ast.limit < 0 || ast.limit > HDB_QUERY_MAX_LIMIT || ast.offset < 0)
    {
        SetLastError("invalid query limit or offset");
        return HDB_ERR_QUERY_RANGE;
    }
    out << "limit=" << ast.limit << "," << ast.offset << "\n";
    outText = out.str();
    if (outText.size() > HDB_QUERY_AST_MAX_BYTES)
    {
        outText.clear();
        SetLastError("query ast bytes exceeds limit");
        return HDB_ERR_BUFFER;
    }
    return HDB_OK;
}

int CHdbQueryAstCodec::Decode(const char* text, CHdbQueryAst& outAst)
{
    std::string all;
    std::string line;
    size_t pos;
    size_t next;
    int seenVersion;

    if (text == NULL)
    {
        SetLastError("query ast text is null");
        return HDB_ERR_PARAM;
    }
    all = text;
    if (all.size() > HDB_QUERY_AST_MAX_BYTES)
    {
        SetLastError("query ast bytes exceeds limit");
        return HDB_ERR_BUFFER;
    }
    outAst.Clear();
    seenVersion = 0;
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
        if (line.find("ast_version=") == 0)
        {
            int version;
            if (ParseInt32Strict(line.substr(12), &version) != HDB_OK || version != HDB_QUERY_AST_VERSION)
            {
                SetLastError("unsupported query ast version");
                return HDB_ERR_PARAM;
            }
            seenVersion = 1;
        }
        else if (!seenVersion)
        {
            SetLastError("query ast version is missing");
            return HDB_ERR_PARAM;
        }
        else if (line.find("root=") == 0)
        {
            std::string value = line.substr(5);
            if (ValidateText(value, "root dataset") != HDB_OK || outAst.SetRootDataset(value.c_str()) != 0)
            {
                return HDB_ERR_PARAM;
            }
        }
        else if (line.find("time=") == 0)
        {
            HdbQueryInt64 beginMs;
            HdbQueryInt64 endMs;
            if (ParsePairInt64(line.substr(5), &beginMs, &endMs) != HDB_OK ||
                outAst.SetTimeRange(beginMs, endMs) != 0)
            {
                SetLastError("invalid query time range");
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else if (line.find("select=") == 0)
        {
            size_t sep = line.find('|', 7);
            std::string fieldPath;
            std::string outputName;
            if (sep == std::string::npos ||
                outAst.selects.size() >= HDB_QUERY_MAX_SELECT_COUNT)
            {
                SetLastError("invalid select item");
                return HDB_ERR_PARAM;
            }
            fieldPath = line.substr(7, sep - 7);
            outputName = line.substr(sep + 1);
            if (ValidateText(fieldPath, "select field path") != HDB_OK ||
                ValidateText(outputName, "select output name") != HDB_OK ||
                outAst.AddSelect(fieldPath.c_str(), outputName.c_str()) != 0)
            {
                SetLastError("invalid select item");
                return HDB_ERR_PARAM;
            }
        }
        else if (line.find("where=") == 0)
        {
            size_t p1 = line.find('|', 6);
            size_t p2 = p1 == std::string::npos ? std::string::npos : line.find('|', p1 + 1);
            size_t p3 = p2 == std::string::npos ? std::string::npos : line.find('|', p2 + 1);
            std::string fieldPath;
            std::string valueText;
            int op;
            int valueType;

            if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos ||
                outAst.wheres.size() >= HDB_QUERY_MAX_WHERE_COUNT)
            {
                SetLastError("invalid where item");
                return HDB_ERR_PARAM;
            }
            fieldPath = line.substr(6, p1 - 6);
            valueText = line.substr(p3 + 1);
            if (ValidateText(fieldPath, "where field path") != HDB_OK ||
                ValidateText(valueText, "where value") != HDB_OK)
            {
                return HDB_ERR_PARAM;
            }
            if (ParseInt32Strict(line.substr(p1 + 1, p2 - p1 - 1), &op) != HDB_OK ||
                ParseInt32Strict(line.substr(p2 + 1, p3 - p2 - 1), &valueType) != HDB_OK ||
                ValidateCompareOp(op) != HDB_OK ||
                ValidateValueType(valueType) != HDB_OK)
            {
                return HDB_ERR_QUERY_RANGE;
            }
            if (valueType == HDB_QVT_INT32)
            {
                int value;
                if (ParseInt32Strict(valueText, &value) != HDB_OK ||
                    outAst.AddWhereInt32(fieldPath.c_str(), op, value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (valueType == HDB_QVT_INT64)
            {
                HdbQueryInt64 value;
                if (ParseInt64Strict(valueText, &value) != HDB_OK ||
                    outAst.AddWhereInt64(fieldPath.c_str(), op, value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (valueType == HDB_QVT_DOUBLE)
            {
                double value;
                if (ParseDoubleStrict(valueText, &value) != HDB_OK ||
                    outAst.AddWhereDouble(fieldPath.c_str(), op, value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (outAst.AddWhereString(fieldPath.c_str(), op, valueText.c_str()) != 0)
            {
                return HDB_ERR_PARAM;
            }
        }
        else if (line.find("order=") == 0)
        {
            size_t sep = line.find('|', 6);
            std::string fieldPath;
            int orderType;
            if (sep == std::string::npos ||
                outAst.orders.size() >= HDB_QUERY_MAX_ORDER_COUNT ||
                ParseInt32Strict(line.substr(sep + 1), &orderType) != HDB_OK ||
                ValidateOrderType(orderType) != HDB_OK)
            {
                SetLastError("invalid order item");
                return HDB_ERR_QUERY_RANGE;
            }
            fieldPath = line.substr(6, sep - 6);
            if (ValidateText(fieldPath, "order field path") != HDB_OK ||
                outAst.AddOrder(fieldPath.c_str(), orderType) != 0)
            {
                SetLastError("invalid order item");
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else if (line.find("limit=") == 0)
        {
            int limit;
            int offset;
            if (ParsePairInt32(line.substr(6), &limit, &offset) != HDB_OK ||
                limit > HDB_QUERY_MAX_LIMIT ||
                outAst.SetLimit(limit, offset) != 0)
            {
                SetLastError("invalid limit");
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else
        {
            SetLastError("unknown query ast line");
            return HDB_ERR_PARAM;
        }
    }
    if (!seenVersion || outAst.rootDataset.empty())
    {
        SetLastError("query ast root is missing");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

const char* CHdbQueryAstCodec::GetLastError() const
{
    return m_lastError.c_str();
}

int CHdbQueryAstCodec::ValidateText(const std::string& text, const char* name)
{
    if (text.empty() || text.size() > HDB_QUERY_MAX_TEXT_LENGTH || HdbQueryCodecContainsUnsafeChar(text))
    {
        SetLastError(name);
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int CHdbQueryAstCodec::ValidateCompareOp(int op)
{
    if (op == HDB_OP_EQ || op == HDB_OP_NE || op == HDB_OP_GT ||
        op == HDB_OP_GE || op == HDB_OP_LT || op == HDB_OP_LE ||
        op == HDB_OP_LIKE)
    {
        return HDB_OK;
    }
    SetLastError("invalid compare op");
    return HDB_ERR_QUERY_RANGE;
}

int CHdbQueryAstCodec::ValidateValueType(int valueType)
{
    if (valueType == HDB_QVT_INT32 || valueType == HDB_QVT_INT64 ||
        valueType == HDB_QVT_DOUBLE || valueType == HDB_QVT_STRING)
    {
        return HDB_OK;
    }
    SetLastError("invalid query value type");
    return HDB_ERR_QUERY_RANGE;
}

int CHdbQueryAstCodec::ValidateOrderType(int orderType)
{
    if (orderType == HDB_ORDER_ASC || orderType == HDB_ORDER_DESC)
    {
        return HDB_OK;
    }
    SetLastError("invalid order type");
    return HDB_ERR_QUERY_RANGE;
}

int CHdbQueryAstCodec::ParseInt32Strict(const std::string& text, int* value)
{
    char* endPtr;
    long parsed;

    if (value == NULL || text.empty())
    {
        return HDB_ERR_PARAM;
    }
    errno = 0;
    endPtr = NULL;
    parsed = strtol(text.c_str(), &endPtr, 10);
    if (errno != 0 || endPtr == NULL || *endPtr != '\0' || parsed < INT_MIN || parsed > INT_MAX)
    {
        SetLastError("invalid int32 text");
        return HDB_ERR_QUERY_RANGE;
    }
    *value = (int)parsed;
    return HDB_OK;
}

int CHdbQueryAstCodec::ParseInt64Strict(const std::string& text, HdbQueryInt64* value)
{
    char* endPtr;

    if (value == NULL || text.empty())
    {
        return HDB_ERR_PARAM;
    }
    errno = 0;
    endPtr = NULL;
#ifdef _WIN32
    *value = (HdbQueryInt64)_strtoi64(text.c_str(), &endPtr, 10);
#else
    *value = (HdbQueryInt64)strtoll(text.c_str(), &endPtr, 10);
#endif
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        SetLastError("invalid int64 text");
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

int CHdbQueryAstCodec::ParseDoubleStrict(const std::string& text, double* value)
{
    char* endPtr;

    if (value == NULL || text.empty())
    {
        return HDB_ERR_PARAM;
    }
    errno = 0;
    endPtr = NULL;
    *value = strtod(text.c_str(), &endPtr);
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        SetLastError("invalid double text");
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

int CHdbQueryAstCodec::ParsePairInt64(const std::string& text, HdbQueryInt64* first, HdbQueryInt64* second)
{
    size_t comma = text.find(',');

    if (comma == std::string::npos)
    {
        return HDB_ERR_PARAM;
    }
    if (ParseInt64Strict(text.substr(0, comma), first) != HDB_OK ||
        ParseInt64Strict(text.substr(comma + 1), second) != HDB_OK)
    {
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

int CHdbQueryAstCodec::ParsePairInt32(const std::string& text, int* first, int* second)
{
    size_t comma = text.find(',');

    if (comma == std::string::npos)
    {
        return HDB_ERR_PARAM;
    }
    if (ParseInt32Strict(text.substr(0, comma), first) != HDB_OK ||
        ParseInt32Strict(text.substr(comma + 1), second) != HDB_OK)
    {
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

void CHdbQueryAstCodec::SetLastError(const char* text)
{
    if (HdbQueryCodecTextEmpty(text))
    {
        m_lastError = "unknown query ast codec error";
    }
    else
    {
        m_lastError = text;
    }
}
