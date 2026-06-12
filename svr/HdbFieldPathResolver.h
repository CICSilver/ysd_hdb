#ifndef YSD_HDB_FIELD_PATH_RESOLVER_H
#define YSD_HDB_FIELD_PATH_RESOLVER_H

#include "HdbDatasetRegistry.h"

#include <string>
#include <vector>

struct HdbResolvedRelationStep
{
    const HdbRelationDef* relation;
    const HdbDatasetDef* fromDataset;
    const HdbDatasetDef* toDataset;
    std::string path;
};

struct HdbResolvedFieldPath
{
    std::vector<HdbResolvedRelationStep> relations;
    const HdbDatasetDef* ownerDataset;
    const HdbFieldDef* field;
};

class CHdbFieldPathResolver
{
public:
    explicit CHdbFieldPathResolver(const CHdbDatasetRegistry* registry);

    int Resolve(const HdbDatasetDef& rootDataset,
        const char* fieldPath,
        HdbResolvedFieldPath& outPath);
    const char* GetLastError() const;

private:
    int SplitPath(const char* fieldPath, std::vector<std::string>& segments);
    int ValidateSegment(const char* segment);
    void SetLastError(const char* text);

private:
    const CHdbDatasetRegistry* m_registry;
    std::string m_lastError;
};

#endif
