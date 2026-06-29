#include "HdbDatasetRegistry.h"
#include "../GeneratedMetaFiles/HdbGeneratedMeta.h"

#include <ctype.h>
#include <string.h>

CHdbDatasetRegistry::CHdbDatasetRegistry()
{
}

const HdbDatasetDef* CHdbDatasetRegistry::FindDataset(const char* datasetName) const
{
    const HdbDatasetDef* datasets;
    int datasetCount;
    int i;
    
    if (datasetName == NULL)
    {
        return NULL;
    }
    datasetCount = 0;
    datasets = HdbGetGeneratedDatasets(&datasetCount);
    if (datasets == NULL || datasetCount <= 0)
    {
        return NULL;
    }
    for (i = 0; i < datasetCount; ++i)
    {
        if (strcmp(datasets[i].datasetName, datasetName) == 0)
        {
            return &datasets[i];
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
