#include "HdbQuerySqlBuilder.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sstream>

static int HdbBuilderLocalTime(struct tm* outTm, const time_t* inTime)
{
#ifdef _WIN32
    return localtime_s(outTm, inTime);
#else
    return localtime_r(inTime, outTm) == NULL ? -1 : 0;
#endif
}

static std::string IntToString(int value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

static int HdbBuilderParseInt32Strict(const std::string& text, int* value)
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
        return HDB_ERR_QUERY_RANGE;
    }
    *value = (int)parsed;
    return HDB_OK;
}

static int HdbBuilderParseInt64Strict(const std::string& text, HdbInt64* value)
{
    char* endPtr;

    if (value == NULL || text.empty())
    {
        return HDB_ERR_PARAM;
    }
    errno = 0;
    endPtr = NULL;
#ifdef _WIN32
    *value = (HdbInt64)_strtoi64(text.c_str(), &endPtr, 10);
#else
    *value = (HdbInt64)strtoll(text.c_str(), &endPtr, 10);
#endif
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

static int HdbBuilderParseDoubleStrict(const std::string& text)
{
    char* endPtr;

    if (text.empty())
    {
        return HDB_ERR_PARAM;
    }
    errno = 0;
    endPtr = NULL;
    strtod(text.c_str(), &endPtr);
    if (errno != 0 || endPtr == NULL || *endPtr != '\0')
    {
        return HDB_ERR_QUERY_RANGE;
    }
    return HDB_OK;
}

void HdbBuiltQuery::Clear()
{
    sql.clear();
    params.clear();
    outputNames.clear();
    outputTypes.clear();
}

CHdbQuerySqlBuilder::CHdbQuerySqlBuilder(const CHdbDatasetRegistry* registry)
    : m_registry(registry)
{
}

int CHdbQuerySqlBuilder::BuildSelect(const CHdbQueryAst& ast, HdbBuiltQuery& outQuery)
{
    std::vector<ResolvedSource> sources;
    std::vector<ResolvedField> selectFields;
    std::vector<ResolvedField> whereFields;
    std::vector<ResolvedField> orderFields;
    std::vector<std::string> rootColumns;
    std::string sourceSql;
    std::string joinSql;
    std::string whereSql;
    std::string orderSql;
    std::ostringstream sql;
    const ResolvedSource* rootSource;
    int ret;
    size_t i;
    int limitValue;
    int limitParam;
    int offsetParam;

    outQuery.Clear();
    if (m_registry == NULL)
    {
        SetLastError("dataset registry is NULL");
        return HDB_ERR_PARAM;
    }
    if (ast.selects.empty())
    {
        SetLastError("query has no select fields");
        return HDB_ERR_PARAM;
    }
    if (ast.limit > HDB_QUERY_MAX_LIMIT || ast.offset < 0)
    {
        SetLastError("query limit exceeds range");
        return HDB_ERR_QUERY_RANGE;
    }
    ret = ResolveSources(ast, sources);
    if (ret != HDB_OK)
    {
        return ret;
    }
    rootSource = FindResolvedSource(sources, 0);
    if (rootSource == NULL)
    {
        SetLastError("root source is missing");
        return HDB_ERR_PARAM;
    }
    if (rootSource->dataset->shard.shardType == HDB_SHARD_DAY && !ast.hasTimeRange)
    {
        SetLastError("day shard query requires time range");
        return HDB_ERR_QUERY_NEED_TIME_RANGE;
    }
    ret = ResolveSelectFields(ast, sources, selectFields);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = ResolveWhereFields(ast, sources, whereFields);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = ResolveOrderFields(ast, sources, orderFields);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = CollectRootColumns(ast, sources, selectFields, whereFields, orderFields, rootColumns);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildRootSource(ast, *rootSource, rootColumns, whereFields, outQuery, sourceSql);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildJoins(sources, joinSql);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildWhere(ast, *rootSource, whereFields, outQuery, whereSql);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildOrder(ast, orderFields, orderSql);
    if (ret != HDB_OK)
    {
        return ret;
    }

    sql << "select ";
    // 对外输出名不直接进入 SQL，SQL 只使用 c1/c2 这类稳定别名
    for (i = 0; i < selectFields.size(); ++i)
    {
        std::string expr;
        ret = AppendFieldExpr(selectFields[i], expr);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (i > 0)
        {
            sql << ", ";
        }
        sql << expr << " as c" << (i + 1);
        outQuery.outputNames.push_back(ast.selects[i].outputName);
        outQuery.outputTypes.push_back(selectFields[i].field->type);
    }
    sql << " from " << sourceSql;
    if (!joinSql.empty())
    {
        sql << " " << joinSql;
    }
    if (!whereSql.empty())
    {
        sql << " where " << whereSql;
    }
    if (!orderSql.empty())
    {
        sql << " order by " << orderSql;
    }

    limitValue = ast.limit > 0 ? ast.limit : HDB_QUERY_DEFAULT_LIMIT;
    // limit 和 offset 也走参数，避免调用方输入拼进 SQL
    limitParam = AddParam(outQuery, IntToString(limitValue));
    offsetParam = AddParam(outQuery, IntToString(ast.offset));
    sql << " limit " << Placeholder(limitParam) << " offset " << Placeholder(offsetParam);

    outQuery.sql = sql.str();
    return HDB_OK;
}

const char* CHdbQuerySqlBuilder::GetLastError() const
{
    return m_lastError.c_str();
}

int CHdbQuerySqlBuilder::ResolveSources(const CHdbQueryAst& ast, std::vector<ResolvedSource>& sources)
{
    size_t i;
    int rootCount;

    sources.clear();
    if (ast.sources.empty() || ast.sources.size() > HDB_QUERY_MAX_SOURCE_COUNT)
    {
        SetLastError("query source list is invalid");
        return HDB_ERR_PARAM;
    }
    rootCount = 0;
    for (i = 0; i < ast.sources.size(); ++i)
    {
        const HdbQuerySourceItem& source = ast.sources[i];
        ResolvedSource resolved;

        if (source.sourceId != (int)i)
        {
            SetLastError("query source id is invalid");
            return HDB_ERR_PARAM;
        }
        resolved.sourceId = source.sourceId;
        resolved.parentSourceId = source.parentSourceId;
        resolved.dataset = NULL;
        resolved.association = NULL;
        resolved.localField = NULL;
        resolved.targetField = NULL;
        resolved.joinType = source.joinType;
        resolved.sqlAlias = AliasForSource(source.sourceId);
        if (source.sourceType == HDB_SOURCE_ROOT)
        {
            const HdbDatasetDef* dataset;
            int ret;

            ++rootCount;
            if (source.sourceId != 0 || source.parentSourceId != -1 || source.joinType != 0)
            {
                SetLastError("root source is invalid");
                return HDB_ERR_PARAM;
            }
            if (m_registry->ValidateIdentifier(source.datasetName.c_str()) != HDB_OK)
            {
                SetLastError(m_registry->GetLastError());
                return HDB_ERR_DATASET_NOT_FOUND;
            }
            dataset = m_registry->FindDataset(source.datasetName.c_str());
            if (dataset == NULL)
            {
                SetLastError("root dataset is not found");
                return HDB_ERR_DATASET_NOT_FOUND;
            }
            ret = m_registry->ValidateDataset(*dataset);
            if (ret != HDB_OK)
            {
                SetLastError(m_registry->GetLastError());
                return ret;
            }
            resolved.dataset = dataset;
        }
        else if (source.sourceType == HDB_SOURCE_JOIN)
        {
            const ResolvedSource* parentSource;
            const HdbAssociationDef* association;
            const HdbDatasetDef* targetDataset;
            int ret;

            if (source.parentSourceId < 0 ||
                source.parentSourceId >= source.sourceId ||
                ValidateJoinType(source.joinType) != HDB_OK)
            {
                SetLastError("join source is invalid");
                return HDB_ERR_QUERY_RANGE;
            }
            parentSource = FindResolvedSource(sources, source.parentSourceId);
            if (parentSource == NULL || parentSource->dataset == NULL)
            {
                SetLastError("join parent source is missing");
                return HDB_ERR_FIELD_REF;
            }
            if (m_registry->ValidateIdentifier(source.associationName.c_str()) != HDB_OK)
            {
                SetLastError(m_registry->GetLastError());
                return HDB_ERR_ASSOCIATION_NOT_FOUND;
            }
            association = m_registry->FindAssociation(parentSource->dataset->datasetName,
                source.associationName.c_str());
            if (association == NULL)
            {
                SetLastError("association is not found");
                return HDB_ERR_ASSOCIATION_NOT_FOUND;
            }
            ret = m_registry->ValidateAssociation(*association);
            if (ret != HDB_OK)
            {
                SetLastError(m_registry->GetLastError());
                return ret;
            }
            targetDataset = m_registry->FindDataset(association->targetDataset);
            if (targetDataset == NULL)
            {
                SetLastError("association target dataset is missing");
                return HDB_ERR_ASSOCIATION_NOT_FOUND;
            }
            ret = ValidateJoinTargetDataset(*targetDataset);
            if (ret != HDB_OK)
            {
                return ret;
            }
            resolved.association = association;
            resolved.dataset = targetDataset;
            resolved.localField = m_registry->FindField(*parentSource->dataset, association->localFieldName);
            resolved.targetField = m_registry->FindField(*targetDataset, association->targetFieldName);
            if (resolved.localField == NULL || resolved.targetField == NULL)
            {
                SetLastError("association field is missing");
                return HDB_ERR_ASSOCIATION_NOT_FOUND;
            }
        }
        else
        {
            SetLastError("source type is invalid");
            return HDB_ERR_PARAM;
        }
        sources.push_back(resolved);
    }
    if (rootCount != 1 || sources[0].dataset == NULL)
    {
        SetLastError("query must have exactly one root source");
        return HDB_ERR_PARAM;
    }
    return HDB_OK;
}

int CHdbQuerySqlBuilder::ResolveSelectFields(const CHdbQueryAst& ast,
    const std::vector<ResolvedSource>& sources,
    std::vector<ResolvedField>& fields)
{
    size_t i;

    fields.clear();
    for (i = 0; i < ast.selects.size(); ++i)
    {
        ResolvedField field;
        int ret = ResolveFieldRef(sources, ast.selects[i].field, field);
        if (ret != HDB_OK)
        {
            return ret;
        }
        fields.push_back(field);
    }
    return HDB_OK;
}

int CHdbQuerySqlBuilder::ResolveWhereFields(const CHdbQueryAst& ast,
    const std::vector<ResolvedSource>& sources,
    std::vector<ResolvedField>& fields)
{
    size_t i;

    fields.clear();
    for (i = 0; i < ast.wheres.size(); ++i)
    {
        ResolvedField field;
        int ret = ResolveFieldRef(sources, ast.wheres[i].field, field);
        if (ret != HDB_OK)
        {
            return ret;
        }
        fields.push_back(field);
    }
    return HDB_OK;
}

int CHdbQuerySqlBuilder::ResolveOrderFields(const CHdbQueryAst& ast,
    const std::vector<ResolvedSource>& sources,
    std::vector<ResolvedField>& fields)
{
    size_t i;

    fields.clear();
    for (i = 0; i < ast.orders.size(); ++i)
    {
        ResolvedField field;
        int ret;

        if (ValidateOrderType(ast.orders[i].orderType) != HDB_OK)
        {
            SetLastError("invalid order type");
            return HDB_ERR_QUERY_RANGE;
        }
        ret = ResolveFieldRef(sources, ast.orders[i].field, field);
        if (ret != HDB_OK)
        {
            return ret;
        }
        fields.push_back(field);
    }
    return HDB_OK;
}

int CHdbQuerySqlBuilder::ResolveFieldRef(const std::vector<ResolvedSource>& sources,
    const HdbQueryFieldRef& fieldRef,
    ResolvedField& outField)
{
    const ResolvedSource* source;
    const HdbFieldDef* field;

    source = FindResolvedSource(sources, fieldRef.sourceId);
    if (source == NULL || source->dataset == NULL)
    {
        SetLastError("field source is not found");
        return HDB_ERR_FIELD_REF;
    }
    if (m_registry->ValidateIdentifier(fieldRef.fieldName.c_str()) != HDB_OK)
    {
        SetLastError(m_registry->GetLastError());
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    field = m_registry->FindField(*source->dataset, fieldRef.fieldName.c_str());
    if (field == NULL)
    {
        SetLastError("field is not found in source dataset");
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    outField.sourceId = fieldRef.sourceId;
    outField.dataset = source->dataset;
    outField.field = field;
    outField.sqlAlias = source->sqlAlias;
    return HDB_OK;
}

int CHdbQuerySqlBuilder::CollectRootColumns(const CHdbQueryAst& ast,
    const std::vector<ResolvedSource>& sources,
    const std::vector<ResolvedField>& selectFields,
    const std::vector<ResolvedField>& whereFields,
    const std::vector<ResolvedField>& orderFields,
    std::vector<std::string>& rootColumns)
{
    const ResolvedSource* rootSource;
    size_t i;

    (void)ast;
    rootColumns.clear();
    rootSource = FindResolvedSource(sources, 0);
    if (rootSource == NULL || rootSource->dataset == NULL)
    {
        SetLastError("root source is missing");
        return HDB_ERR_PARAM;
    }
    if (rootSource->dataset->shard.shardType == HDB_SHARD_DAY)
    {
        const HdbFieldDef* routeField = m_registry->FindField(*rootSource->dataset,
            rootSource->dataset->shard.routeFieldName);
        if (routeField == NULL)
        {
            SetLastError("route field is not found");
            return HDB_ERR_SHARD_DEF;
        }
        AddRootColumn(rootColumns, routeField->columnName);
    }
    for (i = 0; i < selectFields.size(); ++i)
    {
        if (selectFields[i].sourceId == 0)
        {
            AddRootColumn(rootColumns, selectFields[i].field->columnName);
        }
    }
    for (i = 0; i < whereFields.size(); ++i)
    {
        if (whereFields[i].sourceId == 0)
        {
            AddRootColumn(rootColumns, whereFields[i].field->columnName);
        }
    }
    for (i = 0; i < orderFields.size(); ++i)
    {
        if (orderFields[i].sourceId == 0)
        {
            AddRootColumn(rootColumns, orderFields[i].field->columnName);
        }
    }
    for (i = 1; i < sources.size(); ++i)
    {
        if (sources[i].parentSourceId == 0)
        {
            if (sources[i].localField == NULL)
            {
                SetLastError("root join local field is missing");
                return HDB_ERR_ASSOCIATION_NOT_FOUND;
            }
            AddRootColumn(rootColumns, sources[i].localField->columnName);
        }
    }
    return HDB_OK;
}

int CHdbQuerySqlBuilder::AddRootColumn(std::vector<std::string>& rootColumns, const char* columnName)
{
    size_t i;

    if (columnName == NULL || columnName[0] == '\0')
    {
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    for (i = 0; i < rootColumns.size(); ++i)
    {
        if (rootColumns[i] == columnName)
        {
            return HDB_OK;
        }
    }
    rootColumns.push_back(columnName);
    return HDB_OK;
}

const CHdbQuerySqlBuilder::ResolvedSource* CHdbQuerySqlBuilder::FindResolvedSource(
    const std::vector<ResolvedSource>& sources,
    int sourceId) const
{
    size_t i;

    for (i = 0; i < sources.size(); ++i)
    {
        if (sources[i].sourceId == sourceId)
        {
            return &sources[i];
        }
    }
    return NULL;
}

int CHdbQuerySqlBuilder::BuildRootSource(const CHdbQueryAst& ast,
    const ResolvedSource& rootSource,
    const std::vector<std::string>& rootColumns,
    const std::vector<ResolvedField>& whereFields,
    HdbBuiltQuery& outQuery,
    std::string& outSource)
{
    std::vector<std::string> tableNames;
    std::ostringstream sql;
    std::string pushdownWhere;
    size_t i;
    size_t c;
    int ret;
    int beginParam;
    int endParam;
    const HdbFieldDef* routeField;

    outSource.clear();
    ret = m_router.ResolveQueryTables(*rootSource.dataset, ast.beginMs, ast.endMs, tableNames);
    if (ret != HDB_OK)
    {
        SetLastError(m_router.GetLastError());
        return ret;
    }
    if (rootSource.dataset->shard.shardType != HDB_SHARD_DAY)
    {
        outSource = tableNames[0] + " " + rootSource.sqlAlias;
        return HDB_OK;
    }
    if (rootColumns.empty())
    {
        SetLastError("root column list is empty");
        return HDB_ERR_QUERY_RANGE;
    }
    routeField = m_registry->FindField(*rootSource.dataset, rootSource.dataset->shard.routeFieldName);
    if (routeField == NULL)
    {
        SetLastError("route field is not found");
        return HDB_ERR_SHARD_DEF;
    }
    beginParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.beginMs));
    endParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.endMs));
    // 对日分片 root 字段的 where 下推到每个分片子查询
    for (i = 0; i < whereFields.size(); ++i)
    {
        const char* opText;
        std::string paramValue;
        int paramIndex;
        int formatRet;

        if (whereFields[i].sourceId != 0)
        {
            continue;
        }
        opText = OpToSql(ast.wheres[i].op);
        if (opText == NULL)
        {
            SetLastError("unsupported root compare op");
            return HDB_ERR_QUERY_RANGE;
        }
        formatRet = FormatWhereParamValue(whereFields[i], ast.wheres[i], paramValue);
        if (formatRet != HDB_OK)
        {
            return formatRet;
        }
        paramIndex = AddParam(outQuery, paramValue);
        if (!pushdownWhere.empty())
        {
            pushdownWhere += " and ";
        }
        pushdownWhere += whereFields[i].field->columnName;
        pushdownWhere += " ";
        pushdownWhere += opText;
        pushdownWhere += " ";
        pushdownWhere += Placeholder(paramIndex);
    }
    sql << "(";
    // 日分片按物理表 union all 成 root source，后续 JOIN 只面对 s0
    for (i = 0; i < tableNames.size(); ++i)
    {
        if (i > 0)
        {
            sql << " union all ";
        }
        sql << "select ";
        for (c = 0; c < rootColumns.size(); ++c)
        {
            if (c > 0)
            {
                sql << ", ";
            }
            sql << rootColumns[c];
        }
        sql << " from " << tableNames[i]
            << " where " << routeField->columnName << " >= " << Placeholder(beginParam)
            << " and " << routeField->columnName << " < " << Placeholder(endParam);
        if (!pushdownWhere.empty())
        {
            sql << " and " << pushdownWhere;
        }
    }
    sql << ") " << rootSource.sqlAlias;
    outSource = sql.str();
    return HDB_OK;
}

