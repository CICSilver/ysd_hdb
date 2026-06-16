#ifndef YSD_HDB_SHARD_ROUTER_H
#define YSD_HDB_SHARD_ROUTER_H

#include "HdbModelDef.h"

#include <string>
#include <vector>

// 分片路由
class CHdbShardRouter
{
public:
    CHdbShardRouter();

    // 插入按 routeField 定位单张物理表
    int ResolveInsertTable(const HdbDatasetDef& dataset, const void* row, std::string& outTableName);
    // 查询按时间范围展开日表列表
    int ResolveQueryTables(const HdbDatasetDef& dataset,
        HdbInt64 beginMs,
        HdbInt64 endMs,
        std::vector<std::string>& outTableNames);
    // BuildDayTableName 只拼表名，不访问数据库
    int BuildDayTableName(const HdbDatasetDef& dataset, HdbInt64 timeMs, std::string& outTableName);
    const char* GetLastError() const;

private:
    int ReadRouteTimeMs(const HdbDatasetDef& dataset, const void* row, HdbInt64* outMs);
    int ValidateIdentifier(const char* name);
    HdbInt64 LocalDayStartMs(HdbInt64 timeMs);
    HdbInt64 NextLocalDayStartMs(HdbInt64 dayStartMs);
    void SetLastError(const char* text);

private:
    std::string m_lastError; // 最近错误文本
};

#endif
