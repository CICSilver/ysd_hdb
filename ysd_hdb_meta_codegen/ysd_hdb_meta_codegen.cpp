#include "../svr/HdbDatasetRegistry.h"
#include "../svr/HdbPgAdapter.h"

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <direct.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define HDB_PATH_SEP '\\'
#else
#define HDB_PATH_SEP '/'
#endif

struct HdbCodegenOptions
{
    std::string connInfo;
    std::string outDir;
    std::string schemaName;
    int showHelp;
};

struct HdbCodegenColumn
{
    std::string columnName;
    std::string dataType;
    std::string udtName;
    int ordinal;
    int charSize;
    int isPk;
    std::string fieldType;
    std::string cppType;
};

struct HdbCodegenDataset
{
    std::string datasetName;
    int shardType;
    std::string tableName;
    std::string tablePrefix;
    std::string routeFieldName;
    int missingPolicy;
    std::vector<std::string> physicalTables;
    std::vector<HdbCodegenColumn> columns;
};

static std::string HdbToLower(const std::string& text)
{
    std::string value;
    size_t i;

    value = text;
    for (i = 0; i < value.size(); ++i)
    {
        value[i] = (char)tolower((unsigned char)value[i]);
    }
    return value;
}

static int HdbStartsWith(const std::string& text, const char* prefix)
{
    size_t len;

    if (prefix == NULL)
    {
        return 0;
    }
    len = strlen(prefix);
    return text.size() >= len && text.compare(0, len, prefix) == 0;
}

static int HdbIsPathSep(char ch)
{
    return ch == '/' || ch == '\\';
}

static int HdbPathIsAbsolute(const std::string& path)
{
    if (path.empty())
    {
        return 0;
    }
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':')
    {
        return 1;
    }
#endif
    return HdbIsPathSep(path[0]);
}

static std::string HdbJoinPath(const std::string& left, const std::string& right)
{
    if (right.empty())
    {
        return left;
    }
    if (HdbPathIsAbsolute(right))
    {
        return right;
    }
    if (left.empty())
    {
        return right;
    }
    if (HdbIsPathSep(left[left.size() - 1]))
    {
        return left + right;
    }
    return left + HDB_PATH_SEP + right;
}

static std::string HdbParentDir(const std::string& path)
{
    std::string value;
    size_t end;
    size_t pos;

    value = path;
    end = value.size();
    while (end > 0 && HdbIsPathSep(value[end - 1]))
    {
        --end;
    }
    if (end == 0)
    {
        return value;
    }
    pos = value.find_last_of("/\\", end - 1);
    if (pos == std::string::npos)
    {
        return "";
    }
#ifdef _WIN32
    if (pos == 2 && value.size() >= 3 && value[1] == ':')
    {
        return value.substr(0, 3);
    }
#endif
    if (pos == 0)
    {
        return value.substr(0, 1);
    }
    return value.substr(0, pos);
}

static int HdbFileExists(const std::string& path)
{
    std::ifstream input;

    input.open(path.c_str(), std::ios::in | std::ios::binary);
    return input.good() ? 1 : 0;
}

static int HdbDirExists(const std::string& path)
{
#ifdef _WIN32
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
    {
        return 0;
    }
    return (info.st_mode & _S_IFDIR) != 0 ? 1 : 0;
#else
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return 0;
    }
    return S_ISDIR(info.st_mode) ? 1 : 0;
#endif
}

static int HdbMakeDir(const std::string& path)
{
    if (HdbDirExists(path))
    {
        return 1;
    }
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0)
    {
        return 1;
    }
#else
    if (mkdir(path.c_str(), 0755) == 0)
    {
        return 1;
    }
#endif
    return errno == EEXIST ? 1 : 0;
}

static std::string HdbGetCurrentDir()
{
    char buffer[2048];

    memset(buffer, 0, sizeof(buffer));
#ifdef _WIN32
    if (_getcwd(buffer, sizeof(buffer) - 1) == NULL)
    {
        return "";
    }
#else
    if (getcwd(buffer, sizeof(buffer) - 1) == NULL)
    {
        return "";
    }
#endif
    return buffer;
}

