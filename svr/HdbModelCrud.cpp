#include "HdbModelCrud.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sstream>
#include <vector>

static int HdbIsPkField(const HdbFieldDef& field)
{
    return (field.flags & HDB_FIELD_PK) != 0;
}

static size_t HdbStrnlen(const char* text, size_t maxLen)
{
    size_t i;
    if (text == NULL)
    {
        return 0;
    }
    for (i = 0; i < maxLen; ++i)
    {
        if (text[i] == '\0')
        {
            return i;
        }
    }
    return maxLen;
}

static HdbInt64 HdbStringToInt64(const char* text)
{
    if (text == NULL)
    {
        return 0;
    }
#ifdef _WIN32
    return _strtoi64(text, NULL, 10);
#else
    return strtoll(text, NULL, 10);
#endif
}

static int HdbLocalTime(struct tm* outTm, const time_t* inTime)
{
#ifdef _WIN32
    return localtime_s(outTm, inTime);
#else
    return localtime_r(inTime, outTm) == NULL ? -1 : 0;
#endif
}

static std::string HdbFormatTimestampMs(HdbInt64 ms)
{
    time_t seconds;
    int millis;
    struct tm tmValue;
    char buffer[64];

    seconds = (time_t)(ms / 1000);
    millis = (int)(ms % 1000);
    if (millis < 0)
    {
        millis += 1000;
        --seconds;
    }
    memset(&tmValue, 0, sizeof(tmValue));
    if (HdbLocalTime(&tmValue, &seconds) != 0)
    {
        return "";
    }
    HDB_SNPRINTF(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        tmValue.tm_year + 1900,
        tmValue.tm_mon + 1,
        tmValue.tm_mday,
        tmValue.tm_hour,
        tmValue.tm_min,
        tmValue.tm_sec,
        millis);
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
}

static HdbInt64 HdbParseTimestampMs(const char* text)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millis;
    struct tm tmValue;
    time_t seconds;

    if (text == NULL)
    {
        return 0;
    }
    year = month = day = hour = minute = second = millis = 0;
    if (sscanf(text, "%d-%d-%d %d:%d:%d.%d", &year, &month, &day, &hour, &minute, &second, &millis) < 6)
    {
        return HdbStringToInt64(text);
    }

    memset(&tmValue, 0, sizeof(tmValue));
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = minute;
    tmValue.tm_sec = second;
    tmValue.tm_isdst = -1;
    seconds = mktime(&tmValue);
    if (seconds == (time_t)-1)
    {
        return 0;
    }
    return ((HdbInt64)seconds) * 1000 + millis;
}

static void HdbAppendParamName(std::ostringstream& sql, int index)
{
    sql << "$" << index;
}

CHdbModelCrud::CHdbModelCrud(CHdbDbAdapter* adapter)
    : m_adapter(adapter)
{
}

int CHdbModelCrud::InsertModel(const HdbModelDef& def, const void* model)
{
    return InsertModelToTable(def, def.tableName, model);
}

int CHdbModelCrud::InsertModelToTable(const HdbModelDef& def, const char* tableName, const void* model)
{
    std::ostringstream sql;
    std::vector<std::string> values;
    int i;
    int paramIndex;
    int affectedRows;
    int ret;

    m_lastError.clear();
    if (model == NULL)
    {
        SetLastError("insert model is NULL");
        return HDB_ERR_PARAM;
    }
    ret = ValidateModelDef(def);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (ValidateIdentifier(tableName) != HDB_OK)
    {
        return HDB_ERR_MODEL_DEF;
    }

    sql << "insert into " << tableName << " (";
    paramIndex = 0;
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        if ((field.flags & HDB_FIELD_INSERT) == 0)
        {
            continue;
        }
        if (paramIndex > 0)
        {
            sql << ", ";
        }
        sql << field.columnName;
        ++paramIndex;
    }
    if (paramIndex <= 0)
    {
        SetLastError("model has no insert fields");
        return HDB_ERR_MODEL_DEF;
    }
    sql << ") values (";
    for (i = 1; i <= paramIndex; ++i)
    {
        if (i > 1)
        {
            sql << ", ";
        }
        HdbAppendParamName(sql, i);
    }
    sql << ")";

    values.clear();
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        std::string value;
        if ((field.flags & HDB_FIELD_INSERT) == 0)
        {
            continue;
        }
        ret = BuildFieldValue(field, model, value);
        if (ret != HDB_OK)
        {
            return ret;
        }
        values.push_back(value);
    }

    ret = ExecGenerated(sql.str(), values, &affectedRows);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (affectedRows != 1)
    {
        SetLastError("insert affected row count is not 1");
        return HDB_ERR_DB_EXEC;
    }
    return HDB_OK;
}

