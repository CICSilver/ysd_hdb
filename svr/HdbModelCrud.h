#ifndef YSD_HDB_MODEL_CRUD_H
#define YSD_HDB_MODEL_CRUD_H

#include "HdbDbAdapter.h"
#include "HdbModelDef.h"

#include <string>

typedef int (*HdbModelRowCallback)(const void* model, void* userData);

class CHdbModelCrud
{
public:
    explicit CHdbModelCrud(CHdbDbAdapter* adapter);

    int InsertModel(const HdbModelDef& def, const void* model);
    int InsertModelToTable(const HdbModelDef& def, const char* tableName, const void* model);
    int UpdateModel(const HdbModelDef& def, const void* model);
    int DeleteModel(const HdbModelDef& def, const void* model);
    int SelectModelByPk(const HdbModelDef& def, const void* keyModel, void* outModel, int* found);
    int SelectModelList(const HdbModelDef& def, HdbModelRowCallback cb, void* userData);

    const char* GetLastError() const;

private:
    int ValidateModelDef(const HdbModelDef& def);
    int ValidateIdentifier(const char* name);
    int CountFields(const HdbModelDef& def, int requiredFlags, int excludedFlags);
    int BuildFieldValue(const HdbFieldDef& field, const void* model, std::string& value);
    int SetFieldValue(const HdbFieldDef& field, void* model, const char* value);
    int ExecGenerated(const std::string& sql, const std::vector<std::string>& values, int* affectedRows);
    int QueryGenerated(const std::string& sql, const std::vector<std::string>& values, CHdbQueryResult& result);
    void SetLastError(const char* text);

private:
    CHdbDbAdapter* m_adapter;
    std::string m_lastError;
};

#endif
