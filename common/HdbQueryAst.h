#ifndef YSD_HDB_QUERY_AST_H
#define YSD_HDB_QUERY_AST_H

#include "HdbCommon.h"

#include <string>
#include <vector>

typedef HdbInt64 HdbQueryInt64;

#define HDB_QUERY_MAX_SOURCE_COUNT 64

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

struct HdbQuerySourceItem
{
    int sourceId;                 // query 内 source 编号
    int sourceType;               // HdbQuerySourceType
    int parentSourceId;           // JOIN 的父 source，ROOT 固定为 -1
    std::string datasetName;      // ROOT 数据集名，JOIN 由 SERVER 根据 Association 推导
    std::string associationName;  // JOIN 使用的命名 Association
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

class CHdbQueryAst
{
public:
    CHdbQueryAst();

    void Clear();
    int AddRootSource(const char* datasetName, int* outSourceId);
    int AddJoinSource(int parentSourceId, const char* associationName, int joinType, int* outSourceId);
    int AddSelect(int sourceId, const char* fieldName, const char* outputName);
    int AddWhereInt32(int sourceId, const char* fieldName, int op, int value);
    int AddWhereInt64(int sourceId, const char* fieldName, int op, HdbQueryInt64 value);
    int AddWhereDouble(int sourceId, const char* fieldName, int op, double value);
    int AddWhereString(int sourceId, const char* fieldName, int op, const char* value);
    int AddOrder(int sourceId, const char* fieldName, int orderType);
    int SetTimeRange(HdbQueryInt64 beginMs, HdbQueryInt64 endMs);
    int SetLimit(int limit, int offset);

    int HasRootSource() const;
    int FindSourceIndex(int sourceId) const;

    // Serialize 只透出文本，具体格式在 CHdbQueryAstCodec
    int Serialize(std::string& text) const;
    int Deserialize(const char* text);

public:
    std::vector<HdbQuerySourceItem> sources; // 查询源，ROOT 固定 sourceId 0
    int hasTimeRange;                        // 是否带时间范围
    HdbQueryInt64 beginMs;                   // epoch milliseconds 起始值
    HdbQueryInt64 endMs;                     // epoch milliseconds 结束值
    std::vector<HdbQuerySelectItem> selects; // 结果列
    std::vector<HdbQueryWhereItem> wheres;   // AND 条件，当前没有 OR
    std::vector<HdbQueryOrderItem> orders;   // 排序字段
    int limit;                               // 单次最多返回行数
    int offset;                              // 起始偏移

private:
    int AddWhereText(int sourceId,
        const char* fieldName,
        int op,
        int valueType,
        const std::string& valueText);
    int AssignFieldRef(HdbQueryFieldRef& field, int sourceId, const char* fieldName) const;
};

#endif