int CHdbQuerySqlBuilder::BuildJoins(const std::vector<ResolvedSource>& sources, std::string& outSql)
{
    std::ostringstream sql;
    size_t i;

    outSql.clear();
    for (i = 1; i < sources.size(); ++i)
    {
        const ResolvedSource& source = sources[i];
        const ResolvedSource* parentSource;

        parentSource = FindResolvedSource(sources, source.parentSourceId);
        if (parentSource == NULL ||
            parentSource->dataset == NULL ||
            source.dataset == NULL ||
            source.localField == NULL ||
            source.targetField == NULL)
        {
            SetLastError("join source is incomplete");
            return HDB_ERR_ASSOCIATION_NOT_FOUND;
        }
        if (i > 1)
        {
            sql << " ";
        }
        sql << (source.joinType == HDB_JOIN_INNER ? "inner join " : "left join ")
            << source.dataset->shard.tableName << " " << source.sqlAlias
            << " on " << parentSource->sqlAlias << "." << source.localField->columnName
            << " = " << source.sqlAlias << "." << source.targetField->columnName;
    }
    outSql = sql.str();
    return HDB_OK;
}

int CHdbQuerySqlBuilder::BuildWhere(const CHdbQueryAst& ast,
    const ResolvedSource& rootSource,
    const std::vector<ResolvedField>& whereFields,
    HdbBuiltQuery& outQuery,
    std::string& outSql)
{
    std::ostringstream sql;
    size_t i;
    int first;

    outSql.clear();
    first = 1;
    if (rootSource.dataset->shard.shardType == HDB_SHARD_DB_PARTITION && ast.hasTimeRange)
    {
        const HdbFieldDef* routeField = m_registry->FindField(*rootSource.dataset,
            rootSource.dataset->shard.routeFieldName);
        int beginParam;
        int endParam;
        if (routeField == NULL)
        {
            SetLastError("partition route field is not found");
            return HDB_ERR_SHARD_DEF;
        }
        beginParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.beginMs));
        endParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.endMs));
        sql << rootSource.sqlAlias << "." << routeField->columnName << " >= " << Placeholder(beginParam)
            << " and " << rootSource.sqlAlias << "." << routeField->columnName << " < " << Placeholder(endParam);
        first = 0;
    }

    for (i = 0; i < whereFields.size(); ++i)
    {
        std::string expr;
        std::string paramValue;
        const char* opText;
        int paramIndex;
        int formatRet;

        if (rootSource.dataset->shard.shardType == HDB_SHARD_DAY && whereFields[i].sourceId == 0)
        {
            // 已下推到日分片子查询的 root 条件不再重复拼一次
            continue;
        }
        opText = OpToSql(ast.wheres[i].op);
        if (opText == NULL)
        {
            SetLastError("unsupported compare op");
            return HDB_ERR_QUERY_RANGE;
        }
        if (AppendFieldExpr(whereFields[i], expr) != HDB_OK)
        {
            return HDB_ERR_FIELD_REF;
        }
        formatRet = FormatWhereParamValue(whereFields[i], ast.wheres[i], paramValue);
        if (formatRet != HDB_OK)
        {
            return formatRet;
        }
        paramIndex = AddParam(outQuery, paramValue);
        if (!first)
        {
            sql << " and ";
        }
        sql << expr << " " << opText << " " << Placeholder(paramIndex);
        first = 0;
    }
    outSql = sql.str();
    return HDB_OK;
}

