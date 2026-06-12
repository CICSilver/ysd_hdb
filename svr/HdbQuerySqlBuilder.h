#ifndef YSD_HDB_QUERY_SQL_BUILDER_H
#define YSD_HDB_QUERY_SQL_BUILDER_H

#include "HdbFieldPathResolver.h"
#include "HdbShardRouter.h"
#include "../common/HdbQueryAst.h"

#include <string>
#include <vector>

struct HdbBuiltQuery
{
    std::string sql;
    std::vector<std::string> params;
    std::vector<std::string> outputNames;

    void Clear();
};

class CHdbQuerySqlBuilder
{
public:
    explicit CHdbQuerySqlBuilder(const CHdbDatasetRegistry* registry);

    int BuildSelect(const CHdbQueryAst& ast, HdbBuiltQuery& outQuery);
    const char* GetLastError() const;

private:
    struct JoinInfo
    {
        std::string path;
        const HdbRelationDef* relation;
        const HdbDatasetDef* fromDataset;
        const HdbDatasetDef* toDataset;
        std::string fromAlias;
        std::string toAlias;
    };

    int ResolveAndCollect(const CHdbQueryAst& ast,
        const HdbDatasetDef& rootDataset,
        std::vector<HdbResolvedFieldPath>& selectPaths,
        std::vector<HdbResolvedFieldPath>& wherePaths,
        std::vector<HdbResolvedFieldPath>& orderPaths,
        std::vector<JoinInfo>& joins,
        std::vector<std::string>& rootColumns);
    int AddRelationJoins(const HdbResolvedFieldPath& path, std::vector<JoinInfo>& joins);
    int AddRootColumn(std::vector<std::string>& rootColumns, const char* columnName);
    int FindJoin(const std::vector<JoinInfo>& joins, const char* path) const;
    int BuildRootSource(const CHdbQueryAst& ast,
        const HdbDatasetDef& rootDataset,
        const std::vector<std::string>& rootColumns,
        HdbBuiltQuery& outQuery,
        std::string& outSource);
    int BuildJoins(const std::vector<JoinInfo>& joins, std::string& outSql);
    int BuildWhere(const CHdbQueryAst& ast,
        const HdbDatasetDef& rootDataset,
        const std::vector<HdbResolvedFieldPath>& wherePaths,
        const std::vector<JoinInfo>& joins,
        HdbBuiltQuery& outQuery,
        std::string& outSql);
    int BuildOrder(const CHdbQueryAst& ast,
        const std::vector<HdbResolvedFieldPath>& orderPaths,
        const std::vector<JoinInfo>& joins,
        std::string& outSql);
    int AppendFieldExpr(const HdbResolvedFieldPath& path,
        const std::vector<JoinInfo>& joins,
        std::string& outExpr);
    int AddParam(HdbBuiltQuery& query, const std::string& value);
    std::string Placeholder(int index) const;
    std::string FormatTimestampMs(HdbInt64 value) const;
    const char* OpToSql(int op) const;
    int ValidateOrderType(int orderType) const;
    int ValidateRelationDataset(const HdbDatasetDef& dataset);
    void SetLastError(const char* text);

private:
    const CHdbDatasetRegistry* m_registry;
    CHdbShardRouter m_router;
    std::string m_lastError;
};

#endif
