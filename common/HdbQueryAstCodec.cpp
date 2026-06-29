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

    // 文本格式用换行和竖线做分隔，这些字符不进入业务字段
    for (index = 0; index < text.size(); ++index)
    {
        if (text[index] == '\n' || text[index] == '\r' || text[index] == '|')
        {
            return 1;
        }
    }
    return 0;
}

static int HdbQueryCodecContainsDot(const std::string& text)
{
    size_t index;

    for (index = 0; index < text.size(); ++index)
    {
        if (text[index] == '.')
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
    if (!ast.HasRootSource() || ast.sources.size() > HDB_QUERY_MAX_SOURCE_COUNT)
    {
        SetLastError("query source is invalid");
        return HDB_ERR_PARAM;
    }
    if (ast.selects.size() > HDB_QUERY_MAX_SELECT_COUNT ||
        ast.wheres.size() > HDB_QUERY_MAX_WHERE_COUNT ||
        ast.conditions.size() > HDB_QUERY_MAX_CONDITION_COUNT ||
        ast.sets.size() > HDB_QUERY_MAX_SET_COUNT ||
        ast.orders.size() > HDB_QUERY_MAX_ORDER_COUNT)
    {
        SetLastError("query ast item count exceeds limit");
        return HDB_ERR_QUERY_RANGE;
    }
    ret = ValidateStatementType(ast.statementType);
    if (ret != HDB_OK)
    {
        return ret;
    }
    out << "ast_version=" << HDB_QUERY_AST_VERSION << "\n";
    out << "statement=" << ast.statementType << "\n";
    for (index = 0; index < ast.sources.size(); ++index)
    {
        const HdbQuerySourceItem& source = ast.sources[index];
        if (source.sourceId != (int)index)
        {
            SetLastError("query source id is invalid");
            return HDB_ERR_PARAM;
        }
        if (source.sourceType == HDB_SOURCE_ROOT)
        {
            ret = ValidateNameText(source.datasetName, "root dataset");
            if (ret != HDB_OK || source.sourceId != 0 || source.parentSourceId != -1 || source.joinType != 0)
            {
                SetLastError("invalid root source");
                return HDB_ERR_PARAM;
            }
            out << "source=root|" << source.sourceId << "|" << source.datasetName << "\n";
        }
        else if (source.sourceType == HDB_SOURCE_JOIN)
        {
            if (source.parentSourceId < 0 ||
                source.parentSourceId >= source.sourceId ||
                ValidateJoinType(source.joinType) != HDB_OK)
            {
                SetLastError("invalid join source");
                return HDB_ERR_QUERY_RANGE;
            }
            if (!source.datasetName.empty())
            {
                ret = ValidateNameText(source.datasetName, "join target dataset");
                if (ret != HDB_OK)
                {
                    return ret;
                }
                ret = ValidateNameText(source.localFieldName, "join local field");
                if (ret != HDB_OK)
                {
                    return ret;
                }
                ret = ValidateNameText(source.targetFieldName, "join target field");
                if (ret != HDB_OK)
                {
                    return ret;
                }
                out << "source=join_on|" << source.sourceId << "|"
                    << source.parentSourceId << "|"
                    << source.datasetName << "|"
                    << source.joinType << "|"
                    << source.localFieldName << "|"
                    << source.targetFieldName << "\n";
            }
            else
            {
                ret = ValidateNameText(source.associationName, "association name");
                if (ret != HDB_OK)
                {
                    return ret;
                }
                out << "source=join|" << source.sourceId << "|"
                    << source.parentSourceId << "|"
                    << source.associationName << "|"
                    << source.joinType << "\n";
            }
        }
        else
        {
            SetLastError("invalid source type");
            return HDB_ERR_QUERY_RANGE;
        }
    }
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
        ret = ValidateNameText(ast.selects[index].field.fieldName, "select field name");
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateText(ast.selects[index].outputName, "select output name");
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (ast.FindSourceIndex(ast.selects[index].field.sourceId) < 0)
        {
            SetLastError("select source is invalid");
            return HDB_ERR_FIELD_REF;
        }
        out << "select=" << ast.selects[index].field.sourceId << "|"
            << ast.selects[index].field.fieldName << "|"
            << ast.selects[index].outputName << "\n";
    }
    for (index = 0; index < ast.sets.size(); ++index)
    {
        ret = ValidateNameText(ast.sets[index].field.fieldName, "set field name");
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateValueType(ast.sets[index].valueType);
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateValueText(ast.sets[index].valueText, "set value");
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (ast.FindSourceIndex(ast.sets[index].field.sourceId) < 0)
        {
            SetLastError("set source is invalid");
            return HDB_ERR_FIELD_REF;
        }
        out << "set=" << ast.sets[index].field.sourceId << "|"
            << ast.sets[index].field.fieldName << "|"
            << ast.sets[index].valueType << "|"
            << ast.sets[index].valueText << "\n";
    }
    for (index = 0; index < ast.wheres.size(); ++index)
    {
        ret = ValidateNameText(ast.wheres[index].field.fieldName, "where field name");
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
        ret = ValidateValueText(ast.wheres[index].valueText, "where value");
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (ast.FindSourceIndex(ast.wheres[index].field.sourceId) < 0)
        {
            SetLastError("where source is invalid");
            return HDB_ERR_FIELD_REF;
        }
        out << "where=" << ast.wheres[index].field.sourceId << "|"
            << ast.wheres[index].field.fieldName << "|"
            << ast.wheres[index].op << "|"
            << ast.wheres[index].valueType << "|"
            << ast.wheres[index].valueText << "\n";
    }
    for (index = 0; index < ast.conditions.size(); ++index)
    {
        const HdbQueryConditionItem& condition = ast.conditions[index];
        size_t childIndex;

        if (condition.nodeId != (int)index)
        {
            SetLastError("condition node id is invalid");
            return HDB_ERR_QUERY_RANGE;
        }
        ret = ValidateConditionType(condition.conditionType);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (condition.conditionType == HDB_QCT_COMPARE)
        {
            ret = ValidateNameText(condition.field.fieldName, "condition field name");
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateCompareOp(condition.op);
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateValueType(condition.valueType);
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateValueText(condition.valueText, "condition value");
            if (ret != HDB_OK)
            {
                return ret;
            }
            out << "condition=compare|" << condition.nodeId << "|"
                << condition.field.sourceId << "|"
                << condition.field.fieldName << "|"
                << condition.op << "|"
                << condition.valueType << "|"
                << condition.valueText << "\n";
        }
        else if (condition.conditionType == HDB_QCT_NULL)
        {
            ret = ValidateNameText(condition.field.fieldName, "condition field name");
            if (ret != HDB_OK)
            {
                return ret;
            }
            out << "condition=null|" << condition.nodeId << "|"
                << condition.field.sourceId << "|"
                << condition.field.fieldName << "|"
                << condition.op << "\n";
        }
        else if (condition.conditionType == HDB_QCT_BETWEEN)
        {
            ret = ValidateNameText(condition.field.fieldName, "condition field name");
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateValueType(condition.valueType);
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateValueText(condition.valueText, "condition begin value");
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateValueText(condition.secondValueText, "condition end value");
            if (ret != HDB_OK)
            {
                return ret;
            }
            out << "condition=between|" << condition.nodeId << "|"
                << condition.field.sourceId << "|"
                << condition.field.fieldName << "|"
                << condition.valueType << "|"
                << condition.valueText << "|"
                << condition.secondValueText << "\n";
        }
        else if (condition.conditionType == HDB_QCT_IN)
        {
            if (condition.values.empty() || condition.values.size() > HDB_QUERY_MAX_IN_VALUE_COUNT)
            {
                SetLastError("invalid in value count");
                return HDB_ERR_QUERY_RANGE;
            }
            ret = ValidateNameText(condition.field.fieldName, "condition field name");
            if (ret != HDB_OK)
            {
                return ret;
            }
            ret = ValidateValueType(condition.valueType);
            if (ret != HDB_OK)
            {
                return ret;
            }
            out << "condition=in|" << condition.nodeId << "|"
                << condition.field.sourceId << "|"
                << condition.field.fieldName << "|"
                << condition.valueType;
            for (childIndex = 0; childIndex < condition.values.size(); ++childIndex)
            {
                ret = ValidateValueText(condition.values[childIndex], "condition in value");
                if (ret != HDB_OK)
                {
                    return ret;
                }
                out << "|" << condition.values[childIndex];
            }
            out << "\n";
        }
        else if (condition.conditionType == HDB_QCT_GROUP)
        {
            ret = ValidateConditionLogic(condition.logic);
            if (ret != HDB_OK)
            {
                return ret;
            }
            if (condition.childNodeIds.size() < 2)
            {
                SetLastError("condition group has too few children");
                return HDB_ERR_QUERY_RANGE;
            }
            out << "condition=group|" << condition.nodeId << "|"
                << condition.logic;
            for (childIndex = 0; childIndex < condition.childNodeIds.size(); ++childIndex)
            {
                if (ast.FindConditionIndex(condition.childNodeIds[childIndex]) < 0)
                {
                    SetLastError("condition group child is invalid");
                    return HDB_ERR_QUERY_RANGE;
                }
                out << "|" << condition.childNodeIds[childIndex];
            }
            out << "\n";
        }
    }
    if (ast.whereRootNodeId >= 0)
    {
        if (ast.FindConditionIndex(ast.whereRootNodeId) < 0)
        {
            SetLastError("where root condition is invalid");
            return HDB_ERR_QUERY_RANGE;
        }
        out << "where_root=" << ast.whereRootNodeId << "\n";
    }
    for (index = 0; index < ast.orders.size(); ++index)
    {
        ret = ValidateNameText(ast.orders[index].field.fieldName, "order field name");
        if (ret != HDB_OK)
        {
            return ret;
        }
        ret = ValidateOrderType(ast.orders[index].orderType);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (ast.FindSourceIndex(ast.orders[index].field.sourceId) < 0)
        {
            SetLastError("order source is invalid");
            return HDB_ERR_FIELD_REF;
        }
        out << "order=" << ast.orders[index].field.sourceId << "|"
            << ast.orders[index].field.fieldName << "|"
            << ast.orders[index].orderType << "\n";
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
            if (ParseInt32Strict(line.substr(12), &version) != HDB_OK ||
                (version != HDB_QUERY_AST_VERSION && version != 3 && version != 2))
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
        else if (line.find("statement=") == 0)
        {
            int statementType;

            if (ParseInt32Strict(line.substr(10), &statementType) != HDB_OK ||
                ValidateStatementType(statementType) != HDB_OK ||
                outAst.SetStatementType(statementType) != 0)
            {
                SetLastError("invalid statement type");
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else if (line.find("source=") == 0)
        {
            std::vector<std::string> fields;
            int sourceId;
            int parentSourceId;
            int joinType;
            int addedSourceId;

            if (SplitFields(line.substr(7), fields) != HDB_OK || fields.empty())
            {
                SetLastError("invalid source item");
                return HDB_ERR_PARAM;
            }
            if (fields[0] == "root")
            {
                if (fields.size() != 3 ||
                    ParseInt32Strict(fields[1], &sourceId) != HDB_OK ||
                    sourceId != 0 ||
                    ValidateNameText(fields[2], "root dataset") != HDB_OK ||
                    outAst.AddRootSource(fields[2].c_str(), &addedSourceId) != 0 ||
                    addedSourceId != sourceId)
                {
                    SetLastError("invalid root source");
                    return HDB_ERR_PARAM;
                }
            }
            else if (fields[0] == "join")
            {
                if (fields.size() != 5 ||
                    ParseInt32Strict(fields[1], &sourceId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &parentSourceId) != HDB_OK ||
                    ValidateNameText(fields[3], "association name") != HDB_OK ||
                    ParseInt32Strict(fields[4], &joinType) != HDB_OK ||
                    ValidateJoinType(joinType) != HDB_OK ||
                    outAst.AddJoinSource(parentSourceId, fields[3].c_str(), joinType, &addedSourceId) != 0 ||
                    addedSourceId != sourceId)
                {
                    SetLastError("invalid join source");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (fields[0] == "join_on")
            {
                if (fields.size() != 7 ||
                    ParseInt32Strict(fields[1], &sourceId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &parentSourceId) != HDB_OK ||
                    ValidateNameText(fields[3], "join target dataset") != HDB_OK ||
                    ParseInt32Strict(fields[4], &joinType) != HDB_OK ||
                    ValidateJoinType(joinType) != HDB_OK ||
                    ValidateNameText(fields[5], "join local field") != HDB_OK ||
                    ValidateNameText(fields[6], "join target field") != HDB_OK ||
                    outAst.AddJoinSourceOn(parentSourceId,
                        fields[3].c_str(),
                        joinType,
                        fields[5].c_str(),
                        fields[6].c_str(),
                        &addedSourceId) != 0 ||
                    addedSourceId != sourceId)
                {
                    SetLastError("invalid join on source");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else
            {
                SetLastError("invalid source type");
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
            std::vector<std::string> fields;
            int sourceId;

            if (SplitFields(line.substr(7), fields) != HDB_OK ||
                fields.size() != 3 ||
                outAst.selects.size() >= HDB_QUERY_MAX_SELECT_COUNT ||
                ParseInt32Strict(fields[0], &sourceId) != HDB_OK ||
                ValidateNameText(fields[1], "select field name") != HDB_OK ||
                ValidateText(fields[2], "select output name") != HDB_OK ||
                outAst.AddSelect(sourceId, fields[1].c_str(), fields[2].c_str()) != 0)
            {
                SetLastError("invalid select item");
                return HDB_ERR_PARAM;
            }
        }
        else if (line.find("set=") == 0)
        {
            std::vector<std::string> fields;
            int sourceId;
            int valueType;

            if (SplitFields(line.substr(4), fields) != HDB_OK ||
                fields.size() != 4 ||
                outAst.sets.size() >= HDB_QUERY_MAX_SET_COUNT ||
                ParseInt32Strict(fields[0], &sourceId) != HDB_OK ||
                ValidateNameText(fields[1], "set field name") != HDB_OK ||
                ParseInt32Strict(fields[2], &valueType) != HDB_OK ||
                ValidateValueType(valueType) != HDB_OK ||
                ValidateValueText(fields[3], "set value") != HDB_OK)
            {
                SetLastError("invalid set item");
                return HDB_ERR_QUERY_RANGE;
            }
            if (valueType == HDB_QVT_INT32)
            {
                int value;
                if (ParseInt32Strict(fields[3], &value) != HDB_OK ||
                    outAst.AddSetInt32(sourceId, fields[1].c_str(), value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (valueType == HDB_QVT_INT64)
            {
                HdbQueryInt64 value;
                if (ParseInt64Strict(fields[3], &value) != HDB_OK ||
                    outAst.AddSetInt64(sourceId, fields[1].c_str(), value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (valueType == HDB_QVT_DOUBLE)
            {
                double value;
                if (ParseDoubleStrict(fields[3], &value) != HDB_OK ||
                    outAst.AddSetDouble(sourceId, fields[1].c_str(), value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (outAst.AddSetString(sourceId, fields[1].c_str(), fields[3].c_str()) != 0)
            {
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else if (line.find("where=") == 0)
        {
            std::vector<std::string> fields;
            int sourceId;
            int op;
            int valueType;

            if (SplitFields(line.substr(6), fields) != HDB_OK ||
                fields.size() != 5 ||
                outAst.wheres.size() >= HDB_QUERY_MAX_WHERE_COUNT ||
                ParseInt32Strict(fields[0], &sourceId) != HDB_OK ||
                ValidateNameText(fields[1], "where field name") != HDB_OK ||
                ValidateValueText(fields[4], "where value") != HDB_OK ||
                ParseInt32Strict(fields[2], &op) != HDB_OK ||
                ParseInt32Strict(fields[3], &valueType) != HDB_OK ||
                ValidateCompareOp(op) != HDB_OK ||
                ValidateValueType(valueType) != HDB_OK)
            {
                SetLastError("invalid where item");
                return HDB_ERR_QUERY_RANGE;
            }
            if (valueType == HDB_QVT_INT32)
            {
                int value;
                if (ParseInt32Strict(fields[4], &value) != HDB_OK ||
                    outAst.AddWhereInt32(sourceId, fields[1].c_str(), op, value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (valueType == HDB_QVT_INT64)
            {
                HdbQueryInt64 value;
                if (ParseInt64Strict(fields[4], &value) != HDB_OK ||
                    outAst.AddWhereInt64(sourceId, fields[1].c_str(), op, value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (valueType == HDB_QVT_DOUBLE)
            {
                double value;
                if (ParseDoubleStrict(fields[4], &value) != HDB_OK ||
                    outAst.AddWhereDouble(sourceId, fields[1].c_str(), op, value) != 0)
                {
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (outAst.AddWhereString(sourceId, fields[1].c_str(), op, fields[4].c_str()) != 0)
            {
                return HDB_ERR_PARAM;
            }
        }
        else if (line.find("condition=") == 0)
        {
            std::vector<std::string> fields;
            int nodeId;
            int sourceId;
            int op;
            int valueType;
            int addedNodeId;

            if (SplitFields(line.substr(10), fields) != HDB_OK || fields.empty())
            {
                SetLastError("invalid condition item");
                return HDB_ERR_QUERY_RANGE;
            }
            if (fields[0] == "compare")
            {
                if (fields.size() != 7 ||
                    outAst.conditions.size() >= HDB_QUERY_MAX_CONDITION_COUNT ||
                    ParseInt32Strict(fields[1], &nodeId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &sourceId) != HDB_OK ||
                    ValidateNameText(fields[3], "condition field name") != HDB_OK ||
                    ParseInt32Strict(fields[4], &op) != HDB_OK ||
                    ParseInt32Strict(fields[5], &valueType) != HDB_OK ||
                    ValidateCompareOp(op) != HDB_OK ||
                    ValidateValueType(valueType) != HDB_OK ||
                    ValidateValueText(fields[6], "condition value") != HDB_OK ||
                    outAst.AddConditionCompare(sourceId,
                        fields[3].c_str(),
                        op,
                        valueType,
                        fields[6].c_str(),
                        &addedNodeId) != 0 ||
                    addedNodeId != nodeId)
                {
                    SetLastError("invalid compare condition");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (fields[0] == "null")
            {
                if (fields.size() != 5 ||
                    outAst.conditions.size() >= HDB_QUERY_MAX_CONDITION_COUNT ||
                    ParseInt32Strict(fields[1], &nodeId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &sourceId) != HDB_OK ||
                    ValidateNameText(fields[3], "condition field name") != HDB_OK ||
                    ParseInt32Strict(fields[4], &op) != HDB_OK ||
                    (op != 0 && op != 1) ||
                    outAst.AddConditionNull(sourceId, fields[3].c_str(), op, &addedNodeId) != 0 ||
                    addedNodeId != nodeId)
                {
                    SetLastError("invalid null condition");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (fields[0] == "between")
            {
                if (fields.size() != 7 ||
                    outAst.conditions.size() >= HDB_QUERY_MAX_CONDITION_COUNT ||
                    ParseInt32Strict(fields[1], &nodeId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &sourceId) != HDB_OK ||
                    ValidateNameText(fields[3], "condition field name") != HDB_OK ||
                    ParseInt32Strict(fields[4], &valueType) != HDB_OK ||
                    ValidateValueType(valueType) != HDB_OK ||
                    ValidateValueText(fields[5], "condition begin value") != HDB_OK ||
                    ValidateValueText(fields[6], "condition end value") != HDB_OK ||
                    outAst.AddConditionBetween(sourceId,
                        fields[3].c_str(),
                        valueType,
                        fields[5].c_str(),
                        fields[6].c_str(),
                        &addedNodeId) != 0 ||
                    addedNodeId != nodeId)
                {
                    SetLastError("invalid between condition");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (fields[0] == "in")
            {
                std::vector<std::string> values;
                size_t valueIndex;

                if (fields.size() < 6 ||
                    fields.size() - 5 > HDB_QUERY_MAX_IN_VALUE_COUNT ||
                    outAst.conditions.size() >= HDB_QUERY_MAX_CONDITION_COUNT ||
                    ParseInt32Strict(fields[1], &nodeId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &sourceId) != HDB_OK ||
                    ValidateNameText(fields[3], "condition field name") != HDB_OK ||
                    ParseInt32Strict(fields[4], &valueType) != HDB_OK ||
                    ValidateValueType(valueType) != HDB_OK)
                {
                    SetLastError("invalid in condition");
                    return HDB_ERR_QUERY_RANGE;
                }
                values.clear();
                for (valueIndex = 5; valueIndex < fields.size(); ++valueIndex)
                {
                    if (ValidateValueText(fields[valueIndex], "condition in value") != HDB_OK)
                    {
                        return HDB_ERR_QUERY_RANGE;
                    }
                    values.push_back(fields[valueIndex]);
                }
                if (outAst.AddConditionIn(sourceId,
                    fields[3].c_str(),
                    valueType,
                    values,
                    &addedNodeId) != 0 ||
                    addedNodeId != nodeId)
                {
                    SetLastError("invalid in condition");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else if (fields[0] == "group")
            {
                std::vector<int> childNodeIds;
                int logic;
                size_t childIndex;
                int childNodeId;

                if (fields.size() < 5 ||
                    outAst.conditions.size() >= HDB_QUERY_MAX_CONDITION_COUNT ||
                    ParseInt32Strict(fields[1], &nodeId) != HDB_OK ||
                    ParseInt32Strict(fields[2], &logic) != HDB_OK ||
                    ValidateConditionLogic(logic) != HDB_OK)
                {
                    SetLastError("invalid group condition");
                    return HDB_ERR_QUERY_RANGE;
                }
                childNodeIds.clear();
                for (childIndex = 3; childIndex < fields.size(); ++childIndex)
                {
                    if (ParseInt32Strict(fields[childIndex], &childNodeId) != HDB_OK)
                    {
                        return HDB_ERR_QUERY_RANGE;
                    }
                    childNodeIds.push_back(childNodeId);
                }
                if (outAst.AddConditionGroup(logic, childNodeIds, &addedNodeId) != 0 ||
                    addedNodeId != nodeId)
                {
                    SetLastError("invalid group condition");
                    return HDB_ERR_QUERY_RANGE;
                }
            }
            else
            {
                SetLastError("unknown condition type");
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else if (line.find("where_root=") == 0)
        {
            int nodeId;

            if (ParseInt32Strict(line.substr(11), &nodeId) != HDB_OK ||
                outAst.SetWhereRoot(nodeId) != 0)
            {
                SetLastError("invalid where root");
                return HDB_ERR_QUERY_RANGE;
            }
        }
        else if (line.find("order=") == 0)
        {
            std::vector<std::string> fields;
            int sourceId;
            int orderType;

            if (SplitFields(line.substr(6), fields) != HDB_OK ||
                fields.size() != 3 ||
                outAst.orders.size() >= HDB_QUERY_MAX_ORDER_COUNT ||
                ParseInt32Strict(fields[0], &sourceId) != HDB_OK ||
                ValidateNameText(fields[1], "order field name") != HDB_OK ||
                ParseInt32Strict(fields[2], &orderType) != HDB_OK ||
                ValidateOrderType(orderType) != HDB_OK ||
                outAst.AddOrder(sourceId, fields[1].c_str(), orderType) != 0)
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
    if (!seenVersion || !outAst.HasRootSource())
    {
        SetLastError("query ast source is missing");
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

int CHdbQueryAstCodec::ValidateValueText(const std::string& text, const char* name)
{
    if (text.size() > HDB_QUERY_MAX_TEXT_LENGTH || HdbQueryCodecContainsUnsafeChar(text))
    {
        SetLastError(name);
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int CHdbQueryAstCodec::ValidateNameText(const std::string& text, const char* name)
{
    int ret;

    ret = ValidateText(text, name);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (HdbQueryCodecContainsDot(text))
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

int CHdbQueryAstCodec::ValidateJoinType(int joinType)
{
    if (joinType == HDB_JOIN_INNER || joinType == HDB_JOIN_LEFT)
    {
        return HDB_OK;
    }
    SetLastError("invalid join type");
    return HDB_ERR_QUERY_RANGE;
}

int CHdbQueryAstCodec::ValidateStatementType(int statementType)
{
    if (statementType == HDB_QST_SELECT ||
        statementType == HDB_QST_INSERT ||
        statementType == HDB_QST_UPDATE ||
        statementType == HDB_QST_DELETE)
    {
        return HDB_OK;
    }
    SetLastError("invalid statement type");
    return HDB_ERR_QUERY_RANGE;
}

int CHdbQueryAstCodec::ValidateConditionType(int conditionType)
{
    if (conditionType == HDB_QCT_COMPARE ||
        conditionType == HDB_QCT_NULL ||
        conditionType == HDB_QCT_BETWEEN ||
        conditionType == HDB_QCT_IN ||
        conditionType == HDB_QCT_GROUP)
    {
        return HDB_OK;
    }
    SetLastError("invalid condition type");
    return HDB_ERR_QUERY_RANGE;
}

int CHdbQueryAstCodec::ValidateConditionLogic(int logic)
{
    if (logic == HDB_QCL_AND || logic == HDB_QCL_OR)
    {
        return HDB_OK;
    }
    SetLastError("invalid condition logic");
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

int CHdbQueryAstCodec::SplitFields(const std::string& text, std::vector<std::string>& fields)
{
    size_t pos;
    size_t next;

    fields.clear();
    pos = 0;
    while (pos <= text.size())
    {
        next = text.find('|', pos);
        if (next == std::string::npos)
        {
            fields.push_back(text.substr(pos));
            break;
        }
        fields.push_back(text.substr(pos, next - pos));
        pos = next + 1;
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
