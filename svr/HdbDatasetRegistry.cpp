#include "HdbDatasetRegistry.h"

#include <ctype.h>
#include <string.h>

// 当前注册表里的模型只用于字段 offset 和测试数据集映射
struct HdbRegistryAlarmModel
{
    HdbInt64 id;
    HdbInt64 point_id;
    int level;
    char message[128];
    HdbInt64 occur_time;
};

struct HdbRegistryPointModel
{
    HdbInt64 id;
    HdbInt64 device_id;
    char name[128];
};

struct HdbRegistryDeviceModel
{
    HdbInt64 id;
    char name[128];
};

static HdbFieldDef g_hdbRegistryAlarmFields[] =
{
    HDB_FIELD_INT64_PK(HdbRegistryAlarmModel, id, "id"),
    HDB_FIELD_INT64(HdbRegistryAlarmModel, point_id, "point_id"),
    HDB_FIELD_INT32(HdbRegistryAlarmModel, level, "level"),
    HDB_FIELD_CHAR(HdbRegistryAlarmModel, message, "message", 128),
    HDB_FIELD_TIMESTAMP_MS(HdbRegistryAlarmModel, occur_time, "occur_time")
};

static HdbFieldDef g_hdbRegistryPointFields[] =
{
    HDB_FIELD_INT64_PK(HdbRegistryPointModel, id, "id"),
    HDB_FIELD_INT64(HdbRegistryPointModel, device_id, "device_id"),
    HDB_FIELD_CHAR(HdbRegistryPointModel, name, "name", 128)
};

static HdbFieldDef g_hdbRegistryDeviceFields[] =
{
    HDB_FIELD_INT64_PK(HdbRegistryDeviceModel, id, "id"),
    HDB_FIELD_CHAR(HdbRegistryDeviceModel, name, "name", 128)
};

static HdbDatasetDef g_hdbDatasets[] =
{
    // 逻辑数据集到物理表和分片策略的静态注册表
    {
        "alarm",
        sizeof(HdbRegistryAlarmModel),
        g_hdbRegistryAlarmFields,
        (int)HDB_ARRAY_COUNT(g_hdbRegistryAlarmFields),
        { HDB_SHARD_DAY, "", "hdb_alarm", "occur_time", HDB_MISSING_SHARD_IGNORE }
    },
    {
        "point",
        sizeof(HdbRegistryPointModel),
        g_hdbRegistryPointFields,
        (int)HDB_ARRAY_COUNT(g_hdbRegistryPointFields),
        { HDB_SHARD_NONE, "hdb_point", "", "", HDB_MISSING_SHARD_ERROR }
    },
    {
        "device",
        sizeof(HdbRegistryDeviceModel),
        g_hdbRegistryDeviceFields,
        (int)HDB_ARRAY_COUNT(g_hdbRegistryDeviceFields),
        { HDB_SHARD_NONE, "hdb_device", "", "", HDB_MISSING_SHARD_ERROR }+
    }
};

// 手写schema
static HdbAssociationDef g_hdbAssociations[] =
{
    // Association 是显式 JOIN 的命名关联，不触发隐式导航
    { "alarm", "point", "point", "point_id", "id" },
    { "point", "device", "device", "device_id", "id" }
};

CHdbDatasetRegistry::CHdbDatasetRegistry()
{
}

const HdbDatasetDef* CHdbDatasetRegistry::FindDataset(const char* datasetName) const
{
    int i;
    
    if (datasetName == NULL)
    {
        return NULL;
    }
    for (i = 0; i < (int)HDB_ARRAY_COUNT(g_hdbDatasets); ++i)
    {
        if (strcmp(g_hdbDatasets[i].datasetName, datasetName) == 0)
        {
            return &g_hdbDatasets[i];
        }
    }
    return NULL;
}

const HdbFieldDef* CHdbDatasetRegistry::FindField(const HdbDatasetDef& dataset, const char* fieldName) const
{
    int i;

    if (fieldName == NULL)
    {
        return NULL;
    }
    for (i = 0; i < dataset.fieldCount; ++i)
    {
        if (strcmp(dataset.fields[i].fieldName, fieldName) == 0)
        {
            return &dataset.fields[i];
        }
    }
    return NULL;
}

