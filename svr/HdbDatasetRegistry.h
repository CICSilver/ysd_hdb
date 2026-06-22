#ifndef YSD_HDB_DATASET_REGISTRY_H
#define YSD_HDB_DATASET_REGISTRY_H

#include "HdbModelDef.h"

#include <string>

// Association 是可供显式 JOIN 使用的命名关联，不暗中决定 JOIN 类型
struct HdbAssociationDef
{
    const char* ownerDataset;     // 拥有关联的起始数据集
    const char* associationName;  // 查询中显式使用的 Association 名称
    const char* targetDataset;    // 目标数据集
    const char* localFieldName;   // ownerDataset 上参与 ON 的字段
    const char* targetFieldName;  // targetDataset 上参与 ON 的字段
};

class CHdbDatasetRegistry
{
public:
    CHdbDatasetRegistry();

    // 返回静态元数据指针
    const HdbDatasetDef* FindDataset(const char* datasetName) const;
    const HdbFieldDef* FindField(const HdbDatasetDef& dataset, const char* fieldName) const;
    const HdbAssociationDef* FindAssociation(const char* ownerDataset, const char* associationName) const;

    // 进入 SQL builder 的名字都走这里校验
    int ValidateDataset(const HdbDatasetDef& dataset) const;
    int ValidateAssociation(const HdbAssociationDef& association) const;
    int ValidateIdentifier(const char* name) const;
    const char* GetLastError() const;

private:
    void SetLastError(const char* text) const;

private:
    mutable std::string m_lastError; // 最近错误文本
};

#endif