int CHdbModelCrud::UpdateModel(const HdbModelDef& def, const void* model)
{
    std::ostringstream sql;
    std::vector<std::string> values;
    int i;
    int paramIndex;
    int updateFieldCount;
    int affectedRows;
    int ret;

    m_lastError.clear();
    if (model == NULL)
    {
        SetLastError("update model is NULL");
        return HDB_ERR_PARAM;
    }
    ret = ValidateModelDef(def);
    if (ret != HDB_OK)
    {
        return ret;
    }
    updateFieldCount = CountFields(def, HDB_FIELD_UPDATE, HDB_FIELD_PK | HDB_FIELD_READONLY);
    if (updateFieldCount <= 0)
    {
        SetLastError("model has no update fields");
        return HDB_ERR_MODEL_DEF;
    }

    sql << "update " << def.tableName << " set ";
    paramIndex = 1;
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        std::string value;
        if ((field.flags & HDB_FIELD_UPDATE) == 0 || (field.flags & (HDB_FIELD_PK | HDB_FIELD_READONLY)) != 0)
        {
            continue;
        }
        if (paramIndex > 1)
        {
            sql << ", ";
        }
        sql << field.columnName << " = ";
        HdbAppendParamName(sql, paramIndex);
        ret = BuildFieldValue(field, model, value);
        if (ret != HDB_OK)
        {
            return ret;
        }
        values.push_back(value);
        ++paramIndex;
    }
    sql << " where ";
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        std::string value;
        if (!HdbIsPkField(field))
        {
            continue;
        }
        if (paramIndex > updateFieldCount + 1)
        {
            sql << " and ";
        }
        sql << field.columnName << " = ";
        HdbAppendParamName(sql, paramIndex);
        ret = BuildFieldValue(field, model, value);
        if (ret != HDB_OK)
        {
            return ret;
        }
        values.push_back(value);
        ++paramIndex;
    }

    ret = ExecGenerated(sql.str(), values, &affectedRows);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (affectedRows != 1)
    {
        SetLastError("update did not match one record");
        return HDB_ERR_NO_RECORD;
    }
    return HDB_OK;
}

int CHdbModelCrud::DeleteModel(const HdbModelDef& def, const void* model)
{
    std::ostringstream sql;
    std::vector<std::string> values;
    int i;
    int paramIndex;
    int affectedRows;
    int ret;

    m_lastError.clear();
    if (model == NULL)
    {
        SetLastError("delete model is NULL");
        return HDB_ERR_PARAM;
    }
    ret = ValidateModelDef(def);
    if (ret != HDB_OK)
    {
        return ret;
    }

    sql << "delete from " << def.tableName << " where ";
    paramIndex = 1;
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        std::string value;
        if (!HdbIsPkField(field))
        {
            continue;
        }
        if (paramIndex > 1)
        {
            sql << " and ";
        }
        sql << field.columnName << " = ";
        HdbAppendParamName(sql, paramIndex);
        ret = BuildFieldValue(field, model, value);
        if (ret != HDB_OK)
        {
            return ret;
        }
        values.push_back(value);
        ++paramIndex;
    }

    ret = ExecGenerated(sql.str(), values, &affectedRows);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (affectedRows != 1)
    {
        SetLastError("delete did not match one record");
        return HDB_ERR_NO_RECORD;
    }
    return HDB_OK;
}

int CHdbModelCrud::SelectModelByPk(const HdbModelDef& def, const void* keyModel, void* outModel, int* found)
{
    std::ostringstream sql;
    std::vector<std::string> values;
    CHdbQueryResult result;
    int i;
    int paramIndex;
    int ret;

    m_lastError.clear();
    if (found != NULL)
    {
        *found = 0;
    }
    if (keyModel == NULL || outModel == NULL || found == NULL)
    {
        SetLastError("select model arguments are invalid");
        return HDB_ERR_PARAM;
    }
    ret = ValidateModelDef(def);
    if (ret != HDB_OK)
    {
        return ret;
    }

    sql << "select ";
    for (i = 0; i < def.fieldCount; ++i)
    {
        if (i > 0)
        {
            sql << ", ";
        }
        sql << def.fields[i].columnName;
    }
    sql << " from " << def.tableName << " where ";

    paramIndex = 1;
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        std::string value;
        if (!HdbIsPkField(field))
        {
            continue;
        }
        if (paramIndex > 1)
        {
            sql << " and ";
        }
        sql << field.columnName << " = ";
        HdbAppendParamName(sql, paramIndex);
        ret = BuildFieldValue(field, keyModel, value);
        if (ret != HDB_OK)
        {
            return ret;
        }
        values.push_back(value);
        ++paramIndex;
    }
    sql << " limit 1";

    ret = QueryGenerated(sql.str(), values, result);
    if (ret != HDB_OK)
    {
        return ret;
    }
    if (result.RowCount() <= 0)
    {
        *found = 0;
        return HDB_OK;
    }

    memset(outModel, 0, def.modelSize);
    for (i = 0; i < def.fieldCount; ++i)
    {
        ret = SetFieldValue(def.fields[i], outModel, result.GetValue(0, i));
        if (ret != HDB_OK)
        {
            return ret;
        }
    }
    *found = 1;
    return HDB_OK;
}