const HdbAssociationDef* CHdbDatasetRegistry::FindAssociation(const char* ownerDataset, const char* associationName) const
{
    int i;

    if (ownerDataset == NULL || associationName == NULL)
    {
        return NULL;
    }
    for (i = 0; i < (int)HDB_ARRAY_COUNT(g_hdbAssociations); ++i)
    {
        if (strcmp(g_hdbAssociations[i].ownerDataset, ownerDataset) == 0 &&
            strcmp(g_hdbAssociations[i].associationName, associationName) == 0)
        {
            return &g_hdbAssociations[i];
        }
    }
    return NULL;
}

int CHdbDatasetRegistry::ValidateDataset(const HdbDatasetDef& dataset) const
{
    int i;

    if (dataset.datasetName == NULL || dataset.fields == NULL || dataset.fieldCount <= 0 || dataset.modelSize <= 0)
    {
        SetLastError("dataset definition is incomplete");
        return HDB_ERR_DATASET_DEF;
    }
    // 注册表也要走标识符校验，后续 SQL builder 才能直接使用元数据名
    if (ValidateIdentifier(dataset.datasetName) != HDB_OK)
    {
        return HDB_ERR_DATASET_DEF;
    }
    for (i = 0; i < dataset.fieldCount; ++i)
    {
        if (ValidateIdentifier(dataset.fields[i].fieldName) != HDB_OK ||
            ValidateIdentifier(dataset.fields[i].columnName) != HDB_OK)
        {
            return HDB_ERR_DATASET_DEF;
        }
    }
    if (dataset.shard.shardType == HDB_SHARD_NONE || dataset.shard.shardType == HDB_SHARD_DB_PARTITION)
    {
        if (ValidateIdentifier(dataset.shard.tableName) != HDB_OK)
        {
            return HDB_ERR_SHARD_DEF;
        }
    }
    else if (dataset.shard.shardType == HDB_SHARD_DAY)
    {
        // 日分片需要 route 字段来从时间范围推导物理表
        if (ValidateIdentifier(dataset.shard.tablePrefix) != HDB_OK ||
            FindField(dataset, dataset.shard.routeFieldName) == NULL)
        {
            SetLastError("day shard definition is invalid");
            return HDB_ERR_SHARD_DEF;
        }
    }
    else
    {
        SetLastError("unsupported shard type");
        return HDB_ERR_SHARD_DEF;
    }
    return HDB_OK;
}

int CHdbDatasetRegistry::ValidateAssociation(const HdbAssociationDef& association) const
{
    const HdbDatasetDef* ownerDataset;
    const HdbDatasetDef* targetDataset;

    if (ValidateIdentifier(association.ownerDataset) != HDB_OK ||
        ValidateIdentifier(association.associationName) != HDB_OK ||
        ValidateIdentifier(association.targetDataset) != HDB_OK ||
        ValidateIdentifier(association.localFieldName) != HDB_OK ||
        ValidateIdentifier(association.targetFieldName) != HDB_OK)
    {
        return HDB_ERR_ASSOCIATION_NOT_FOUND;
    }

    ownerDataset = FindDataset(association.ownerDataset);
    targetDataset = FindDataset(association.targetDataset);
    if (ownerDataset == NULL || targetDataset == NULL)
    {
        SetLastError("association dataset is missing");
        return HDB_ERR_ASSOCIATION_NOT_FOUND;
    }
    if (FindField(*ownerDataset, association.localFieldName) == NULL ||
        FindField(*targetDataset, association.targetFieldName) == NULL)
    {
        SetLastError("association field is missing");
        return HDB_ERR_ASSOCIATION_NOT_FOUND;
    }
    return HDB_OK;
}

int CHdbDatasetRegistry::ValidateIdentifier(const char* name) const
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

const char* CHdbDatasetRegistry::GetLastError() const
{
    return m_lastError.c_str();
}

void CHdbDatasetRegistry::SetLastError(const char* text) const
{
    if (text == NULL || text[0] == '\0')
    {
        m_lastError = "unknown dataset registry error";
    }
    else
    {
        m_lastError = text;
    }
}
