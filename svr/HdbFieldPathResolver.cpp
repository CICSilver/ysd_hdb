#include "HdbFieldPathResolver.h"

#include <ctype.h>
#include <string.h>

CHdbFieldPathResolver::CHdbFieldPathResolver(const CHdbDatasetRegistry* registry)
    : m_registry(registry)
{
}

int CHdbFieldPathResolver::Resolve(const HdbDatasetDef& rootDataset,
    const char* fieldPath,
    HdbResolvedFieldPath& outPath)
{
    std::vector<std::string> segments;
    const HdbDatasetDef* currentDataset;
    std::string relationPath;
    size_t i;
    int ret;

    outPath.relations.clear();
    outPath.ownerDataset = NULL;
    outPath.field = NULL;

    if (m_registry == NULL)
    {
        SetLastError("dataset registry is NULL");
        return HDB_ERR_PARAM;
    }
    ret = SplitPath(fieldPath, segments);
    if (ret != HDB_OK)
    {
        return ret;
    }
    currentDataset = &rootDataset;
    if (segments.size() == 1)
    {
        outPath.field = m_registry->FindField(*currentDataset, segments[0].c_str());
        if (outPath.field == NULL)
        {
            SetLastError("root field is not found");
            return HDB_ERR_FIELD_NOT_FOUND;
        }
        outPath.ownerDataset = currentDataset;
        return HDB_OK;
    }

    for (i = 0; i + 1 < segments.size(); ++i)
    {
        const HdbRelationDef* relation;
        const HdbDatasetDef* nextDataset;
        HdbResolvedRelationStep step;

        relation = m_registry->FindRelation(currentDataset->datasetName, segments[i].c_str());
        if (relation == NULL)
        {
            SetLastError("relation is not found");
            return HDB_ERR_RELATION_NOT_FOUND;
        }
        nextDataset = m_registry->FindDataset(relation->toDataset);
        if (nextDataset == NULL)
        {
            SetLastError("relation target dataset is not found");
            return HDB_ERR_DATASET_NOT_FOUND;
        }
        if (!relationPath.empty())
        {
            relationPath += ".";
        }
        relationPath += segments[i];

        step.relation = relation;
        step.fromDataset = currentDataset;
        step.toDataset = nextDataset;
        step.path = relationPath;
        outPath.relations.push_back(step);
        currentDataset = nextDataset;
    }

    outPath.field = m_registry->FindField(*currentDataset, segments[segments.size() - 1].c_str());
    if (outPath.field == NULL)
    {
        SetLastError("field is not found");
        return HDB_ERR_FIELD_NOT_FOUND;
    }
    outPath.ownerDataset = currentDataset;
    return HDB_OK;
}

const char* CHdbFieldPathResolver::GetLastError() const
{
    return m_lastError.c_str();
}

int CHdbFieldPathResolver::SplitPath(const char* fieldPath, std::vector<std::string>& segments)
{
    std::string text;
    size_t pos;
    size_t next;

    segments.clear();
    if (fieldPath == NULL || fieldPath[0] == '\0')
    {
        SetLastError("empty field path");
        return HDB_ERR_FIELD_PATH;
    }
    text = fieldPath;
    pos = 0;
    while (pos <= text.size())
    {
        std::string segment;
        next = text.find('.', pos);
        if (next == std::string::npos)
        {
            segment = text.substr(pos);
            pos = text.size() + 1;
        }
        else
        {
            segment = text.substr(pos, next - pos);
            pos = next + 1;
        }
        if (ValidateSegment(segment.c_str()) != HDB_OK)
        {
            return HDB_ERR_FIELD_PATH;
        }
        segments.push_back(segment);
    }
    if (segments.empty())
    {
        SetLastError("empty field path");
        return HDB_ERR_FIELD_PATH;
    }
    return HDB_OK;
}

int CHdbFieldPathResolver::ValidateSegment(const char* segment)
{
    int i;

    if (segment == NULL || segment[0] == '\0')
    {
        SetLastError("empty field path segment");
        return HDB_ERR_FIELD_PATH;
    }
    if (!(isalpha((unsigned char)segment[0]) || segment[0] == '_'))
    {
        SetLastError("field path segment must start with a letter or underscore");
        return HDB_ERR_FIELD_PATH;
    }
    for (i = 1; segment[i] != '\0'; ++i)
    {
        if (!(isalnum((unsigned char)segment[i]) || segment[i] == '_'))
        {
            SetLastError("field path segment contains invalid characters");
            return HDB_ERR_FIELD_PATH;
        }
    }
    return HDB_OK;
}

void CHdbFieldPathResolver::SetLastError(const char* text)
{
    if (text == NULL || text[0] == '\0')
    {
        m_lastError = "unknown field path resolver error";
    }
    else
    {
        m_lastError = text;
    }
}
