#ifndef YSD_HDB_QUERY_AST_H
#define YSD_HDB_QUERY_AST_H

#include "HdbCommon.h"

#include <string>
#include <vector>

typedef HdbInt64 HdbQueryInt64;

enum HdbQueryValueType
{
    HDB_QVT_INT32 = 1, // 32 位整数条件值
    HDB_QVT_INT64 = 2, // 64 位整数条件值
    HDB_QVT_DOUBLE = 3, // 双精度浮点条件值
    HDB_QVT_STRING = 4 // 字符串条件值
};

// select 描述结果列
struct HdbQuerySelectItem
{
    std::string fieldPath;  // 逻辑字段路径
    std::string outputName; // DLL 取值列名
};

// 当前 where 只有 AND 组合，没有 OR、IN、GROUP BY
struct HdbQueryWhereItem
{
    std::string fieldPath; // 逻辑字段路径
    int op;                // HdbCompareOp
    int valueType;         // HdbQueryValueType
    std::string valueText; // valueType 转成的内部文本
};

// order 描述排序列
struct HdbQueryOrderItem
{
    std::string fieldPath; // 逻辑字段路径
    int orderType;         // HdbOrderType
};

class CHdbQueryAst
{
public:
    CHdbQueryAst();

    void Clear();
    int SetRootDataset(const char* datasetName);
    int SetTimeRange(HdbQueryInt64 beginMs, HdbQueryInt64 endMs);
    int AddSelect(const char* fieldPath, const char* outputName);
    int AddWhereInt32(const char* fieldPath, int op, int value);
    int AddWhereInt64(const char* fieldPath, int op, HdbQueryInt64 value);
    int AddWhereDouble(const char* fieldPath, int op, double value);
    int AddWhereString(const char* fieldPath, int op, const char* value);
    int AddOrder(const char* fieldPath, int orderType);
    int SetLimit(int limit, int offset);

    // Serialize 只透出文本，具体格式在 CHdbQueryAstCodec
    int Serialize(std::string& text) const;
    int Deserialize(const char* text);

public:
    std::string rootDataset;                 // 逻辑数据集名
    int hasTimeRange;                        // 是否带时间范围
    HdbQueryInt64 beginMs;                   // epoch milliseconds 起始值
    HdbQueryInt64 endMs;                     // epoch milliseconds 结束值
    std::vector<HdbQuerySelectItem> selects; // 结果列
    std::vector<HdbQueryWhereItem> wheres;   // AND 条件，当前没有 OR
    std::vector<HdbQueryOrderItem> orders;   // 排序字段
    int limit;                               // 单次最多返回行数
    int offset;                              // 起始偏移

private:
    int AddWhereText(const char* fieldPath, int op, int valueType, const std::string& valueText);
};

#endif
