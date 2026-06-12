#include "HdbQuerySqlBuilder.h"

#include <stdio.h>
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

void HdbBuiltQuery::Clear()
{
    sql.clear();
    params.clear();
    outputNames.clear();
}

CHdbQuerySqlBuilder::CHdbQuerySqlBuilder(const CHdbDatasetRegistry* registry)
    : m_registry(registry)
{
}

int CHdbQuerySqlBuilder::BuildSelect(const CHdbQueryAst& ast, HdbBuiltQuery& outQuery)
{
    const HdbDatasetDef* rootDataset;
    std::vector<HdbResolvedFieldPath> selectPaths;
    std::vector<HdbResolvedFieldPath> wherePaths;
    std::vector<HdbResolvedFieldPath> orderPaths;
    std::vector<JoinInfo> joins;
    std::vector<std::string> rootColumns;
    std::string sourceSql;
    std::string joinSql;
    std::string whereSql;
    std::string orderSql;
    std::ostringstream sql;
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
    if (m_registry->ValidateIdentifier(ast.rootDataset.c_str()) != HDB_OK)
    {
        SetLastError(m_registry->GetLastError());
        return HDB_ERR_DATASET_NOT_FOUND;
    }
    rootDataset = m_registry->FindDataset(ast.rootDataset.c_str());
    if (rootDataset == NULL)
    {
        SetLastError("root dataset is not found");
        return HDB_ERR_DATASET_NOT_FOUND;
    }
    ret = m_registry->ValidateDataset(*rootDataset);
    if (ret != HDB_OK)
    {
        SetLastError(m_registry->GetLastError());
        return ret;
    }
    if (ast.selects.empty())
    {
        SetLastError("query has no select fields");
        return HDB_ERR_PARAM;
    }
    if (rootDataset->shard.shardType == HDB_SHARD_DAY && !ast.hasTimeRange)
    {
        SetLastError("day shard query requires time range");
        return HDB_ERR_QUERY_NEED_TIME_RANGE;
    }

    ret = ResolveAndCollect(ast, *rootDataset, selectPaths, wherePaths, orderPaths, joins, rootColumns);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildRootSource(ast, *rootDataset, rootColumns, outQuery, sourceSql);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildJoins(joins, joinSql);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildWhere(ast, *rootDataset, wherePaths, joins, outQuery, whereSql);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = BuildOrder(ast, orderPaths, joins, orderSql);
    if (ret != HDB_OK)
    {
        return ret;
    }

    sql << "select ";
    for (i = 0; i < selectPaths.size(); ++i)
    {
        std::string expr;
        ret = AppendFieldExpr(selectPaths[i], joins, expr);
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

    limitValue = ast.limit > 0 ? ast.limit : 1000;
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

int CHdbQuerySqlBuilder::ResolveAndCollect(const CHdbQueryAst& ast,
    const HdbDatasetDef& rootDataset,
    std::vector<HdbResolvedFieldPath>& selectPaths,
    std::vector<HdbResolvedFieldPath>& wherePaths,
    std::vector<HdbResolvedFieldPath>& orderPaths,
    std::vector<JoinInfo>& joins,
    std::vector<std::string>& rootColumns)
{
    CHdbFieldPathResolver resolver(m_registry);
    size_t i;
    int ret;

    if (rootDataset.shard.shardType == HDB_SHARD_DAY)
    {
        const HdbFieldDef* routeField = m_registry->FindField(rootDataset, rootDataset.shard.routeFieldName);
        if (routeField == NULL)
        {
            SetLastError("route field is not found");
            return HDB_ERR_SHARD_DEF;
        }
        AddRootColumn(rootColumns, routeField->columnName);
    }

    for (i = 0; i < ast.selects.size(); ++i)
    {
        HdbResolvedFieldPath path;
        ret = resolver.Resolve(rootDataset, ast.selects[i].fieldPath.c_str(), path);
        if (ret != HDB_OK)
        {
            SetLastError(resolver.GetLastError());
            return ret;
        }
        ret = AddRelationJoins(path, joins);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (path.relations.empty())
        {
            AddRootColumn(rootColumns, path.field->columnName);
        }
        selectPaths.push_back(path);
    }
    for (i = 0; i < ast.wheres.size(); ++i)
    {
        HdbResolvedFieldPath path;
        ret = resolver.Resolve(rootDataset, ast.wheres[i].fieldPath.c_str(), path);
        if (ret != HDB_OK)
        {
            SetLastError(resolver.GetLastError());
            return ret;
        }
        ret = AddRelationJoins(path, joins);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (path.relations.empty())
        {
            AddRootColumn(rootColumns, path.field->columnName);
        }
        wherePaths.push_back(path);
    }
    for (i = 0; i < ast.orders.size(); ++i)
    {
        HdbResolvedFieldPath path;
        if (ValidateOrderType(ast.orders[i].orderType) != HDB_OK)
        {
            SetLastError("invalid order type");
            return HDB_ERR_QUERY_RANGE;
        }
        ret = resolver.Resolve(rootDataset, ast.orders[i].fieldPath.c_str(), path);
        if (ret != HDB_OK)
        {
            SetLastError(resolver.GetLastError());
            return ret;
        }
        ret = AddRelationJoins(path, joins);
        if (ret != HDB_OK)
        {
            return ret;
        }
        if (path.relations.empty())
        {
            AddRootColumn(rootColumns, path.field->columnName);
        }
        orderPaths.push_back(path);
    }
    for (i = 0; i < joins.size(); ++i)
    {
        if (strcmp(joins[i].fromAlias.c_str(), "r") == 0)
        {
            const HdbFieldDef* fromField = m_registry->FindField(*joins[i].fromDataset, joins[i].relation->fromFieldName);
            if (fromField == NULL)
            {
                SetLastError("join root field is not found");
                return HDB_ERR_FIELD_NOT_FOUND;
            }
            AddRootColumn(rootColumns, fromField->columnName);
        }
    }
    return HDB_OK;
}

int CHdbQuerySqlBuilder::AddRelationJoins(const HdbResolvedFieldPath& path, std::vector<JoinInfo>& joins)
{
    size_t i;

    for (i = 0; i < path.relations.size(); ++i)
    {
        const HdbResolvedRelationStep& step = path.relations[i];
        JoinInfo join;
        int exists;

        exists = FindJoin(joins, step.path.c_str());
        if (exists >= 0)
        {
            continue;
        }
        if (ValidateRelationDataset(*step.toDataset) != HDB_OK)
        {
            return HDB_ERR_SHARD_DEF;
        }
        join.path = step.path;
        join.relation = step.relation;
        join.fromDataset = step.fromDataset;
        join.toDataset = step.toDataset;
        if (i == 0)
        {
            join.fromAlias = "r";
        }
        else
        {
            int parentIndex;
            parentIndex = FindJoin(joins, path.relations[i - 1].path.c_str());
            if (parentIndex < 0)
            {
                SetLastError("parent join is missing");
                return HDB_ERR_RELATION_NOT_FOUND;
            }
            join.fromAlias = joins[parentIndex].toAlias;
        }
        join.toAlias = "j";
        join.toAlias += IntToString((int)joins.size() + 1);
        joins.push_back(join);
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

int CHdbQuerySqlBuilder::FindJoin(const std::vector<JoinInfo>& joins, const char* path) const
{
    size_t i;

    if (path == NULL)
    {
        return -1;
    }
    for (i = 0; i < joins.size(); ++i)
    {
        if (joins[i].path == path)
        {
            return (int)i;
        }
    }
    return -1;
}

int CHdbQuerySqlBuilder::BuildRootSource(const CHdbQueryAst& ast,
    const HdbDatasetDef& rootDataset,
    const std::vector<std::string>& rootColumns,
    HdbBuiltQuery& outQuery,
    std::string& outSource)
{
    std::vector<std::string> tableNames;
    std::ostringstream sql;
    size_t i;
    size_t c;
    int ret;
    int beginParam;
    int endParam;
    const HdbFieldDef* routeField;

    outSource.clear();
    ret = m_router.ResolveQueryTables(rootDataset, ast.beginMs, ast.endMs, tableNames);
    if (ret != HDB_OK)
    {
        SetLastError(m_router.GetLastError());
        return ret;
    }
    if (rootDataset.shard.shardType != HDB_SHARD_DAY)
    {
        outSource = tableNames[0] + " r";
        return HDB_OK;
    }
    if (rootColumns.empty())
    {
        SetLastError("root column list is empty");
        return HDB_ERR_QUERY_RANGE;
    }

    routeField = m_registry->FindField(rootDataset, rootDataset.shard.routeFieldName);
    if (routeField == NULL)
    {
        SetLastError("route field is not found");
        return HDB_ERR_SHARD_DEF;
    }
    beginParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.beginMs));
    endParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.endMs));
    sql << "(";
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
    }
    sql << ") r";
    outSource = sql.str();
    return HDB_OK;
}

