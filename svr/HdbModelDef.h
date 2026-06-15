#ifndef YSD_HDB_MODEL_DEF_H
#define YSD_HDB_MODEL_DEF_H

#include "HdbCommon.h"

#include <stddef.h>

// XXX 字段标记用于控制字段参与哪些自动生成的 SQL
enum HdbFieldFlag
{
    HDB_FIELD_PK = 0x01, // 主键字段，用于 WHERE 条件
    HDB_FIELD_INSERT = 0x02, // 参与 INSERT 语句
    HDB_FIELD_UPDATE = 0x04, // 参与 UPDATE SET 语句
    HDB_FIELD_READONLY = 0x08 // 只读字段，即使可见也不参与 UPDATE
};
struct HdbFieldDef
{
    const char* fieldName;
    const char* columnName;
    HdbFieldType type;
    int offset;
    int size;
    int flags;
};

struct HdbModelDef
{
    const char* tableName;
    int modelSize;
    const HdbFieldDef* fields;
    int fieldCount;
};

enum HdbShardType
{
    HDB_SHARD_NONE = 0, // 非分片表，直接访问固定物理表
    HDB_SHARD_DAY = 1, // 按日物理表，表名由前缀和 yyyyMMdd 后缀组成
    HDB_SHARD_DB_PARTITION = 2 // 数据库原生分区，SERVER 查询父表
};

enum HdbMissingShardPolicy
{
    HDB_MISSING_SHARD_IGNORE = 1, // 当前预留，尚未实现物理表存在性检查
    HDB_MISSING_SHARD_ERROR = 2, // 查询时遇到缺失分片直接报错
    HDB_MISSING_SHARD_CREATE = 3 // 插入时允许由上层创建缺失分片
};

struct HdbShardDef
{
    HdbShardType shardType;
    const char* tableName;
    const char* tablePrefix;
    const char* routeFieldName;
    int missingPolicy;
};

struct HdbDatasetDef
{
    const char* datasetName;
    int modelSize;
    const HdbFieldDef* fields;
    int fieldCount;
    HdbShardDef shard;
};
// XXX 字段定义辅助宏只减少重复代码，数据库映射信息仍然显式记录
#define HDB_FIELD_INT32(model, member, column) \
    { column, column, HDB_FT_INT32, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_INT64(model, member, column) \
    { column, column, HDB_FT_INT64, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_DOUBLE(model, member, column) \
    { column, column, HDB_FT_DOUBLE, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_SMALLINT(model, member, column) \
    { column, column, HDB_FT_SMALLINT, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_CHAR(model, member, column, bytes) \
    { column, column, HDB_FT_CHAR_ARRAY, (int)offsetof(model, member), bytes, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_TIMESTAMP_MS(model, member, column) \
    { column, column, HDB_FT_TIMESTAMP_MS, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_INT64_PK(model, member, column) \
    { column, column, HDB_FT_INT64, (int)offsetof(model, member), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY }

#define HDB_FIELD_INT32_PK(model, member, column) \
    { column, column, HDB_FT_INT32, (int)offsetof(model, member), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY }

#define HDB_FIELD_INT32_EX(model, member, field, column) \
    { field, column, HDB_FT_INT32, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_INT64_EX(model, member, field, column) \
    { field, column, HDB_FT_INT64, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_DOUBLE_EX(model, member, field, column) \
    { field, column, HDB_FT_DOUBLE, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_SMALLINT_EX(model, member, field, column) \
    { field, column, HDB_FT_SMALLINT, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_CHAR_EX(model, member, field, column, bytes) \
    { field, column, HDB_FT_CHAR_ARRAY, (int)offsetof(model, member), bytes, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_TIMESTAMP_MS_EX(model, member, field, column) \
    { field, column, HDB_FT_TIMESTAMP_MS, (int)offsetof(model, member), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }

#define HDB_FIELD_INT64_PK_EX(model, member, field, column) \
    { field, column, HDB_FT_INT64, (int)offsetof(model, member), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY }

#define HDB_FIELD_INT32_PK_EX(model, member, field, column) \
    { field, column, HDB_FT_INT32, (int)offsetof(model, member), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY }

#endif
