#ifndef YSD_HDB_DATASET_REGISTRY_H
#define YSD_HDB_DATASET_REGISTRY_H

#include "HdbModelDef.h"

#include <string>

enum HdbJoinType
{
    HDB_JOIN_INNER = 1, // 内连接，右侧无匹配时整行不返回
    HDB_JOIN_LEFT = 2 // 左连接，右侧无匹配时右侧字段返回 NULL
};

// relation 描述数据集之间的跳转
struct HdbRelationDef
{
    const char* fromDataset;   // 起始数据集
    const char* relationName;  // fieldPath 中的一段
    const char* toDataset;     // 目标数据集
    const char* fromFieldName; // 起始字段
    const char* toFieldName;   // 目标字段
    int joinType;              // HdbJoinType
};

class CHdbDatasetRegistry
{
public:
    CHdbDatasetRegistry();

    // 返回静态元数据指针
    const HdbDatasetDef* FindDataset(const char* datasetName) const;
    const HdbFieldDef* FindField(const HdbDatasetDef& dataset, const char* fieldName) const;
    const HdbRelationDef* FindRelation(const char* fromDataset, const char* relationName) const;

    // 进入 SQL builder 的名字都走这里校验
    int ValidateDataset(const HdbDatasetDef& dataset) const;
    int ValidateRelation(const HdbRelationDef& relation) const;
    int ValidateIdentifier(const char* name) const;
    const char* GetLastError() const;

private:
    void SetLastError(const char* text) const;

private:
    mutable std::string m_lastError; // 最近错误文本
};

#endif
