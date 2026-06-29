#ifndef YSD_HDB_QUERY_AST_H
#define YSD_HDB_QUERY_AST_H

#include "HdbCommon.h"

#include <string>
#include <vector>

typedef HdbInt64 HdbQueryInt64;

#define HDB_QUERY_MAX_SOURCE_COUNT 64

enum HdbQueryStatementType
{
    HDB_QST_SELECT = 1, // 查询
    HDB_QST_INSERT = 2, // 插入
    HDB_QST_UPDATE = 3, // 更新
    HDB_QST_DELETE = 4 // 删除
};

enum HdbQueryValueType
{
    HDB_QVT_INT32 = 1, // 32 位整数条件值
    HDB_QVT_INT64 = 2, // 64 位整数条件值
    HDB_QVT_DOUBLE = 3, // 双精度浮点条件值
    HDB_QVT_STRING = 4 // 字符串条件值
};

enum HdbQuerySourceType
{
    HDB_SOURCE_ROOT = 1, // 查询根数据集
    HDB_SOURCE_JOIN = 2 // 通过 Association 显式 JOIN 出来的查询源
};

enum HdbQueryConditionType
{
    HDB_QCT_COMPARE = 1, // 字段和值比较
    HDB_QCT_NULL = 2, // IS NULL 或 IS NOT NULL
    HDB_QCT_BETWEEN = 3, // BETWEEN 区间
    HDB_QCT_IN = 4, // IN 值列表
    HDB_QCT_GROUP = 5 // AND/OR 组合
};

enum HdbQueryConditionLogic
{
    HDB_QCL_AND = 1, // AND
    HDB_QCL_OR = 2 // OR
};

struct HdbQuerySourceItem
{
    int sourceId;                 // query 内 source 编号
    int sourceType;               // HdbQuerySourceType
    int parentSourceId;           // JOIN 的父 source，ROOT 固定为 -1
    std::string datasetName;      // ROOT 数据集名，显式 ON JOIN 时为目标数据集名
    std::string associationName;  // 旧 Association JOIN 使用的命名关系
    std::string localFieldName;   // 显式 ON JOIN 的父 source 字段
    std::string targetFieldName;  // 显式 ON JOIN 的目标 source 字段
    int joinType;                 // HdbJoinType，ROOT 固定为 0
};

struct HdbQueryFieldRef
{
    int sourceId;            // 字段所属 source
    std::string fieldName;   // source 数据集上的字段名
};

struct HdbQuerySelectItem
{
    HdbQueryFieldRef field;  // 结果列字段引用
    std::string outputName;  // DLL 取值列名
};

// 当前 where 只有 AND 组合，没有 OR、IN、GROUP BY
struct HdbQueryWhereItem
{
    HdbQueryFieldRef field;  // 条件字段引用
    int op;                  // HdbCompareOp
    int valueType;           // HdbQueryValueType
    std::string valueText;   // valueType 转成的内部文本
};

struct HdbQueryOrderItem
{
    HdbQueryFieldRef field;  // 排序字段引用
    int orderType;           // HdbOrderType
};

struct HdbQueryConditionItem
{
    int nodeId;                       // 条件节点编号
    int conditionType;                // HdbQueryConditionType
    HdbQueryFieldRef field;           // 条件字段
    int op;                           // HdbCompareOp 或 NULL 判断标记
    int valueType;                    // HdbQueryValueType
    std::string valueText;            // 第一个值
    std::string secondValueText;      // BETWEEN 第二个值
    std::vector<std::string> values;  // IN 值列表
    int logic;                        // HdbQueryConditionLogic
    std::vector<int> childNodeIds;    // GROUP 子节点
};

struct HdbQuerySetItem
{
    HdbQueryFieldRef field; // 写入字段
    int valueType;          // HdbQueryValueType
    std::string valueText;  // 写入值
};

class CHdbQueryAst
{
public:
    CHdbQueryAst();

    void Clear();
    int SetStatementType(int statementType);
    int AddRootSource(const char* datasetName, int* outSourceId);
    int AddJoinSource(int parentSourceId, const char* associationName, int joinType, int* outSourceId);
    int AddJoinSourceOn(int parentSourceId,
        const char* targetDatasetName,
        int joinType,
        const char* localFieldName,
        const char* targetFieldName,
        int* outSourceId);
    int AddSelect(int sourceId, const char* fieldName, const char* outputName);
    int AddWhereInt32(int sourceId, const char* fieldName, int op, int value);
    int AddWhereInt64(int sourceId, const char* fieldName, int op, HdbQueryInt64 value);
    int AddWhereDouble(int sourceId, const char* fieldName, int op, double value);
    int AddWhereString(int sourceId, const char* fieldName, int op, const char* value);
    int AddOrder(int sourceId, const char* fieldName, int orderType);
    int AddSetInt32(int sourceId, const char* fieldName, int value);
    int AddSetInt64(int sourceId, const char* fieldName, HdbQueryInt64 value);
    int AddSetDouble(int sourceId, const char* fieldName, double value);
    int AddSetString(int sourceId, const char* fieldName, const char* value);
    int AddConditionCompare(int sourceId,
        const char* fieldName,
        int op,
        int valueType,
        const char* valueText,
        int* outNodeId);
    int AddConditionNull(int sourceId, const char* fieldName, int isNotNull, int* outNodeId);
    int AddConditionBetween(int sourceId,
        const char* fieldName,
        int valueType,
        const char* beginText,
        const char* endText,
        int* outNodeId);
    int AddConditionIn(int sourceId,
        const char* fieldName,
        int valueType,
        const std::vector<std::string>& values,
        int* outNodeId);
    int AddConditionGroup(int logic, const std::vector<int>& childNodeIds, int* outNodeId);
    int SetWhereRoot(int nodeId);
    int SetTimeRange(HdbQueryInt64 beginMs, HdbQueryInt64 endMs);
    int SetLimit(int limit, int offset);

    int HasRootSource() const;
    int FindSourceIndex(int sourceId) const;
    int FindConditionIndex(int nodeId) const;

    // Serialize 只透出文本，具体格式在 CHdbQueryAstCodec
    int Serialize(std::string& text) const;
    int Deserialize(const char* text);

public:
    int statementType;                         // HdbQueryStatementType
    std::vector<HdbQuerySourceItem> sources; // 查询源，ROOT 固定 sourceId 0
    int hasTimeRange;                        // 是否带时间范围
    HdbQueryInt64 beginMs;                   // epoch milliseconds 起始值
    HdbQueryInt64 endMs;                     // epoch milliseconds 结束值
    std::vector<HdbQuerySelectItem> selects; // 结果列
    std::vector<HdbQueryWhereItem> wheres;   // AND 条件，当前没有 OR
    std::vector<HdbQueryConditionItem> conditions; // 条件树节点
    int whereRootNodeId;                     // 条件树根节点
    std::vector<HdbQueryOrderItem> orders;   // 排序字段
    std::vector<HdbQuerySetItem> sets;       // INSERT/UPDATE 写入字段
    int limit;                               // 单次最多返回行数
    int offset;                              // 起始偏移

private:
    int AddWhereText(int sourceId,
        const char* fieldName,
        int op,
        int valueType,
        const std::string& valueText);
    int AddSetText(int sourceId,
        const char* fieldName,
        int valueType,
        const std::string& valueText);
    int AssignFieldRef(HdbQueryFieldRef& field, int sourceId, const char* fieldName) const;
};

#endif
