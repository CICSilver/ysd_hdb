#ifndef YSD_HDB_MODEL_CRUD_H
#define YSD_HDB_MODEL_CRUD_H

#include "HdbDbAdapter.h"
#include "HdbModelDef.h"

#include <string>

// 回调里的 model 指针只在回调期间有效
typedef int (*HdbModelRowCallback)(const void* model, void* userData);

// 单表 CRUD SQL 生成器
class CHdbModelCrud
{
public:
    explicit CHdbModelCrud(CHdbDbAdapter* adapter);

    int InsertModel(const HdbModelDef& def, const void* model);
    // tableName 可由 ShardRouter 决定，用于日分片插入
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
    CHdbDbAdapter* m_adapter; // 数据库适配器
    std::string m_lastError;  // 最近错误文本
};

#endif