int CHdbModelCrud::SelectModelList(const HdbModelDef& def, HdbModelRowCallback cb, void* userData)
{
    (void)def;
    (void)cb;
    (void)userData;
    SetLastError("SelectModelList is reserved for later query conditions");
    return HDB_ERR_PARAM;
}

const char* CHdbModelCrud::GetLastError() const
{
    if (!m_lastError.empty())
    {
        return m_lastError.c_str();
    }
    if (m_adapter != NULL)
    {
        return m_adapter->GetLastError();
    }
    return "";
}

int CHdbModelCrud::ValidateModelDef(const HdbModelDef& def)
{
    int i;
    int pkCount;

    if (m_adapter == NULL)
    {
        SetLastError("database adapter is NULL");
        return HDB_ERR_PARAM;
    }
    if (def.tableName == NULL || def.fields == NULL || def.fieldCount <= 0 || def.modelSize <= 0)
    {
        SetLastError("model definition is incomplete");
        return HDB_ERR_MODEL_DEF;
    }
    if (ValidateIdentifier(def.tableName) != HDB_OK)
    {
        return HDB_ERR_MODEL_DEF;
    }

    pkCount = 0;
    for (i = 0; i < def.fieldCount; ++i)
    {
        const HdbFieldDef& field = def.fields[i];
        if (ValidateIdentifier(field.fieldName) != HDB_OK)
        {
            return HDB_ERR_MODEL_DEF;
        }
        if (ValidateIdentifier(field.columnName) != HDB_OK)
        {
            return HDB_ERR_MODEL_DEF;
        }
        if (field.offset < 0 || field.offset >= def.modelSize)
        {
            SetLastError("field offset is out of model range");
            return HDB_ERR_MODEL_DEF;
        }
        if (field.type != HDB_FT_CHAR_ARRAY && field.offset + (int)sizeof(HdbInt64) > def.modelSize)
        {
            if (field.type == HDB_FT_INT32 && field.offset + (int)sizeof(int) <= def.modelSize)
            {
            }
            else if (field.type == HDB_FT_DOUBLE && field.offset + (int)sizeof(double) <= def.modelSize)
            {
            }
            else if (field.type == HDB_FT_SMALLINT && field.offset + (int)sizeof(short) <= def.modelSize)
            {
            }
            else
            {
                SetLastError("field storage is out of model range");
                return HDB_ERR_MODEL_DEF;
            }
        }
        if (field.type == HDB_FT_CHAR_ARRAY && field.size <= 0)
        {
            SetLastError("char array field size is invalid");
            return HDB_ERR_MODEL_DEF;
        }
        if (field.type == HDB_FT_CHAR_ARRAY && field.offset + field.size > def.modelSize)
        {
            SetLastError("char array field storage is out of model range");
            return HDB_ERR_MODEL_DEF;
        }
        if (HdbIsPkField(field))
        {
            ++pkCount;
        }
    }
    if (pkCount <= 0)
    {
        SetLastError("model requires at least one primary key field");
        return HDB_ERR_MODEL_DEF;
    }
    return HDB_OK;
}

int CHdbModelCrud::ValidateIdentifier(const char* name)
{
    int i;
    if (name == NULL || name[0] == '\0')
    {
        SetLastError("empty identifier");
        return HDB_ERR_MODEL_DEF;
    }
    if (!(isalpha((unsigned char)name[0]) || name[0] == '_'))
    {
        SetLastError("identifier must start with a letter or underscore");
        return HDB_ERR_MODEL_DEF;
    }
    for (i = 1; name[i] != '\0'; ++i)
    {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_'))
        {
            SetLastError("identifier contains invalid characters");
            return HDB_ERR_MODEL_DEF;
        }
    }
    return HDB_OK;
}