static int HdbFindProjectRoot(std::string& outRoot)
{
    std::string dir;

    dir = HdbGetCurrentDir();
    while (!dir.empty())
    {
        if (HdbFileExists(HdbJoinPath(dir, "common/HdbCommon.h")) &&
            HdbFileExists(HdbJoinPath(dir, "ysd_hdb_meta_codegen/ysd_hdb_meta_codegen.vcxproj")))
        {
            outRoot = dir;
            return 1;
        }
        if (HdbParentDir(dir) == dir)
        {
            break;
        }
        dir = HdbParentDir(dir);
    }
    return 0;
}

static int HdbIsNameText(const char* text)
{
    int i;

    if (text == NULL || text[0] == '\0')
    {
        return 0;
    }
    if (!(isalpha((unsigned char)text[0]) || text[0] == '_'))
    {
        return 0;
    }
    for (i = 1; text[i] != '\0'; ++i)
    {
        if (!(isalnum((unsigned char)text[i]) || text[i] == '_'))
        {
            return 0;
        }
    }
    return 1;
}

static int HdbIsCppKeyword(const std::string& text)
{
    static const char* keywords[] =
    {
        "asm", "auto", "bool", "break", "case", "catch", "char", "class",
        "const", "const_cast", "continue", "default", "delete", "do",
        "double", "dynamic_cast", "else", "enum", "explicit", "export",
        "extern", "false", "float", "for", "friend", "goto", "if",
        "inline", "int", "long", "mutable", "namespace", "new", "operator",
        "private", "protected", "public", "register", "reinterpret_cast",
        "return", "short", "signed", "sizeof", "static", "static_cast",
        "struct", "switch", "template", "this", "throw", "true", "try",
        "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "wchar_t", "while"
    };
    int i;

    for (i = 0; i < (int)HDB_ARRAY_COUNT(keywords); ++i)
    {
        if (text == keywords[i])
        {
            return 1;
        }
    }
    return 0;
}

static int HdbIsSafeName(const std::string& text)
{
    return HdbIsNameText(text.c_str()) && !HdbIsCppKeyword(text);
}

static std::string HdbToPascalName(const std::string& text)
{
    std::string value;
    int upperNext;
    size_t i;

    value.clear();
    upperNext = 1;
    for (i = 0; i < text.size(); ++i)
    {
        char ch;

        ch = text[i];
        if (ch == '_')
        {
            upperNext = 1;
            continue;
        }
        if (upperNext)
        {
            value += (char)toupper((unsigned char)ch);
            upperNext = 0;
        }
        else
        {
            value += ch;
        }
    }
    if (value.empty())
    {
        value = "Model";
    }
    return value;
}

static std::string HdbIntToString(int value)
{
    std::ostringstream out;

    out << value;
    return out.str();
}

static void HdbAppendTextUtf8(std::ostringstream& out, const char* text)
{
    if (text == NULL)
    {
        return;
    }
#ifdef _WIN32
    {
        int wideLen;
        int utf8Len;
        std::vector<wchar_t> wideText;
        std::vector<char> utf8Text;

        wideLen = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
        if (wideLen <= 0)
        {
            out << text;
            return;
        }
        wideText.resize(wideLen);
        if (MultiByteToWideChar(CP_ACP, 0, text, -1, &wideText[0], wideLen) <= 0)
        {
            out << text;
            return;
        }
        utf8Len = WideCharToMultiByte(CP_UTF8, 0, &wideText[0], -1, NULL, 0, NULL, NULL);
        if (utf8Len <= 0)
        {
            out << text;
            return;
        }
        utf8Text.resize(utf8Len);
        if (WideCharToMultiByte(CP_UTF8, 0, &wideText[0], -1, &utf8Text[0], utf8Len, NULL, NULL) <= 0)
        {
            out << text;
            return;
        }
        out << &utf8Text[0];
    }
#else
    out << text;
#endif
}

static std::string HdbSqlLiteral(const std::string& text)
{
    std::string value;
    size_t i;

    value.clear();
    for (i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\'')
        {
            value += "''";
        }
        else
        {
            value += text[i];
        }
    }
    return value;
}

