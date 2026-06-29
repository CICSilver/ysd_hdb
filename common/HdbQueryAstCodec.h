#ifndef YSD_HDB_QUERY_AST_CODEC_H
#define YSD_HDB_QUERY_AST_CODEC_H

#include "HdbQueryAst.h"

#include <string>
#include <vector>

#define HDB_QUERY_AST_MAX_BYTES (1024u * 1024u) // AST 文本字节上限
#define HDB_QUERY_MAX_SELECT_COUNT 128 // select 最大项数
#define HDB_QUERY_MAX_WHERE_COUNT 128 // where 最大项数
#define HDB_QUERY_MAX_CONDITION_COUNT 256 // 条件树最大节点数
#define HDB_QUERY_MAX_SET_COUNT 128 // INSERT/UPDATE 最大 set 项数
#define HDB_QUERY_MAX_IN_VALUE_COUNT 256 // IN 最大值数量
#define HDB_QUERY_MAX_ORDER_COUNT 16 // order 最大项数
#define HDB_QUERY_MAX_TEXT_LENGTH 4096 // 单段文本上限

class CHdbQueryAstCodec
{
public:
    CHdbQueryAstCodec();

    int Encode(const CHdbQueryAst& ast, std::string& outText);
    int Decode(const char* text, CHdbQueryAst& outAst);
    const char* GetLastError() const;

private:
    int ValidateText(const std::string& text, const char* name);
    int ValidateValueText(const std::string& text, const char* name);
    int ValidateNameText(const std::string& text, const char* name);
    int ValidateCompareOp(int op);
    int ValidateFieldCompareOp(int op);
    int ValidateValueType(int valueType);
    int ValidateOrderType(int orderType);
    int ValidateJoinType(int joinType);
    int ValidateStatementType(int statementType);
    int ValidateConditionType(int conditionType);
    int ValidateConditionLogic(int logic);
    int ParseInt32Strict(const std::string& text, int* value);
    int ParseInt64Strict(const std::string& text, HdbQueryInt64* value);
    int ParseDoubleStrict(const std::string& text, double* value);
    int ParsePairInt64(const std::string& text, HdbQueryInt64* first, HdbQueryInt64* second);
    int ParsePairInt32(const std::string& text, int* first, int* second);
    int SplitFields(const std::string& text, std::vector<std::string>& fields);
    void SetLastError(const char* text);

private:
    std::string m_lastError; // 最近错误文本
};

#endif
