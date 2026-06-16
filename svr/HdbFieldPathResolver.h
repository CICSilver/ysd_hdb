#ifndef YSD_HDB_FIELD_PATH_RESOLVER_H
#define YSD_HDB_FIELD_PATH_RESOLVER_H

#include "HdbDatasetRegistry.h"

#include <string>
#include <vector>

struct HdbResolvedRelationStep
{
    const HdbRelationDef* relation;    // 当前 relation 定义
    const HdbDatasetDef* fromDataset;  // 起始数据集
    const HdbDatasetDef* toDataset;    // 目标数据集
    std::string path;                  // 从 root 走到当前 relation 的路径
};

struct HdbResolvedFieldPath
{
    std::vector<HdbResolvedRelationStep> relations; // relation 链，空表示 root 字段
    const HdbDatasetDef* ownerDataset;              // 字段所属数据集
    const HdbFieldDef* field;                       // 目标字段
};

class CHdbFieldPathResolver
{
public:
    explicit CHdbFieldPathResolver(const CHdbDatasetRegistry* registry);

    // fieldPath 最后一段按字段名解析，前面的段按 relation 解析
    int Resolve(const HdbDatasetDef& rootDataset,
        const char* fieldPath,
        HdbResolvedFieldPath& outPath);
    const char* GetLastError() const;

private:
    int SplitPath(const char* fieldPath, std::vector<std::string>& segments);
    int ValidateSegment(const char* segment);
    void SetLastError(const char* text);

private:
    const CHdbDatasetRegistry* m_registry; // 元数据注册表
    std::string m_lastError;               // 最近错误文本
};

#endif