static int HdbIsDayShardTable(const std::string& tableName, std::string& outPrefix)
{
    size_t i;
    size_t suffixPos;

    if (tableName.size() <= 9)
    {
        return 0;
    }
    suffixPos = tableName.size() - 8;
    if (suffixPos == 0 || tableName[suffixPos - 1] != '_')
    {
        return 0;
    }
    for (i = suffixPos; i < tableName.size(); ++i)
    {
        if (!isdigit((unsigned char)tableName[i]))
        {
            return 0;
        }
    }
    outPrefix = tableName.substr(0, suffixPos - 1);
    return 1;
}

static std::string HdbDatasetNameFromTable(const std::string& tableName)
{
    if (HdbStartsWith(tableName, "hdb_"))
    {
        return tableName.substr(4);
    }
    return tableName;
}

static int HdbMapColumnType(HdbCodegenColumn& column, std::string& error)
{
    std::string dataType;
    std::string udtName;

    dataType = HdbToLower(column.dataType);
    udtName = HdbToLower(column.udtName);
    if (udtName == "int8" || dataType == "bigint")
    {
        column.fieldType = "HDB_FT_INT64";
        column.cppType = "HdbInt64";
        column.charSize = 0;
        return 1;
    }
    if (udtName == "int4" || dataType == "integer")
    {
        column.fieldType = "HDB_FT_INT32";
        column.cppType = "int";
        column.charSize = 0;
        return 1;
    }
    if (udtName == "int2" || dataType == "smallint")
    {
        column.fieldType = "HDB_FT_SMALLINT";
        column.cppType = "short";
        column.charSize = 0;
        return 1;
    }
    if (udtName == "float8" || dataType == "double precision")
    {
        column.fieldType = "HDB_FT_DOUBLE";
        column.cppType = "double";
        column.charSize = 0;
        return 1;
    }
    if (udtName == "timestamp" ||
        udtName == "timestamptz" ||
        dataType == "timestamp without time zone" ||
        dataType == "timestamp with time zone")
    {
        column.fieldType = "HDB_FT_TIMESTAMP_MS";
        column.cppType = "HdbInt64";
        column.charSize = 0;
        return 1;
    }
    if (udtName == "varchar" ||
        udtName == "bpchar" ||
        dataType == "character varying" ||
        dataType == "character")
    {
        if (column.charSize <= 0)
        {
            error = "char column requires character_maximum_length: " + column.columnName;
            return 0;
        }
        column.fieldType = "HDB_FT_CHAR_ARRAY";
        column.cppType = "char";
        return 1;
    }
    error = "unsupported column type: " + column.columnName + " " + column.dataType + " " + column.udtName;
    return 0;
}

static int HdbFindColumn(const HdbCodegenDataset& dataset, const std::string& fieldName)
{
    int i;

    for (i = 0; i < (int)dataset.columns.size(); ++i)
    {
        if (dataset.columns[i].columnName == fieldName)
        {
            return i;
        }
    }
    return -1;
}

static int HdbSameColumnShape(const HdbCodegenColumn& left, const HdbCodegenColumn& right)
{
    return left.columnName == right.columnName &&
        left.fieldType == right.fieldType &&
        left.cppType == right.cppType &&
        left.charSize == right.charSize &&
        left.isPk == right.isPk;
}

static int HdbSameTableShape(const std::vector<HdbCodegenColumn>& left,
    const std::vector<HdbCodegenColumn>& right)
{
    int i;

    if (left.size() != right.size())
    {
        return 0;
    }
    for (i = 0; i < (int)left.size(); ++i)
    {
        if (!HdbSameColumnShape(left[i], right[i]))
        {
            return 0;
        }
    }
    return 1;
}

static int HdbFindDataset(const std::vector<HdbCodegenDataset>& datasets, const std::string& datasetName)
{
    int i;

    for (i = 0; i < (int)datasets.size(); ++i)
    {
        if (datasets[i].datasetName == datasetName)
        {
            return i;
        }
    }
    return -1;
}