int CHdbModelCrud::CountFields(const HdbModelDef& def, int requiredFlags, int excludedFlags)
{
    int i;
    int count;
    count = 0;
    for (i = 0; i < def.fieldCount; ++i)
    {
        if ((def.fields[i].flags & requiredFlags) == requiredFlags &&
            (def.fields[i].flags & excludedFlags) == 0)
        {
            ++count;
        }
    }
    return count;
}

int CHdbModelCrud::BuildFieldValue(const HdbFieldDef& field, const void* model, std::string& value)
{
    const char* base;
    const char* text;
    std::ostringstream out;
    char buffer[64];

    if (model == NULL)
    {
        SetLastError("model is NULL");
        return HDB_ERR_PARAM;
    }

    base = (const char*)model + field.offset;
    switch (field.type)
    {
    case HDB_FT_INT32:
        out << *((const int*)base);
        value = out.str();
        break;
    case HDB_FT_INT64:
        out << *((const HdbInt64*)base);
        value = out.str();
        break;
    case HDB_FT_DOUBLE:
        HDB_SNPRINTF(buffer, sizeof(buffer), "%.17g", *((const double*)base));
        buffer[sizeof(buffer) - 1] = '\0';
        value = buffer;
        break;
    case HDB_FT_SMALLINT:
        out << (int)(*((const short*)base));
        value = out.str();
        break;
    case HDB_FT_CHAR_ARRAY:
        text = (const char*)base;
        value.assign(text, HdbStrnlen(text, (size_t)field.size));
        break;
    case HDB_FT_TIMESTAMP_MS:
        value = HdbFormatTimestampMs(*((const HdbInt64*)base));
        break;
    default:
        SetLastError("unsupported field type");
        return HDB_ERR_MODEL_DEF;
    }
    return HDB_OK;
}

int CHdbModelCrud::SetFieldValue(const HdbFieldDef& field, void* model, const char* value)
{
    char* base;
    char* text;
    int copyLen;

    if (model == NULL)
    {
        SetLastError("output model is NULL");
        return HDB_ERR_PARAM;
    }
    if (value == NULL)
    {
        value = "";
    }

    base = (char*)model + field.offset;
    switch (field.type)
    {
    case HDB_FT_INT32:
        *((int*)base) = atoi(value);
        break;
    case HDB_FT_INT64:
        *((HdbInt64*)base) = HdbStringToInt64(value);
        break;
    case HDB_FT_DOUBLE:
        *((double*)base) = atof(value);
        break;
    case HDB_FT_SMALLINT:
        *((short*)base) = (short)atoi(value);
        break;
    case HDB_FT_CHAR_ARRAY:
        text = (char*)base;
        memset(text, 0, field.size);
        copyLen = (int)strlen(value);
        if (copyLen >= field.size)
        {
            copyLen = field.size - 1;
        }
        if (copyLen > 0)
        {
            memcpy(text, value, copyLen);
        }
        break;
    case HDB_FT_TIMESTAMP_MS:
        *((HdbInt64*)base) = HdbParseTimestampMs(value);
        break;
    default:
        SetLastError("unsupported field type");
        return HDB_ERR_MODEL_DEF;
    }
    return HDB_OK;
}

int CHdbModelCrud::ExecGenerated(const std::string& sql, const std::vector<std::string>& values, int* affectedRows)
{
    std::vector<const char*> params;
    size_t i;

    if (m_adapter == NULL)
    {
        SetLastError("database adapter is NULL");
        return HDB_ERR_PARAM;
    }
    for (i = 0; i < values.size(); ++i)
    {
        params.push_back(values[i].c_str());
    }
    return m_adapter->ExecParams(sql.c_str(), (int)params.size(), params.empty() ? NULL : &params[0], affectedRows);
}

int CHdbModelCrud::QueryGenerated(const std::string& sql, const std::vector<std::string>& values, CHdbQueryResult& result)
{
    std::vector<const char*> params;
    size_t i;

    if (m_adapter == NULL)
    {
        SetLastError("database adapter is NULL");
        return HDB_ERR_PARAM;
    }
    for (i = 0; i < values.size(); ++i)
    {
        params.push_back(values[i].c_str());
    }
    return m_adapter->QueryParams(sql.c_str(), (int)params.size(), params.empty() ? NULL : &params[0], result);
}

void CHdbModelCrud::SetLastError(const char* text)
{
    if (text == NULL || text[0] == '\0')
    {
        m_lastError = "unknown model crud error";
    }
    else
    {
        m_lastError = text;
    }
}