int CHdbQuerySqlBuilder::BuildOrder(const CHdbQueryAst& ast,
    const std::vector<ResolvedField>& orderFields,
    std::string& outSql)
{
    std::ostringstream sql;
    size_t i;

    outSql.clear();
    for (i = 0; i < orderFields.size(); ++i)
    {
        std::string expr;
        if (AppendFieldExpr(orderFields[i], expr) != HDB_OK)
        {
            return HDB_ERR_FIELD_REF;
        }
        if (i > 0)
        {
            sql << ", ";
        }
        sql << expr << (ast.orders[i].orderType == HDB_ORDER_DESC ? " desc" : " asc");
    }
    outSql = sql.str();
    return HDB_OK;
}

int CHdbQuerySqlBuilder::AppendFieldExpr(const ResolvedField& field, std::string& outExpr)
{
    if (field.field == NULL || field.sqlAlias.empty())
    {
        SetLastError("resolved field is invalid");
        return HDB_ERR_FIELD_REF;
    }
    outExpr = field.sqlAlias;
    outExpr += ".";
    outExpr += field.field->columnName;
    return HDB_OK;
}

int CHdbQuerySqlBuilder::FormatWhereParamValue(const ResolvedField& field,
    const HdbQueryWhereItem& whereItem,
    std::string& outValue)
{
    int intValue;
    HdbInt64 int64Value;

    outValue.clear();
    // 参数值按字段类型二次校验，SQL builder 是 SERVER 侧最后一道类型边界
    if (field.field == NULL)
    {
        SetLastError("where field is NULL");
        return HDB_ERR_FIELD_REF;
    }
    if (whereItem.op == HDB_OP_LIKE && field.field->type != HDB_FT_CHAR_ARRAY)
    {
        SetLastError("like operator requires string field");
        return HDB_ERR_TYPE_MISMATCH;
    }
    switch (field.field->type)
    {
    case HDB_FT_INT32:
        if (whereItem.valueType != HDB_QVT_INT32)
        {
            SetLastError("int32 field requires int32 value");
            return HDB_ERR_TYPE_MISMATCH;
        }
        if (HdbBuilderParseInt32Strict(whereItem.valueText, &intValue) != HDB_OK)
        {
            SetLastError("int32 where value is invalid");
            return HDB_ERR_QUERY_RANGE;
        }
        outValue = whereItem.valueText;
        return HDB_OK;
    case HDB_FT_SMALLINT:
        if (whereItem.valueType != HDB_QVT_INT32)
        {
            SetLastError("smallint field requires int32 value");
            return HDB_ERR_TYPE_MISMATCH;
        }
        if (HdbBuilderParseInt32Strict(whereItem.valueText, &intValue) != HDB_OK ||
            intValue < -32768 ||
            intValue > 32767)
        {
            SetLastError("smallint where value is out of range");
            return HDB_ERR_QUERY_RANGE;
        }
        outValue = whereItem.valueText;
        return HDB_OK;
    case HDB_FT_INT64:
        if (whereItem.valueType != HDB_QVT_INT64)
        {
            SetLastError("int64 field requires int64 value");
            return HDB_ERR_TYPE_MISMATCH;
        }
        if (HdbBuilderParseInt64Strict(whereItem.valueText, &int64Value) != HDB_OK)
        {
            SetLastError("int64 where value is invalid");
            return HDB_ERR_QUERY_RANGE;
        }
        outValue = whereItem.valueText;
        return HDB_OK;
    case HDB_FT_DOUBLE:
        if (whereItem.valueType != HDB_QVT_DOUBLE)
        {
            SetLastError("double field requires double value");
            return HDB_ERR_TYPE_MISMATCH;
        }
        if (HdbBuilderParseDoubleStrict(whereItem.valueText) != HDB_OK)
        {
            SetLastError("double where value is invalid");
            return HDB_ERR_QUERY_RANGE;
        }
        outValue = whereItem.valueText;
        return HDB_OK;
    case HDB_FT_CHAR_ARRAY:
        if (whereItem.valueType != HDB_QVT_STRING)
        {
            SetLastError("string field requires string value");
            return HDB_ERR_TYPE_MISMATCH;
        }
        outValue = whereItem.valueText;
        return HDB_OK;
    case HDB_FT_TIMESTAMP_MS:
        if (whereItem.valueType != HDB_QVT_INT64)
        {
            SetLastError("timestamp_ms field requires int64 epoch ms value");
            return HDB_ERR_TYPE_MISMATCH;
        }
        if (HdbBuilderParseInt64Strict(whereItem.valueText, &int64Value) != HDB_OK)
        {
            SetLastError("timestamp_ms where value is invalid");
            return HDB_ERR_QUERY_RANGE;
        }
        outValue = FormatTimestampMs(int64Value);
        if (outValue.empty())
        {
            SetLastError("timestamp_ms where value conversion failed");
            return HDB_ERR_QUERY_RANGE;
        }
        return HDB_OK;
    default:
        SetLastError("unsupported where field type");
        return HDB_ERR_FIELD_REF;
    }
}