static int HdbSelectRouteField(HdbCodegenDataset& dataset, std::string& error)
{
    int i;

    if (dataset.shardType != HDB_SHARD_DAY)
    {
        return 1;
    }
    if (HdbFindColumn(dataset, "occur_time") >= 0 &&
        dataset.columns[HdbFindColumn(dataset, "occur_time")].fieldType == "HDB_FT_TIMESTAMP_MS")
    {
        dataset.routeFieldName = "occur_time";
        return 1;
    }
    for (i = 0; i < (int)dataset.columns.size(); ++i)
    {
        if (dataset.columns[i].fieldType == "HDB_FT_TIMESTAMP_MS")
        {
            dataset.routeFieldName = dataset.columns[i].columnName;
            return 1;
        }
    }
    error = "day shard dataset has no timestamp route field: " + dataset.datasetName;
    return 0;
}

static int HdbReadSchema(CHdbPgAdapter& adapter,
    const std::string& schemaName,
    std::vector<HdbCodegenDataset>& datasets,
    std::string& error)
{
    typedef std::map< std::string, std::vector<HdbCodegenColumn> > HdbTableMap;
    HdbTableMap tableMap;
    HdbTableMap::const_iterator tableIt;
    CHdbQueryResult result;
    std::ostringstream sql;
    int row;
    int ret;

    if (!HdbIsNameText(schemaName.c_str()))
    {
        error = "schema name is invalid";
        return 0;
    }

    sql << "select c.table_name, c.column_name, c.ordinal_position, "
        << "c.data_type, c.udt_name, coalesce(c.character_maximum_length, 0), "
        << "case when kcu.column_name is null then 0 else 1 end "
        << "from information_schema.columns c "
        << "left join information_schema.table_constraints tc "
        << "on tc.table_schema = c.table_schema "
        << "and tc.table_name = c.table_name "
        << "and tc.constraint_type = 'PRIMARY KEY' "
        << "left join information_schema.key_column_usage kcu "
        << "on kcu.constraint_schema = tc.constraint_schema "
        << "and kcu.constraint_name = tc.constraint_name "
        << "and kcu.table_schema = c.table_schema "
        << "and kcu.table_name = c.table_name "
        << "and kcu.column_name = c.column_name "
        << "where c.table_schema = '" << HdbSqlLiteral(schemaName) << "' "
        << "and c.table_name like 'hdb\\_%' escape '\\' "
        << "order by c.table_name, c.ordinal_position";

    ret = adapter.QueryParams(sql.str().c_str(), 0, NULL, result);
    if (ret != HDB_OK)
    {
        error = adapter.GetLastError();
        return 0;
    }
    if (result.RowCount() <= 0)
    {
        error = "no hdb_* tables found";
        return 0;
    }

    for (row = 0; row < result.RowCount(); ++row)
    {
        HdbCodegenColumn column;
        std::string tableName;

        tableName = result.GetValue(row, 0);
        column.columnName = result.GetValue(row, 1);
        column.ordinal = atoi(result.GetValue(row, 2));
        column.dataType = result.GetValue(row, 3);
        column.udtName = result.GetValue(row, 4);
        column.charSize = atoi(result.GetValue(row, 5));
        column.isPk = atoi(result.GetValue(row, 6)) != 0 ? 1 : 0;

        if (!HdbIsSafeName(tableName))
        {
            error = "table name is not a safe C identifier: " + tableName;
            return 0;
        }
        if (!HdbIsSafeName(column.columnName))
        {
            error = "column name is not a safe C identifier: " + column.columnName;
            return 0;
        }
        if (!HdbMapColumnType(column, error))
        {
            return 0;
        }
        tableMap[tableName].push_back(column);
    }

    for (tableIt = tableMap.begin(); tableIt != tableMap.end(); ++tableIt)
    {
        HdbCodegenDataset dataset;
        std::string tableName;
        std::string tablePrefix;
        std::string datasetName;
        int shardType;
        int index;

        tableName = tableIt->first;
        if (HdbIsDayShardTable(tableName, tablePrefix))
        {
            shardType = HDB_SHARD_DAY;
            datasetName = HdbDatasetNameFromTable(tablePrefix);
        }
        else
        {
            shardType = HDB_SHARD_NONE;
            datasetName = HdbDatasetNameFromTable(tableName);
        }
        if (!HdbIsSafeName(datasetName))
        {
            error = "dataset name is not a safe C identifier: " + datasetName;
            return 0;
        }

        index = HdbFindDataset(datasets, datasetName);
        if (index < 0)
        {
            dataset.datasetName = datasetName;
            dataset.shardType = shardType;
            dataset.missingPolicy = shardType == HDB_SHARD_DAY ? HDB_MISSING_SHARD_IGNORE : HDB_MISSING_SHARD_ERROR;
            dataset.columns = tableIt->second;
            if (shardType == HDB_SHARD_DAY)
            {
                dataset.tablePrefix = tablePrefix;
            }
            else
            {
                dataset.tableName = tableName;
            }
            dataset.physicalTables.push_back(tableName);
            if (!HdbSelectRouteField(dataset, error))
            {
                return 0;
            }
            datasets.push_back(dataset);
        }
        else
        {
            if (datasets[index].shardType != shardType)
            {
                error = "same dataset has mixed shard types: " + datasetName;
                return 0;
            }
            if (shardType == HDB_SHARD_DAY && datasets[index].tablePrefix != tablePrefix)
            {
                error = "same dataset has mixed day shard prefixes: " + datasetName;
                return 0;
            }
            if (!HdbSameTableShape(datasets[index].columns, tableIt->second))
            {
                error = "day shard table columns are inconsistent: " + tableName;
                return 0;
            }
            datasets[index].physicalTables.push_back(tableName);
        }
    }
    return 1;
}

