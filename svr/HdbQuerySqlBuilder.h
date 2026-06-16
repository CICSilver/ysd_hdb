#ifndef YSD_HDB_QUERY_SQL_BUILDER_H
#define YSD_HDB_QUERY_SQL_BUILDER_H

#include "HdbFieldPathResolver.h"
#include "HdbShardRouter.h"
#include "../common/HdbQueryAst.h"

#include <string>
#include <vector>

// SQL builder 输出
struct HdbBuiltQuery
{
    std::string sql;                      // 参数化 SQL
    std::vector<std::string> params;      // 占位符参数文本
    std::vector<std::string> outputNames; // DLL 取值列名
    std::vector<int> outputTypes;         // select 输出类型

    void Clear();
};

// 逻辑查询转参数化 SQL
class CHdbQuerySqlBuilder
{
public:
    explicit CHdbQuerySqlBuilder(const CHdbDatasetRegistry* registry);

    int BuildSelect(const CHdbQueryAst& ast, HdbBuiltQuery& outQuery);
    const char* GetLastError() const;

private:
    struct JoinInfo
    {
        std::string path;                 // relation 链路径
        const HdbRelationDef* relation;   // 当前 relation
        const HdbDatasetDef* fromDataset; // 起始数据集
        const HdbDatasetDef* toDataset;   // 目标数据集
        std::string fromAlias;            // 起始表别名
        std::string toAlias;              // 目标表别名
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
        const std::vector<HdbResolvedFieldPath>& wherePaths,
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
    // where 参数值在这里按字段类型转成数据库文本
    int FormatWhereParamValue(const HdbResolvedFieldPath& path,
        const HdbQueryWhereItem& whereItem,
        std::string& outValue);
    int AddParam(HdbBuiltQuery& query, const std::string& value);
    std::string Placeholder(int index) const;
    std::string FormatTimestampMs(HdbInt64 value) const;
    const char* OpToSql(int op) const;
    int ValidateOrderType(int orderType) const;
    int ValidateRelationDataset(const HdbDatasetDef& dataset);
    void SetLastError(const char* text);

private:
    const CHdbDatasetRegistry* m_registry; // 元数据注册表
    CHdbShardRouter m_router;              // 分片表路由
    std::string m_lastError;               // 最近错误文本
};

#endif