int CHdbQuerySqlBuilder::AddParam(HdbBuiltQuery& query, const std::string& value)
{
    query.params.push_back(value);
    return (int)query.params.size();
}

std::string CHdbQuerySqlBuilder::Placeholder(int index) const
{
    std::ostringstream out;
    out << "$" << index;
    return out.str();
}

std::string CHdbQuerySqlBuilder::FormatTimestampMs(HdbInt64 value) const
{
    time_t seconds;
    int millis;
    struct tm tmValue;
    char buffer[64];

    seconds = (time_t)(value / 1000);
    millis = (int)(value % 1000);
    if (millis < 0)
    {
        // 负时间戳需要把毫秒余数修正到同一天内的正数
        millis += 1000;
        --seconds;
    }
    memset(&tmValue, 0, sizeof(tmValue));
    if (HdbBuilderLocalTime(&tmValue, &seconds) != 0)
    {
        return "";
    }
    HDB_SNPRINTF(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        tmValue.tm_year + 1900,
        tmValue.tm_mon + 1,
        tmValue.tm_mday,
        tmValue.tm_hour,
        tmValue.tm_min,
        tmValue.tm_sec,
        millis);
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
}

const char* CHdbQuerySqlBuilder::OpToSql(int op) const
{
    switch (op)
    {
    case HDB_OP_EQ:
        return "=";
    case HDB_OP_NE:
        return "<>";
    case HDB_OP_GT:
        return ">";
    case HDB_OP_GE:
        return ">=";
    case HDB_OP_LT:
        return "<";
    case HDB_OP_LE:
        return "<=";
    case HDB_OP_LIKE:
        return "like";
    default:
        return NULL;
    }
}

