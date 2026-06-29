// 此文件由 ysd_hdb_meta_codegen 生成，请勿手动修改
#include "HdbGeneratedMeta.h"

#include <stddef.h>

struct HdbGeneratedAlarmModel
{
    HdbInt64 id;
    HdbInt64 point_id;
    int level;
    char message[128];
    HdbInt64 occur_time;
};

struct HdbGeneratedDeviceModel
{
    HdbInt64 id;
    char name[128];
};

struct HdbGeneratedModelCrudTestModel
{
    HdbInt64 id;
    int type;
    char name[128];
    HdbInt64 create_time;
};

struct HdbGeneratedPointModel
{
    HdbInt64 id;
    HdbInt64 device_id;
    char name[128];
};

static HdbFieldDef g_hdbGeneratedAlarmFields[] =
{
    { "id", "id", HDB_FT_INT64, (int)offsetof(HdbGeneratedAlarmModel, id), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY },
    { "point_id", "point_id", HDB_FT_INT64, (int)offsetof(HdbGeneratedAlarmModel, point_id), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE },
    { "level", "level", HDB_FT_INT32, (int)offsetof(HdbGeneratedAlarmModel, level), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE },
    { "message", "message", HDB_FT_CHAR_ARRAY, (int)offsetof(HdbGeneratedAlarmModel, message), 128, HDB_FIELD_INSERT | HDB_FIELD_UPDATE },
    { "occur_time", "occur_time", HDB_FT_TIMESTAMP_MS, (int)offsetof(HdbGeneratedAlarmModel, occur_time), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }
};

static HdbFieldDef g_hdbGeneratedDeviceFields[] =
{
    { "id", "id", HDB_FT_INT64, (int)offsetof(HdbGeneratedDeviceModel, id), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY },
    { "name", "name", HDB_FT_CHAR_ARRAY, (int)offsetof(HdbGeneratedDeviceModel, name), 128, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }
};

static HdbFieldDef g_hdbGeneratedModelCrudTestFields[] =
{
    { "id", "id", HDB_FT_INT64, (int)offsetof(HdbGeneratedModelCrudTestModel, id), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY },
    { "type", "type", HDB_FT_INT32, (int)offsetof(HdbGeneratedModelCrudTestModel, type), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE },
    { "name", "name", HDB_FT_CHAR_ARRAY, (int)offsetof(HdbGeneratedModelCrudTestModel, name), 128, HDB_FIELD_INSERT | HDB_FIELD_UPDATE },
    { "create_time", "create_time", HDB_FT_TIMESTAMP_MS, (int)offsetof(HdbGeneratedModelCrudTestModel, create_time), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }
};

static HdbFieldDef g_hdbGeneratedPointFields[] =
{
    { "id", "id", HDB_FT_INT64, (int)offsetof(HdbGeneratedPointModel, id), 0, HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY },
    { "device_id", "device_id", HDB_FT_INT64, (int)offsetof(HdbGeneratedPointModel, device_id), 0, HDB_FIELD_INSERT | HDB_FIELD_UPDATE },
    { "name", "name", HDB_FT_CHAR_ARRAY, (int)offsetof(HdbGeneratedPointModel, name), 128, HDB_FIELD_INSERT | HDB_FIELD_UPDATE }
};

static HdbDatasetDef g_hdbGeneratedDatasets[] =
{
    {
        "alarm",
        sizeof(HdbGeneratedAlarmModel),
        g_hdbGeneratedAlarmFields,
        (int)HDB_ARRAY_COUNT(g_hdbGeneratedAlarmFields),
        { HDB_SHARD_DAY, "", "hdb_alarm", "occur_time", HDB_MISSING_SHARD_IGNORE }
    },
    {
        "device",
        sizeof(HdbGeneratedDeviceModel),
        g_hdbGeneratedDeviceFields,
        (int)HDB_ARRAY_COUNT(g_hdbGeneratedDeviceFields),
        { HDB_SHARD_NONE, "hdb_device", "", "", HDB_MISSING_SHARD_ERROR }
    },
    {
        "model_crud_test",
        sizeof(HdbGeneratedModelCrudTestModel),
        g_hdbGeneratedModelCrudTestFields,
        (int)HDB_ARRAY_COUNT(g_hdbGeneratedModelCrudTestFields),
        { HDB_SHARD_NONE, "hdb_model_crud_test", "", "", HDB_MISSING_SHARD_ERROR }
    },
    {
        "point",
        sizeof(HdbGeneratedPointModel),
        g_hdbGeneratedPointFields,
        (int)HDB_ARRAY_COUNT(g_hdbGeneratedPointFields),
        { HDB_SHARD_NONE, "hdb_point", "", "", HDB_MISSING_SHARD_ERROR }
    }
};

const HdbDatasetDef* HdbGetGeneratedDatasets(int* outCount)
{
    if (outCount != NULL)
    {
        *outCount = (int)HDB_ARRAY_COUNT(g_hdbGeneratedDatasets);
    }
    return g_hdbGeneratedDatasets;
}
