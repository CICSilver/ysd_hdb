#ifndef YSD_HDB_IPC_RESULT_CODEC_H
#define YSD_HDB_IPC_RESULT_CODEC_H

#include "HdbIpcProtocol.h"

#include <string>
#include <vector>

struct HdbIpcResultCell
{
    std::string value;
    int isNull;
};

struct HdbIpcResultSet
{
    std::vector<std::string> columns;
    std::vector< std::vector<HdbIpcResultCell> > rows;

    void Clear();
};

int HdbIpcEncodeResultSchema(const HdbIpcResultSet& result, std::vector<unsigned char>& outData);
int HdbIpcDecodeResultSchema(const void* data, unsigned int length, HdbIpcResultSet& result);

int HdbIpcEncodeResultRows(const HdbIpcResultSet& result, std::vector<unsigned char>& outData);
int HdbIpcDecodeResultRows(const void* data, unsigned int length, HdbIpcResultSet& result);

#endif