int CHdbQuerySqlBuilder::ValidateOrderType(int orderType) const
{
    return (orderType == HDB_ORDER_ASC || orderType == HDB_ORDER_DESC) ? HDB_OK : HDB_ERR_QUERY_RANGE;
}

int CHdbQuerySqlBuilder::ValidateJoinType(int joinType) const
{
    return (joinType == HDB_JOIN_INNER || joinType == HDB_JOIN_LEFT) ? HDB_OK : HDB_ERR_QUERY_RANGE;
}

int CHdbQuerySqlBuilder::ValidateJoinTargetDataset(const HdbDatasetDef& dataset)
{
    int ret;

    ret = m_registry->ValidateDataset(dataset);
    if (ret != HDB_OK)
    {
        SetLastError(m_registry->GetLastError());
        return ret;
    }
    if (dataset.shard.shardType == HDB_SHARD_DAY)
    {
        // 当前 JOIN 目标只支持固定表或数据库分区，日分片目标会让别名和分片范围都变复杂
        SetLastError("day sharded association target is not supported");
        return HDB_ERR_SHARD_DEF;
    }
    return HDB_OK;
}

std::string CHdbQuerySqlBuilder::AliasForSource(int sourceId) const
{
    std::string alias = "s";
    alias += IntToString(sourceId);
    return alias;
}

void CHdbQuerySqlBuilder::SetLastError(const char* text)
{
    if (text == NULL || text[0] == '\0')
    {
        m_lastError = "unknown query sql builder error";
    }
    else
    {
        m_lastError = text;
    }
}
