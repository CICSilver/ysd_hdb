#ifndef YSD_HDB_DATASET_REGISTRY_H
#define YSD_HDB_DATASET_REGISTRY_H

#include "HdbModelDef.h"

#include <string>

enum HdbJoinType
{
    HDB_JOIN_INNER = 1, // 内连接，右侧无匹配时整行不返回
    HDB_JOIN_LEFT = 2 // 左连接，右侧无匹配时右侧字段返回 NULL
};

struct HdbRelationDef
{
    const char* fromDataset;
    const char* relationName;
    const char* toDataset;
    const char* fromFieldName;
    const char* toFieldName;
    int joinType;
};

class CHdbDatasetRegistry
{
public:
    CHdbDatasetRegistry();

    const HdbDatasetDef* FindDataset(const char* datasetName) const;
    const HdbFieldDef* FindField(const HdbDatasetDef& dataset, const char* fieldName) const;
    const HdbRelationDef* FindRelation(const char* fromDataset, const char* relationName) const;

    int ValidateDataset(const HdbDatasetDef& dataset) const;
    int ValidateRelation(const HdbRelationDef& relation) const;
    int ValidateIdentifier(const char* name) const;
    const char* GetLastError() const;

private:
    void SetLastError(const char* text) const;

private:
    mutable std::string m_lastError;
};

#endif