static std::string HdbFieldFlagsText(const HdbCodegenColumn& column)
{
    if (column.isPk)
    {
        return "HDB_FIELD_PK | HDB_FIELD_INSERT | HDB_FIELD_READONLY";
    }
    return "HDB_FIELD_INSERT | HDB_FIELD_UPDATE";
}

static std::string HdbModelName(const HdbCodegenDataset& dataset)
{
    return "HdbGenerated" + HdbToPascalName(dataset.datasetName) + "Model";
}

static std::string HdbFieldArrayName(const HdbCodegenDataset& dataset)
{
    return "g_hdbGenerated" + HdbToPascalName(dataset.datasetName) + "Fields";
}

static std::string HdbToUpperIdentifier(const std::string& text)
{
    std::string value;
    size_t i;

    value.clear();
    if (text.empty() || !(isalpha((unsigned char)text[0]) || text[0] == '_'))
    {
        value = "HDB_";
    }
    for (i = 0; i < text.size(); ++i)
    {
        char ch;

        ch = text[i];
        if (isalnum((unsigned char)ch))
        {
            value += (char)toupper((unsigned char)ch);
        }
        else
        {
            value += "_";
        }
    }
    if (value.empty())
    {
        value = "HDB_ITEM";
    }
    return value;
}

static std::string HdbDslTableTypeName(const HdbCodegenDataset& dataset)
{
    return "CHdbGenerated" + HdbToPascalName(dataset.datasetName) + "Table";
}

static std::string HdbDslRowTypeName(const HdbCodegenDataset& dataset)
{
    return HdbToPascalName(dataset.datasetName) + "Row";
}

static std::string HdbDslTableVarName(const HdbCodegenDataset& dataset)
{
    return HdbToUpperIdentifier(dataset.datasetName);
}

static std::string HdbDslFieldMemberName(const HdbCodegenColumn& column)
{
    return HdbToUpperIdentifier(column.columnName);
}

static std::string HdbDslRowFieldType(const HdbCodegenColumn& column)
{
    if (column.fieldType == "HDB_FT_CHAR_ARRAY")
    {
        return "std::string";
    }
    if (column.fieldType == "HDB_FT_SMALLINT")
    {
        return "int";
    }
    return column.cppType;
}

static std::string HdbQuote(const std::string& text)
{
    std::string value;
    size_t i;

    value = "\"";
    for (i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\\' || text[i] == '"')
        {
            value += '\\';
        }
        value += text[i];
    }
    value += "\"";
    return value;
}

