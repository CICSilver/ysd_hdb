#include "HdbShardRouter.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int HdbRouterLocalTime(struct tm* outTm, const time_t* inTime)
{
#ifdef _WIN32
    return localtime_s(outTm, inTime);
#else
    return localtime_r(inTime, outTm) == NULL ? -1 : 0;
#endif
}

CHdbShardRouter::CHdbShardRouter()
{
}

int CHdbShardRouter::ResolveInsertTable(const HdbDatasetDef& dataset, const void* row, std::string& outTableName)
{
    HdbInt64 routeTime;
    int ret;

    outTableName.clear();
    if (dataset.shard.shardType == HDB_SHARD_NONE || dataset.shard.shardType == HDB_SHARD_DB_PARTITION)
    {
        if (ValidateIdentifier(dataset.shard.tableName) != HDB_OK)
        {
            return HDB_ERR_SHARD_DEF;
        }
        outTableName = dataset.shard.tableName;
        return HDB_OK;
    }
    if (dataset.shard.shardType != HDB_SHARD_DAY)
    {
        SetLastError("unsupported shard type");
        return HDB_ERR_SHARD_DEF;
    }

    // 插入只根据单行 route 字段定位目标表
    ret = ReadRouteTimeMs(dataset, row, &routeTime);
    if (ret != HDB_OK)
    {
        return ret;
    }
    return BuildDayTableName(dataset, routeTime, outTableName);
}

int CHdbShardRouter::ResolveQueryTables(const HdbDatasetDef& dataset,
    HdbInt64 beginMs,
    HdbInt64 endMs,
    std::vector<std::string>& outTableNames)
{
    HdbInt64 dayStart;
    HdbInt64 lastDayStart;
    std::string tableName;
    int guard;
    int ret;

    outTableNames.clear();
    if (dataset.shard.shardType == HDB_SHARD_NONE || dataset.shard.shardType == HDB_SHARD_DB_PARTITION)
    {
        if (ValidateIdentifier(dataset.shard.tableName) != HDB_OK)
        {
            return HDB_ERR_SHARD_DEF;
        }
        outTableNames.push_back(dataset.shard.tableName);
        return HDB_OK;
    }
    if (dataset.shard.shardType != HDB_SHARD_DAY)
    {
        SetLastError("unsupported shard type");
        return HDB_ERR_SHARD_DEF;
    }
    if (beginMs >= endMs)
    {
        SetLastError("invalid shard time range");
        return HDB_ERR_QUERY_RANGE;
    }

    // 查询范围是左闭右开，endMs - 1 对应最后一个需要访问的自然日
    dayStart = LocalDayStartMs(beginMs);
    lastDayStart = LocalDayStartMs(endMs - 1);
    guard = 0;
    while (dayStart <= lastDayStart)
    {
        ret = BuildDayTableName(dataset, dayStart, tableName);
        if (ret != HDB_OK)
        {
            return ret;
        }
        outTableNames.push_back(tableName);
        dayStart = NextLocalDayStartMs(dayStart);
        ++guard;
        if (guard > 3660)
        {
            // 防御异常时间换算导致死循环，也限制一次跨太多年表
            SetLastError("shard time range is too large");
            return HDB_ERR_QUERY_RANGE;
        }
    }
    return outTableNames.empty() ? HDB_ERR_SHARD_NOT_FOUND : HDB_OK;
}

