#ifndef YSD_HDB_DATASET_REGISTRY_H
#define YSD_HDB_DATASET_REGISTRY_H

#include "HdbModelDef.h"

#include <string>

class CHdbDatasetRegistry
{
public:
    CHdbDatasetRegistry();

    // 返回静态元数据指针
    const HdbDatasetDef* FindDataset(const char* datasetName) const;
    const HdbFieldDef* FindField(const HdbDatasetDef& dataset, const char* fieldName) const;

    // 进入 SQL builder 的名字都走这里校验
    int ValidateDataset(const HdbDatasetDef& dataset) const;
    int ValidateIdentifier(const char* name) const;
    const char* GetLastError() const;

private:
    void SetLastError(const char* text) const;

private:
    mutable std::string m_lastError; // 最近错误文本
};

#endif