static void HdbAppendGeneratedNotice(std::ostringstream& out)
{
    HdbAppendTextUtf8(out, "// 此文件由 ysd_hdb_meta_codegen 生成，请勿手动修改\n");
}

static void HdbAppendGeneratedHeader(std::ostringstream& out)
{
    HdbAppendGeneratedNotice(out);
    out << "#ifndef YSD_HDB_GENERATED_META_H\n";
    out << "#define YSD_HDB_GENERATED_META_H\n\n";
    out << "#include \"../svr/HdbDatasetRegistry.h\"\n\n";
    out << "const HdbDatasetDef* HdbGetGeneratedDatasets(int* outCount);\n";
    out << "\n";
    out << "#endif\n";
}

static void HdbAppendGeneratedSource(const std::vector<HdbCodegenDataset>& datasets,
    std::ostringstream& out)
{
    int i;
    int j;

    HdbAppendGeneratedNotice(out);
    out << "#include \"HdbGeneratedMeta.h\"\n\n";
    out << "#include <stddef.h>\n\n";

    for (i = 0; i < (int)datasets.size(); ++i)
    {
        out << "struct " << HdbModelName(datasets[i]) << "\n";
        out << "{\n";
        for (j = 0; j < (int)datasets[i].columns.size(); ++j)
        {
            const HdbCodegenColumn& column = datasets[i].columns[j];
            if (column.fieldType == "HDB_FT_CHAR_ARRAY")
            {
                out << "    char " << column.columnName << "[" << column.charSize << "];\n";
            }
            else
            {
                out << "    " << column.cppType << " " << column.columnName << ";\n";
            }
        }
        out << "};\n\n";
    }

    for (i = 0; i < (int)datasets.size(); ++i)
    {
        out << "static HdbFieldDef " << HdbFieldArrayName(datasets[i]) << "[] =\n";
        out << "{\n";
        for (j = 0; j < (int)datasets[i].columns.size(); ++j)
        {
            const HdbCodegenColumn& column = datasets[i].columns[j];
            out << "    { "
                << HdbQuote(column.columnName) << ", "
                << HdbQuote(column.columnName) << ", "
                << column.fieldType << ", "
                << "(int)offsetof(" << HdbModelName(datasets[i]) << ", " << column.columnName << "), "
                << column.charSize << ", "
                << HdbFieldFlagsText(column)
                << " }";
            if (j + 1 < (int)datasets[i].columns.size())
            {
                out << ",";
            }
            out << "\n";
        }
        out << "};\n\n";
    }

    out << "static HdbDatasetDef g_hdbGeneratedDatasets[] =\n";
    out << "{\n";
    for (i = 0; i < (int)datasets.size(); ++i)
    {
        const HdbCodegenDataset& dataset = datasets[i];

        out << "    {\n";
        out << "        " << HdbQuote(dataset.datasetName) << ",\n";
        out << "        sizeof(" << HdbModelName(dataset) << "),\n";
        out << "        " << HdbFieldArrayName(dataset) << ",\n";
        out << "        (int)HDB_ARRAY_COUNT(" << HdbFieldArrayName(dataset) << "),\n";
        out << "        { ";
        if (dataset.shardType == HDB_SHARD_DAY)
        {
            out << "HDB_SHARD_DAY, \"\", "
                << HdbQuote(dataset.tablePrefix) << ", "
                << HdbQuote(dataset.routeFieldName) << ", "
                << "HDB_MISSING_SHARD_IGNORE";
        }
        else
        {
            out << "HDB_SHARD_NONE, "
                << HdbQuote(dataset.tableName) << ", \"\", \"\", "
                << "HDB_MISSING_SHARD_ERROR";
        }
        out << " }\n";
        out << "    }";
        if (i + 1 < (int)datasets.size())
        {
            out << ",";
        }
        out << "\n";
    }
    out << "};\n\n";

    out << "const HdbDatasetDef* HdbGetGeneratedDatasets(int* outCount)\n";
    out << "{\n";
    out << "    if (outCount != NULL)\n";
    out << "    {\n";
    out << "        *outCount = (int)HDB_ARRAY_COUNT(g_hdbGeneratedDatasets);\n";
    out << "    }\n";
    out << "    return g_hdbGeneratedDatasets;\n";
    out << "}\n";
}

