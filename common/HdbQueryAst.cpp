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

static int HdbQueryIsValidCompareOp(int op)
{
    return op == HDB_OP_EQ ||
        op == HDB_OP_NE ||
        op == HDB_OP_GT ||
        op == HDB_OP_GE ||
        op == HDB_OP_LT ||
        op == HDB_OP_LE ||
        op == HDB_OP_LIKE;
}

static int HdbQueryIsValidFieldCompareOp(int op)
{
    return op == HDB_OP_EQ ||
        op == HDB_OP_NE ||
        op == HDB_OP_GT ||
        op == HDB_OP_GE ||
        op == HDB_OP_LT ||
        op == HDB_OP_LE;
}

static int HdbQueryIsValidStatementType(int statementType)
{
    return statementType == HDB_QST_SELECT ||
        statementType == HDB_QST_INSERT ||
        statementType == HDB_QST_UPDATE ||
        statementType == HDB_QST_DELETE;
}

static int HdbQueryIsValidConditionLogic(int logic)
{
    return logic == HDB_QCL_AND || logic == HDB_QCL_OR;
}

CHdbQueryAst::CHdbQueryAst()
{
    Clear();
}

void CHdbQueryAst::Clear()
{
    statementType = HDB_QST_SELECT;
    sources.clear();
    hasTimeRange = 0;
    beginMs = 0;
    endMs = 0;
    selects.clear();
    wheres.clear();
    conditions.clear();
    whereRootNodeId = -1;
    orders.clear();
    sets.clear();
    limit = 0;
    offset = 0;
}

int CHdbQueryAst::SetStatementType(int typeValue)
{
    if (!HdbQueryIsValidStatementType(typeValue))
    {
        return -1;
    }
    statementType = typeValue;
    return 0;
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
    item.localFieldName.clear();
    item.targetFieldName.clear();
    item.joinType = 0;
    item.onRootNodeId = -1;
    sources.push_back(item);
    if (outSourceId != NULL)
    {
        *outSourceId = item.sourceId;
    }
    return 0;
}

int CHdbQueryAst::AddJoinSource(int parentSourceId,
    const char* targetDatasetName,
    int joinType,
    int* outSourceId)
{
    HdbQuerySourceItem item;

    if (outSourceId != NULL)
    {
        *outSourceId = -1;
    }
    if (HdbQueryTextEmpty(targetDatasetName) ||
        HdbQueryTextContainsDot(targetDatasetName) ||
        !HdbQueryIsValidJoinType(joinType) ||
        FindSourceIndex(parentSourceId) < 0 ||
        sources.size() >= HDB_QUERY_MAX_SOURCE_COUNT)
    {
        return -1;
    }
    item.sourceId = (int)sources.size();
    item.sourceType = HDB_SOURCE_JOIN;
    item.parentSourceId = parentSourceId;
    item.datasetName = targetDatasetName;
    item.localFieldName.clear();
    item.targetFieldName.clear();
    item.joinType = joinType;
    item.onRootNodeId = -1;
    sources.push_back(item);
    if (outSourceId != NULL)
    {
        *outSourceId = item.sourceId;
    }
    return 0;
}

int CHdbQueryAst::AddJoinSourceOn(int parentSourceId,
    const char* targetDatasetName,
    int joinType,
    const char* localFieldName,
    const char* targetFieldName,
    int* outSourceId)
{
    int targetSourceId;
    int conditionId;

    if (outSourceId != NULL)
    {
        *outSourceId = -1;
    }
    if (HdbQueryTextEmpty(targetDatasetName) ||
        HdbQueryTextEmpty(localFieldName) ||
        HdbQueryTextEmpty(targetFieldName) ||
        HdbQueryTextContainsDot(targetDatasetName) ||
        HdbQueryTextContainsDot(localFieldName) ||
        HdbQueryTextContainsDot(targetFieldName))
    {
        return -1;
    }
    if (AddJoinSource(parentSourceId, targetDatasetName, joinType, &targetSourceId) != 0)
    {
        return -1;
    }
    conditionId = -1;
    if (AddConditionFieldCompare(parentSourceId,
        localFieldName,
        HDB_OP_EQ,
        targetSourceId,
        targetFieldName,
        &conditionId) != 0 ||
        SetJoinOnRoot(targetSourceId, conditionId) != 0)
    {
        if (!sources.empty() && sources.back().sourceId == targetSourceId)
        {
            sources.pop_back();
        }
        if (!conditions.empty() && conditions.back().nodeId == conditionId)
        {
            conditions.pop_back();
        }
        return -1;
    }
    sources[targetSourceId].localFieldName = localFieldName;
    sources[targetSourceId].targetFieldName = targetFieldName;
    if (outSourceId != NULL)
    {
        *outSourceId = targetSourceId;
    }
    return 0;
}

