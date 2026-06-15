#ifndef YSD_HDB_IPC_RESULT_CODEC_H
#define YSD_HDB_IPC_RESULT_CODEC_H

#include "HdbCommon.h"
#include "HdbIpcProtocol.h"

#include <string>
#include <vector>

#define HDB_IPC_MAX_RESULT_COLUMNS 1024
#define HDB_IPC_MAX_RESULT_ROWS HDB_QUERY_MAX_LIMIT
#define HDB_IPC_MAX_RESULT_CELL_BYTES (1024u * 1024u)
#define HDB_IPC_MAX_QUERY_AST_BYTES (1024u * 1024u)

struct HdbIpcResultColumn
{
    std::string name;
    int fieldType;
};

struct HdbIpcResultCell
{
    std::string value;
    int isNull;
};

struct HdbIpcResultSet
{
    std::vector<HdbIpcResultColumn> columns;
    std::vector< std::vector<HdbIpcResultCell> > rows;

    void Clear();
};

int HdbIpcEncodeResultSchema(const HdbIpcResultSet& result, std::vector<unsigned char>& outData);
int HdbIpcDecodeResultSchema(const void* data, unsigned int length, HdbIpcResultSet& result);

int HdbIpcEncodeResultRows(const HdbIpcResultSet& result, std::vector<unsigned char>& outData);
int HdbIpcDecodeResultRows(const void* data, unsigned int length, HdbIpcResultSet& result);

#endif