static void HdbAppendGeneratedDslHeader(const std::vector<HdbCodegenDataset>& datasets,
    std::ostringstream& out)
{
    int i;
    int j;

    HdbAppendGeneratedNotice(out);
    out << "#ifndef YSD_HDB_GENERATED_DSL_H\n";
    out << "#define YSD_HDB_GENERATED_DSL_H\n\n";
    out << "#include \"../dll/HdbQueryBuilder.h\"\n\n";
    out << "namespace HdbDsl\n";
    out << "{\n\n";
    for (i = 0; i < (int)datasets.size(); ++i)
    {
        const HdbCodegenDataset& dataset = datasets[i];

        out << "struct " << HdbDslTableTypeName(dataset) << " : public CHdbDslTable\n";
        out << "{\n";
        for (j = 0; j < (int)dataset.columns.size(); ++j)
        {
            out << "    CHdbDslField " << HdbDslFieldMemberName(dataset.columns[j]) << ";\n";
        }
        out << "\n";
        out << "    " << HdbDslTableTypeName(dataset) << "()\n";
        out << "        : CHdbDslTable(" << HdbQuote(dataset.datasetName) << ")";
        for (j = 0; j < (int)dataset.columns.size(); ++j)
        {
            const HdbCodegenColumn& column = dataset.columns[j];

            out << ",\n";
            out << "          " << HdbDslFieldMemberName(column)
                << "(" << HdbQuote(dataset.datasetName) << ", "
                << HdbQuote(column.columnName) << ", "
                << column.fieldType << ")";
        }
        out << "\n";
        out << "    {\n";
        out << "    }\n";
        out << "};\n\n";
        out << "static const " << HdbDslTableTypeName(dataset) << " "
            << HdbDslTableVarName(dataset) << ";\n\n";
        out << "struct " << HdbDslRowTypeName(dataset) << "\n";
        out << "{\n";
        for (j = 0; j < (int)dataset.columns.size(); ++j)
        {
            const HdbCodegenColumn& column = dataset.columns[j];

            out << "    " << HdbDslRowFieldType(column) << " " << column.columnName << ";\n";
        }
        out << "\n";
        out << "    int FillFrom(CHdbDslResult& result)\n";
        out << "    {\n";
        out << "        int ret;\n\n";
        for (j = 0; j < (int)dataset.columns.size(); ++j)
        {
            const HdbCodegenColumn& column = dataset.columns[j];

            out << "        ret = result.Get(" << HdbDslTableVarName(dataset) << "."
                << HdbDslFieldMemberName(column) << ", " << column.columnName << ");\n";
            out << "        if (ret != HDB_OK)\n";
            out << "        {\n";
            out << "            return ret;\n";
            out << "        }\n";
        }
        out << "        return HDB_OK;\n";
        out << "    }\n";
        out << "};\n\n";
    }
    out << "}\n\n";
    out << "#endif\n";
}

static std::string HdbNormalizeNewlines(const std::string& text)
{
    std::string value;
    size_t i;

    value.clear();
    for (i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\r')
        {
            if (i + 1 < text.size() && text[i + 1] == '\n')
            {
                ++i;
            }
            value += "\r\n";
        }
        else if (text[i] == '\n')
        {
            value += "\r\n";
        }
        else
        {
            value += text[i];
        }
    }
    return value;
}

static int HdbWriteTextFile(const std::string& path, const std::string& text, std::string& error)
{
    std::ofstream output;
    unsigned char bom[3];
    std::string normalized;

    output.open(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output)
    {
        error = "open output file failed: " + path;
        return 0;
    }
    bom[0] = 0xef;
    bom[1] = 0xbb;
    bom[2] = 0xbf;
    normalized = HdbNormalizeNewlines(text);
    output.write((const char*)bom, 3);
    output.write(normalized.c_str(), (std::streamsize)normalized.size());
    if (!output)
    {
        error = "write output file failed: " + path;
        return 0;
    }
    return 1;
}

