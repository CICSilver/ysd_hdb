#ifndef YSD_HDB_SHARD_ROUTER_H
#define YSD_HDB_SHARD_ROUTER_H

#include "HdbModelDef.h"

#include <string>
#include <vector>

class CHdbShardRouter
{
public:
    CHdbShardRouter();

    int ResolveInsertTable(const HdbDatasetDef& dataset, const void* row, std::string& outTableName);
    int ResolveQueryTables(const HdbDatasetDef& dataset,
        HdbInt64 beginMs,
        HdbInt64 endMs,
        std::vector<std::string>& outTableNames);
    int BuildDayTableName(const HdbDatasetDef& dataset, HdbInt64 timeMs, std::string& outTableName);
    const char* GetLastError() const;

private:
    int ReadRouteTimeMs(const HdbDatasetDef& dataset, const void* row, HdbInt64* outMs);
    int ValidateIdentifier(const char* name);
    HdbInt64 LocalDayStartMs(HdbInt64 timeMs);
    HdbInt64 NextLocalDayStartMs(HdbInt64 dayStartMs);
    void SetLastError(const char* text);

private:
    std::string m_lastError;
};

#endif