int CHdbShardRouter::BuildDayTableName(const HdbDatasetDef& dataset, HdbInt64 timeMs, std::string& outTableName)
{
    time_t seconds;
    struct tm tmValue;
    char suffix[32];

    outTableName.clear();
    if (dataset.shard.shardType != HDB_SHARD_DAY)
    {
        SetLastError("dataset is not day sharded");
        return HDB_ERR_SHARD_DEF;
    }
    if (ValidateIdentifier(dataset.shard.tablePrefix) != HDB_OK)
    {
        return HDB_ERR_SHARD_DEF;
    }

    seconds = (time_t)(timeMs / 1000);
    memset(&tmValue, 0, sizeof(tmValue));
    // 表后缀按本地日生成，和 LocalDayStartMs 的切日规则保持一致
    if (HdbRouterLocalTime(&tmValue, &seconds) != 0)
    {
        SetLastError("local time conversion failed");
        return HDB_ERR_QUERY_RANGE;
    }
    HDB_SNPRINTF(suffix, sizeof(suffix), "_%04d%02d%02d",
        tmValue.tm_year + 1900,
        tmValue.tm_mon + 1,
        tmValue.tm_mday);
    suffix[sizeof(suffix) - 1] = '\0';
    outTableName = dataset.shard.tablePrefix;
    outTableName += suffix;
    return HDB_OK;
}

const char* CHdbShardRouter::GetLastError() const
{
    return m_lastError.c_str();
}

int CHdbShardRouter::ReadRouteTimeMs(const HdbDatasetDef& dataset, const void* row, HdbInt64* outMs)
{
    int i;

    if (row == NULL || outMs == NULL)
    {
        SetLastError("insert row is NULL");
        return HDB_ERR_PARAM;
    }
    for (i = 0; i < dataset.fieldCount; ++i)
    {
        const HdbFieldDef& field = dataset.fields[i];
        if (strcmp(field.fieldName, dataset.shard.routeFieldName) != 0)
        {
            continue;
        }
        if (field.type != HDB_FT_TIMESTAMP_MS && field.type != HDB_FT_INT64)
        {
            SetLastError("route field is not int64 timestamp");
            return HDB_ERR_SHARD_DEF;
        }
        // route 字段从调用方结构体 offset 读取，不额外依赖数据库默认值
        *outMs = *((const HdbInt64*)((const char*)row + field.offset));
        return HDB_OK;
    }
    SetLastError("route field is not found");
    return HDB_ERR_FIELD_NOT_FOUND;
}

int CHdbShardRouter::ValidateIdentifier(const char* name)
{
    int i;

    if (name == NULL || name[0] == '\0')
    {
        SetLastError("empty identifier");
        return HDB_ERR_PARAM;
    }
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_'))
    {
        SetLastError("identifier must start with a letter or underscore");
        return HDB_ERR_PARAM;
    }
    for (i = 1; name[i] != '\0'; ++i)
    {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
        {
            SetLastError("identifier contains invalid characters");
            return HDB_ERR_PARAM;
        }
    }
    return HDB_OK;
}

HdbInt64 CHdbShardRouter::LocalDayStartMs(HdbInt64 timeMs)
{
    time_t seconds;
    struct tm tmValue;

    seconds = (time_t)(timeMs / 1000);
    memset(&tmValue, 0, sizeof(tmValue));
    if (HdbRouterLocalTime(&tmValue, &seconds) != 0)
    {
        return timeMs;
    }
    tmValue.tm_hour = 0;
    tmValue.tm_min = 0;
    tmValue.tm_sec = 0;
    tmValue.tm_isdst = -1;
    return ((HdbInt64)mktime(&tmValue)) * 1000;
}

HdbInt64 CHdbShardRouter::NextLocalDayStartMs(HdbInt64 dayStartMs)
{
    time_t seconds;
    struct tm tmValue;

    seconds = (time_t)(dayStartMs / 1000);
    memset(&tmValue, 0, sizeof(tmValue));
    if (HdbRouterLocalTime(&tmValue, &seconds) != 0)
    {
        return dayStartMs + 24LL * 60LL * 60LL * 1000LL;
    }
    tmValue.tm_mday += 1;
    tmValue.tm_isdst = -1;
    // 通过 mktime 加一天，保留夏令时等本地时间规则
    return ((HdbInt64)mktime(&tmValue)) * 1000;
}

void CHdbShardRouter::SetLastError(const char* text)
{
    if (text == NULL || text[0] == '\0')
    {
        m_lastError = "unknown shard router error";
    }
    else
    {
        m_lastError = text;
    }
}