int CHdbQuerySqlBuilder::BuildJoins(const std::vector<JoinInfo>& joins, std::string& outSql)
{
    std::ostringstream sql;
    size_t i;

    outSql.clear();
    for (i = 0; i < joins.size(); ++i)
    {
        const JoinInfo& join = joins[i];
        const HdbFieldDef* fromField;
        const HdbFieldDef* toField;

        fromField = m_registry->FindField(*join.fromDataset, join.relation->fromFieldName);
        toField = m_registry->FindField(*join.toDataset, join.relation->toFieldName);
        if (fromField == NULL || toField == NULL)
        {
            SetLastError("join field is not found");
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        if (i > 0)
        {
            sql << " ";
        }
        sql << (join.relation->joinType == HDB_JOIN_INNER ? "inner join " : "left join ")
            << join.toDataset->shard.tableName << " " << join.toAlias
            << " on " << join.fromAlias << "." << fromField->columnName
            << " = " << join.toAlias << "." << toField->columnName;
    }
    outSql = sql.str();
    return HDB_OK;
}

int CHdbQuerySqlBuilder::BuildWhere(const CHdbQueryAst& ast,
    const HdbDatasetDef& rootDataset,
    const std::vector<HdbResolvedFieldPath>& wherePaths,
    const std::vector<JoinInfo>& joins,
    HdbBuiltQuery& outQuery,
    std::string& outSql)
{
    std::ostringstream sql;
    size_t i;
    int first;

    outSql.clear();
    first = 1;
    if (rootDataset.shard.shardType == HDB_SHARD_DB_PARTITION && ast.hasTimeRange)
    {
        const HdbFieldDef* routeField = m_registry->FindField(rootDataset, rootDataset.shard.routeFieldName);
        int beginParam;
        int endParam;
        if (routeField == NULL)
        {
            SetLastError("partition route field is not found");
            return HDB_ERR_SHARD_DEF;
        }
        beginParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.beginMs));
        endParam = AddParam(outQuery, FormatTimestampMs((HdbInt64)ast.endMs));
        sql << "r." << routeField->columnName << " >= " << Placeholder(beginParam)
            << " and r." << routeField->columnName << " < " << Placeholder(endParam);
        first = 0;
    }

    for (i = 0; i < wherePaths.size(); ++i)
    {
        std::string expr;
        const char* opText;
        int paramIndex;

        opText = OpToSql(ast.wheres[i].op);
        if (opText == NULL)
        {
            SetLastError("unsupported compare op");
            return HDB_ERR_QUERY_RANGE;
        }
        if (AppendFieldExpr(wherePaths[i], joins, expr) != HDB_OK)
        {
            return HDB_ERR_FIELD_PATH;
        }
        paramIndex = AddParam(outQuery, ast.wheres[i].valueText);
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
    const std::vector<HdbResolvedFieldPath>& orderPaths,
    const std::vector<JoinInfo>& joins,
    std::string& outSql)
{
    std::ostringstream sql;
    size_t i;

    outSql.clear();
    for (i = 0; i < orderPaths.size(); ++i)
    {
        std::string expr;
        if (AppendFieldExpr(orderPaths[i], joins, expr) != HDB_OK)
        {
            return HDB_ERR_FIELD_PATH;
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

int CHdbQuerySqlBuilder::AppendFieldExpr(const HdbResolvedFieldPath& path,
    const std::vector<JoinInfo>& joins,
    std::string& outExpr)
{
    std::string alias;

    if (path.field == NULL)
    {
        SetLastError("resolved field is NULL");
        return HDB_ERR_FIELD_PATH;
    }
    if (path.relations.empty())
    {
        alias = "r";
    }
    else
    {
        int joinIndex = FindJoin(joins, path.relations[path.relations.size() - 1].path.c_str());
        if (joinIndex < 0)
        {
            SetLastError("field join is not found");
            return HDB_ERR_RELATION_NOT_FOUND;
        }
        alias = joins[joinIndex].toAlias;
    }
    outExpr = alias;
    outExpr += ".";
    outExpr += path.field->columnName;
    return HDB_OK;
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

int CHdbQuerySqlBuilder::ValidateRelationDataset(const HdbDatasetDef& dataset)
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
        SetLastError("day sharded relation target is not supported in first query version");
        return HDB_ERR_SHARD_DEF;
    }
    return HDB_OK;
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
