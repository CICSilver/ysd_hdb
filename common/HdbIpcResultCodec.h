#ifndef YSD_HDB_IPC_RESULT_CODEC_H
#define YSD_HDB_IPC_RESULT_CODEC_H

#include "HdbCommon.h"
#include "HdbIpcProtocol.h"

#include <string>
#include <vector>

#define HDB_IPC_MAX_RESULT_COLUMNS 1024 // schema 最大列数
#define HDB_IPC_MAX_RESULT_ROWS HDB_QUERY_MAX_LIMIT // 单页最大行数
#define HDB_IPC_MAX_RESULT_CELL_BYTES (1024u * 1024u) // 单元格文本上限
#define HDB_IPC_MAX_QUERY_AST_BYTES (1024u * 1024u) // 查询 AST 文本上限

// result schema 列
struct HdbIpcResultColumn
{
    std::string name; // 输出列名
    int fieldType;    // HdbFieldType
};

struct HdbIpcResultCell
{
    std::string value; // 数据库文本值
    int isNull;        // 1 表示数据库 NULL
};

struct HdbIpcResultSet
{
    std::vector<HdbIpcResultColumn> columns;          // schema 列
    std::vector< std::vector<HdbIpcResultCell> > rows; // 行数据

    void Clear();
};

int HdbIpcEncodeResultSchema(const HdbIpcResultSet& result, std::vector<unsigned char>& outData);
int HdbIpcDecodeResultSchema(const void* data, unsigned int length, HdbIpcResultSet& result);

int HdbIpcEncodeResultRows(const HdbIpcResultSet& result, std::vector<unsigned char>& outData);
int HdbIpcDecodeResultRows(const void* data, unsigned int length, HdbIpcResultSet& result);

#endif