int CHdbQueryAst::SetJoinOnRoot(int sourceId, int nodeId)
{
    int sourceIndex;

    sourceIndex = FindSourceIndex(sourceId);
    if (sourceIndex < 0 ||
        sources[sourceIndex].sourceType != HDB_SOURCE_JOIN ||
        FindConditionIndex(nodeId) < 0)
    {
        return -1;
    }
    sources[sourceIndex].onRootNodeId = nodeId;
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

int CHdbQueryAst::AddSetInt32(int sourceId, const char* fieldName, int value)
{
    char buffer[32];
    HDB_QUERY_SNPRINTF(buffer, sizeof(buffer), "%d", value);
    buffer[sizeof(buffer) - 1] = '\0';
    return AddSetText(sourceId, fieldName, HDB_QVT_INT32, buffer);
}

int CHdbQueryAst::AddSetInt64(int sourceId, const char* fieldName, HdbQueryInt64 value)
{
    return AddSetText(sourceId, fieldName, HDB_QVT_INT64, HdbQueryInt64ToText(value));
}

int CHdbQueryAst::AddSetDouble(int sourceId, const char* fieldName, double value)
{
    return AddSetText(sourceId, fieldName, HDB_QVT_DOUBLE, HdbQueryDoubleToText(value));
}

int CHdbQueryAst::AddSetString(int sourceId, const char* fieldName, const char* value)
{
    if (value == NULL)
    {
        return -1;
    }
    return AddSetText(sourceId, fieldName, HDB_QVT_STRING, value);
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

int CHdbQueryAst::AddConditionCompare(int sourceId,
    const char* fieldName,
    int op,
    int valueType,
    const char* valueText,
    int* outNodeId)
{
    HdbQueryConditionItem item;

    if (outNodeId != NULL)
    {
        *outNodeId = -1;
    }
    if (valueText == NULL ||
        !HdbQueryIsValidCompareOp(op) ||
        AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.nodeId = (int)conditions.size();
    item.conditionType = HDB_QCT_COMPARE;
    item.op = op;
    item.valueType = valueType;
    item.valueText = valueText;
    item.rightField.sourceId = -1;
    item.rightField.fieldName.clear();
    item.secondValueText.clear();
    item.values.clear();
    item.logic = 0;
    item.childNodeIds.clear();
    conditions.push_back(item);
    if (outNodeId != NULL)
    {
        *outNodeId = item.nodeId;
    }
    return 0;
}

int CHdbQueryAst::AddConditionFieldCompare(int leftSourceId,
    const char* leftFieldName,
    int op,
    int rightSourceId,
    const char* rightFieldName,
    int* outNodeId)
{
    HdbQueryConditionItem item;

    if (outNodeId != NULL)
    {
        *outNodeId = -1;
    }
    if (!HdbQueryIsValidFieldCompareOp(op) ||
        AssignFieldRef(item.field, leftSourceId, leftFieldName) != 0 ||
        AssignFieldRef(item.rightField, rightSourceId, rightFieldName) != 0)
    {
        return -1;
    }
    item.nodeId = (int)conditions.size();
    item.conditionType = HDB_QCT_FIELD_COMPARE;
    item.op = op;
    item.valueType = 0;
    item.valueText.clear();
    item.secondValueText.clear();
    item.values.clear();
    item.logic = 0;
    item.childNodeIds.clear();
    conditions.push_back(item);
    if (outNodeId != NULL)
    {
        *outNodeId = item.nodeId;
    }
    return 0;
}

int CHdbQueryAst::AddConditionNull(int sourceId, const char* fieldName, int isNotNull, int* outNodeId)
{
    HdbQueryConditionItem item;

    if (outNodeId != NULL)
    {
        *outNodeId = -1;
    }
    if (AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.nodeId = (int)conditions.size();
    item.conditionType = HDB_QCT_NULL;
    item.op = isNotNull ? 1 : 0;
    item.valueType = 0;
    item.valueText.clear();
    item.rightField.sourceId = -1;
    item.rightField.fieldName.clear();
    item.secondValueText.clear();
    item.values.clear();
    item.logic = 0;
    item.childNodeIds.clear();
    conditions.push_back(item);
    if (outNodeId != NULL)
    {
        *outNodeId = item.nodeId;
    }
    return 0;
}

int CHdbQueryAst::AddConditionBetween(int sourceId,
    const char* fieldName,
    int valueType,
    const char* beginText,
    const char* endText,
    int* outNodeId)
{
    HdbQueryConditionItem item;

    if (outNodeId != NULL)
    {
        *outNodeId = -1;
    }
    if (beginText == NULL || endText == NULL || AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.nodeId = (int)conditions.size();
    item.conditionType = HDB_QCT_BETWEEN;
    item.op = 0;
    item.valueType = valueType;
    item.valueText = beginText;
    item.rightField.sourceId = -1;
    item.rightField.fieldName.clear();
    item.secondValueText = endText;
    item.values.clear();
    item.logic = 0;
    item.childNodeIds.clear();
    conditions.push_back(item);
    if (outNodeId != NULL)
    {
        *outNodeId = item.nodeId;
    }
    return 0;
}

int CHdbQueryAst::AddConditionIn(int sourceId,
    const char* fieldName,
    int valueType,
    const std::vector<std::string>& values,
    int* outNodeId)
{
    HdbQueryConditionItem item;

    if (outNodeId != NULL)
    {
        *outNodeId = -1;
    }
    if (values.empty() || AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.nodeId = (int)conditions.size();
    item.conditionType = HDB_QCT_IN;
    item.op = 0;
    item.valueType = valueType;
    item.valueText.clear();
    item.rightField.sourceId = -1;
    item.rightField.fieldName.clear();
    item.secondValueText.clear();
    item.values = values;
    item.logic = 0;
    item.childNodeIds.clear();
    conditions.push_back(item);
    if (outNodeId != NULL)
    {
        *outNodeId = item.nodeId;
    }
    return 0;
}

int CHdbQueryAst::AddConditionGroup(int logic,
    const std::vector<int>& childNodeIds,
    int* outNodeId)
{
    HdbQueryConditionItem item;
    size_t i;

    if (outNodeId != NULL)
    {
        *outNodeId = -1;
    }
    if (!HdbQueryIsValidConditionLogic(logic) || childNodeIds.size() < 2)
    {
        return -1;
    }
    for (i = 0; i < childNodeIds.size(); ++i)
    {
        if (FindConditionIndex(childNodeIds[i]) < 0)
        {
            return -1;
        }
    }
    item.nodeId = (int)conditions.size();
    item.conditionType = HDB_QCT_GROUP;
    item.op = 0;
    item.valueType = 0;
    item.valueText.clear();
    item.rightField.sourceId = -1;
    item.rightField.fieldName.clear();
    item.secondValueText.clear();
    item.values.clear();
    item.logic = logic;
    item.childNodeIds = childNodeIds;
    conditions.push_back(item);
    if (outNodeId != NULL)
    {
        *outNodeId = item.nodeId;
    }
    return 0;
}

int CHdbQueryAst::SetWhereRoot(int nodeId)
{
    if (FindConditionIndex(nodeId) < 0)
    {
        return -1;
    }
    whereRootNodeId = nodeId;
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

int CHdbQueryAst::FindConditionIndex(int nodeId) const
{
    size_t i;

    for (i = 0; i < conditions.size(); ++i)
    {
        if (conditions[i].nodeId == nodeId)
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

int CHdbQueryAst::AddSetText(int sourceId,
    const char* fieldName,
    int valueType,
    const std::string& valueText)
{
    HdbQuerySetItem item;

    if (AssignFieldRef(item.field, sourceId, fieldName) != 0)
    {
        return -1;
    }
    item.valueType = valueType;
    item.valueText = valueText;
    sets.push_back(item);
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
