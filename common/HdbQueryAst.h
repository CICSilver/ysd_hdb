#ifndef YSD_HDB_QUERY_AST_H
#define YSD_HDB_QUERY_AST_H

#include <string>
#include <vector>

typedef long long HdbQueryInt64;

enum HdbQueryValueType
{
    HDB_QVT_INT32 = 1, // 32 位整数条件值。
    HDB_QVT_INT64 = 2, // 64 位整数条件值。
    HDB_QVT_DOUBLE = 3, // 双精度浮点条件值。
    HDB_QVT_STRING = 4 // 字符串条件值。
};

struct HdbQuerySelectItem
{
    std::string fieldPath;
    std::string outputName;
};

struct HdbQueryWhereItem
{
    std::string fieldPath;
    int op;
    int valueType;
    std::string valueText;
};

struct HdbQueryOrderItem
{
    std::string fieldPath;
    int orderType;
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

    int Serialize(std::string& text) const;
    int Deserialize(const char* text);

public:
    std::string rootDataset;
    int hasTimeRange;
    HdbQueryInt64 beginMs;
    HdbQueryInt64 endMs;
    std::vector<HdbQuerySelectItem> selects;
    std::vector<HdbQueryWhereItem> wheres;
    std::vector<HdbQueryOrderItem> orders;
    int limit;
    int offset;

private:
    int AddWhereText(const char* fieldPath, int op, int valueType, const std::string& valueText);
};

#endif
