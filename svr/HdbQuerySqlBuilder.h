#ifndef YSD_HDB_QUERY_SQL_BUILDER_H
#define YSD_HDB_QUERY_SQL_BUILDER_H

#include "HdbDatasetRegistry.h"
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
    int BuildExecute(const CHdbQueryAst& ast, HdbBuiltQuery& outQuery);
    const char* GetLastError() const;

private:
    struct ResolvedSource
    {
        int sourceId;                         // AST sourceId
        int parentSourceId;                   // JOIN 父 source
        const HdbDatasetDef* dataset;         // 当前 source 对应的数据集
        const HdbAssociationDef* association; // JOIN 使用的 Association，ROOT 为空
        const HdbFieldDef* localField;        // 父 source 上参与 ON 的字段
        const HdbFieldDef* targetField;       // 当前 source 上参与 ON 的字段
        int joinType;                         // HdbJoinType
        std::string sqlAlias;                 // s0/s1/s2
    };

    struct ResolvedField
    {
        int sourceId;                 // 字段所属 source
        const HdbDatasetDef* dataset; // 字段所属数据集
        const HdbFieldDef* field;     // 字段元数据
        std::string sqlAlias;         // 字段 SQL 别名
    };

    int ResolveSources(const CHdbQueryAst& ast, std::vector<ResolvedSource>& sources);
    int ResolveSelectFields(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        std::vector<ResolvedField>& fields);
    int ResolveWhereFields(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        std::vector<ResolvedField>& fields);
    int ResolveOrderFields(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        std::vector<ResolvedField>& fields);
    int ResolveSetFields(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        std::vector<ResolvedField>& fields);
    int ResolveConditionFields(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        std::vector<ResolvedField>& fields);
    int ResolveFieldRef(const std::vector<ResolvedSource>& sources,
        const HdbQueryFieldRef& fieldRef,
        ResolvedField& outField);
    int CollectRootColumns(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        const std::vector<ResolvedField>& selectFields,
        const std::vector<ResolvedField>& whereFields,
        const std::vector<ResolvedField>& conditionFields,
        const std::vector<ResolvedField>& orderFields,
        std::vector<std::string>& rootColumns);
    int AddRootColumn(std::vector<std::string>& rootColumns, const char* columnName);
    const ResolvedSource* FindResolvedSource(const std::vector<ResolvedSource>& sources, int sourceId) const;
    int BuildRootSource(const CHdbQueryAst& ast,
        const ResolvedSource& rootSource,
        const std::vector<std::string>& rootColumns,
        const std::vector<ResolvedField>& whereFields,
        HdbBuiltQuery& outQuery,
        std::string& outSource);
    int BuildJoins(const std::vector<ResolvedSource>& sources, std::string& outSql);
    int BuildWhere(const CHdbQueryAst& ast,
        const ResolvedSource& rootSource,
        const std::vector<ResolvedSource>& sources,
        const std::vector<ResolvedField>& whereFields,
        HdbBuiltQuery& outQuery,
        std::string& outSql);
    int BuildConditionSql(const CHdbQueryAst& ast,
        const std::vector<ResolvedSource>& sources,
        int nodeId,
        HdbBuiltQuery& outQuery,
        std::string& outSql);
    int BuildOrder(const CHdbQueryAst& ast,
        const std::vector<ResolvedField>& orderFields,
        std::string& outSql);
    int BuildInsert(const CHdbQueryAst& ast,
        const ResolvedSource& rootSource,
        const std::vector<ResolvedField>& setFields,
        HdbBuiltQuery& outQuery);
    int BuildUpdate(const CHdbQueryAst& ast,
        const ResolvedSource& rootSource,
        const std::vector<ResolvedField>& setFields,
        HdbBuiltQuery& outQuery);
    int BuildDelete(const CHdbQueryAst& ast,
        const ResolvedSource& rootSource,
        HdbBuiltQuery& outQuery);
    int ResolveDmlTableName(const CHdbQueryAst& ast,
        const ResolvedSource& rootSource,
        const std::vector<ResolvedField>& setFields,
        std::string& outTableName);
    int AppendFieldExpr(const ResolvedField& field, std::string& outExpr);
    // where 参数值在这里按字段类型转成数据库文本
    int FormatWhereParamValue(const ResolvedField& field,
        const HdbQueryWhereItem& whereItem,
        std::string& outValue);
    int FormatValueForField(const ResolvedField& field,
        int valueType,
        const std::string& valueText,
        std::string& outValue);
    int AddParam(HdbBuiltQuery& query, const std::string& value);
    std::string Placeholder(int index) const;
    std::string FormatTimestampMs(HdbInt64 value) const;
    const char* OpToSql(int op) const;
    int ValidateOrderType(int orderType) const;
    int ValidateJoinType(int joinType) const;
    int ValidateJoinTargetDataset(const HdbDatasetDef& dataset);
    std::string AliasForSource(int sourceId) const;
    void SetLastError(const char* text);

private:
    const CHdbDatasetRegistry* m_registry; // 元数据注册表
    CHdbShardRouter m_router;              // 分片表路由
    std::string m_lastError;               // 最近错误文本
};

#endif