static int HdbWriteGeneratedFiles(const HdbCodegenOptions& options,
    const std::vector<HdbCodegenDataset>& datasets,
    std::string& error)
{
    std::ostringstream header;
    std::ostringstream source;
    std::ostringstream dslHeader;
    std::string headerPath;
    std::string sourcePath;
    std::string dslHeaderPath;

    if (!HdbMakeDir(options.outDir))
    {
        error = "create output directory failed: " + options.outDir;
        return 0;
    }

    HdbAppendGeneratedHeader(header);
    HdbAppendGeneratedSource(datasets, source);
    HdbAppendGeneratedDslHeader(datasets, dslHeader);

    headerPath = HdbJoinPath(options.outDir, "HdbGeneratedMeta.h");
    sourcePath = HdbJoinPath(options.outDir, "HdbGeneratedMeta.cpp");
    dslHeaderPath = HdbJoinPath(options.outDir, "HdbGeneratedDsl.h");
    if (!HdbWriteTextFile(headerPath, header.str(), error))
    {
        return 0;
    }
    if (!HdbWriteTextFile(sourcePath, source.str(), error))
    {
        return 0;
    }
    if (!HdbWriteTextFile(dslHeaderPath, dslHeader.str(), error))
    {
        return 0;
    }
    return 1;
}

static void HdbPrintUsage()
{
    std::cout
        << "usage: ysd_hdb_meta_codegen [--conn connInfo] [--out dir] [--schema name]\n"
        << "defaults:\n"
        << "  --conn   HDB_PG_CONNINFO or local postgres default\n"
        << "  --out    <project-root>\\GeneratedMetaFiles\n"
        << "  --schema public\n";
}

static int HdbParseArgs(int argc, char** argv, HdbCodegenOptions& options, std::string& error)
{
    std::string root;
    const char* envConn;
    int i;

    if (!HdbFindProjectRoot(root))
    {
        root = HdbGetCurrentDir();
    }
    options.schemaName = "public";
    options.outDir = HdbJoinPath(root, "GeneratedMetaFiles");
    options.showHelp = 0;

    envConn = getenv("HDB_PG_CONNINFO");
    if (envConn != NULL && envConn[0] != '\0')
    {
        options.connInfo = envConn;
    }
    else
    {
        options.connInfo = "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=postgres";
    }

    for (i = 1; i < argc; ++i)
    {
        std::string arg;

        arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            options.showHelp = 1;
            return 1;
        }
        if (arg == "--conn" || arg == "-c")
        {
            if (i + 1 >= argc)
            {
                error = "missing value for --conn";
                return 0;
            }
            options.connInfo = argv[++i];
        }
        else if (arg == "--out")
        {
            if (i + 1 >= argc)
            {
                error = "missing value for --out";
                return 0;
            }
            options.outDir = argv[++i];
        }
        else if (arg == "--schema")
        {
            if (i + 1 >= argc)
            {
                error = "missing value for --schema";
                return 0;
            }
            options.schemaName = argv[++i];
        }
        else
        {
            error = "unknown argument: " + arg;
            return 0;
        }
    }
    return 1;
}

int main(int argc, char** argv)
{
    HdbCodegenOptions options;
    CHdbPgAdapter adapter;
    std::vector<HdbCodegenDataset> datasets;
    std::string error;
    int ret;

    if (!HdbParseArgs(argc, argv, options, error))
    {
        std::cerr << error << std::endl;
        HdbPrintUsage();
        return 1;
    }
    if (options.showHelp)
    {
        HdbPrintUsage();
        return 0;
    }
    ret = adapter.Open(options.connInfo.c_str());
    if (ret != HDB_OK)
    {
        std::cerr << "open database failed: " << adapter.GetLastError() << std::endl;
        return 3;
    }
    if (!HdbReadSchema(adapter, options.schemaName, datasets, error))
    {
        std::cerr << "read schema failed: " << error << std::endl;
        return 4;
    }
    if (!HdbWriteGeneratedFiles(options, datasets, error))
    {
        std::cerr << "write generated files failed: " << error << std::endl;
        return 7;
    }

    std::cout << "generated datasets: " << datasets.size() << std::endl;
    std::cout << "output directory: " << options.outDir << std::endl;
    return 0;
}
