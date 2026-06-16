#include "HdbIpcResultCodec.h"

#include <string.h>

void HdbIpcResultSet::Clear()
{
    columns.clear();
    rows.clear();
}

static int HdbIpcCodecAppendUInt32(std::vector<unsigned char>& data, unsigned int value)
{
    unsigned char buffer[4];

    if (data.size() > HDB_IPC_MAX_BODY_LENGTH - 4)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    buffer[0] = (unsigned char)(value & 0xffu);
    buffer[1] = (unsigned char)((value >> 8) & 0xffu);
    buffer[2] = (unsigned char)((value >> 16) & 0xffu);
    buffer[3] = (unsigned char)((value >> 24) & 0xffu);
    data.insert(data.end(), buffer, buffer + 4);
    return HDB_IPC_OK;
}

static int HdbIpcCodecAppendBytes(std::vector<unsigned char>& data, const void* bytes, unsigned int length)
{
    const unsigned char* begin;

    if (length > 0 && bytes == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (length > HDB_IPC_MAX_BODY_LENGTH || data.size() > HDB_IPC_MAX_BODY_LENGTH - length)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    begin = (const unsigned char*)bytes;
    data.insert(data.end(), begin, begin + length);
    return HDB_IPC_OK;
}

static unsigned int HdbIpcCodecReadUInt32(const unsigned char* data)
{
    return ((unsigned int)data[0]) |
        (((unsigned int)data[1]) << 8) |
        (((unsigned int)data[2]) << 16) |
        (((unsigned int)data[3]) << 24);
}

static int HdbIpcCodecReadUInt32At(const unsigned char* data,
    unsigned int length,
    unsigned int* offset,
    unsigned int* value)
{
    if (data == NULL || offset == NULL || value == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (*offset > length || length - *offset < 4)
    {
        return HDB_IPC_ERR_FIELD;
    }
    *value = HdbIpcCodecReadUInt32(data + *offset);
    *offset += 4;
    return HDB_IPC_OK;
}

static int HdbIpcCodecReadStringAt(const unsigned char* data,
    unsigned int length,
    unsigned int* offset,
    std::string& value)
{
    unsigned int textLength;
    int ret;

    value.clear();
    ret = HdbIpcCodecReadUInt32At(data, length, offset, &textLength);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    if (textLength > HDB_IPC_MAX_RESULT_CELL_BYTES ||
        textLength > length ||
        *offset > length ||
        textLength > length - *offset)
    {
        return HDB_IPC_ERR_FIELD;
    }
    if (textLength > 0)
    {
        value.assign((const char*)(data + *offset), textLength);
    }
    *offset += textLength;
    return HDB_IPC_OK;
}

static int HdbIpcCodecAppendString(std::vector<unsigned char>& data, const std::string& value)
{
    int ret;

    if (value.size() > HDB_IPC_MAX_RESULT_CELL_BYTES || value.size() > HDB_IPC_MAX_BODY_LENGTH)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    ret = HdbIpcCodecAppendUInt32(data, (unsigned int)value.size());
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    return HdbIpcCodecAppendBytes(data, value.empty() ? NULL : value.data(), (unsigned int)value.size());
}

static int HdbIpcCodecCheckFieldType(int fieldType)
{
    if (fieldType == HDB_FT_INT32 ||
        fieldType == HDB_FT_INT64 ||
        fieldType == HDB_FT_DOUBLE ||
        fieldType == HDB_FT_SMALLINT ||
        fieldType == HDB_FT_CHAR_ARRAY ||
        fieldType == HDB_FT_TIMESTAMP_MS)
    {
        return HDB_IPC_OK;
    }
    return HDB_IPC_ERR_FIELD;
}

int HdbIpcEncodeResultSchema(const HdbIpcResultSet& result, std::vector<unsigned char>& outData)
{
    unsigned int i;
    int ret;

    // schema 和 rows 分开编码，DLL 可以先拿到列名和类型再解释行值
    outData.clear();
    if (result.columns.size() > HDB_IPC_MAX_RESULT_COLUMNS)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    ret = HdbIpcCodecAppendUInt32(outData, (unsigned int)result.columns.size());
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    for (i = 0; i < (unsigned int)result.columns.size(); ++i)
    {
        if (HdbIpcCodecCheckFieldType(result.columns[i].fieldType) != HDB_IPC_OK)
        {
            return HDB_IPC_ERR_FIELD;
        }
        ret = HdbIpcCodecAppendString(outData, result.columns[i].name);
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
        ret = HdbIpcCodecAppendUInt32(outData, (unsigned int)result.columns[i].fieldType);
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
    }
    return HDB_IPC_OK;
}

int HdbIpcDecodeResultSchema(const void* data, unsigned int length, HdbIpcResultSet& result)
{
    const unsigned char* bytes;
    unsigned int offset;
    unsigned int count;
    unsigned int i;
    int ret;

    result.columns.clear();
    if (length > 0 && data == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    bytes = (const unsigned char*)data;
    offset = 0;
    ret = HdbIpcCodecReadUInt32At(bytes, length, &offset, &count);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    if (count > HDB_IPC_MAX_RESULT_COLUMNS)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    for (i = 0; i < count; ++i)
    {
        HdbIpcResultColumn column;
        unsigned int fieldType;

        column.fieldType = HDB_FT_CHAR_ARRAY;
        ret = HdbIpcCodecReadStringAt(bytes, length, &offset, column.name);
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
        ret = HdbIpcCodecReadUInt32At(bytes, length, &offset, &fieldType);
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
        column.fieldType = (int)fieldType;
        if (HdbIpcCodecCheckFieldType(column.fieldType) != HDB_IPC_OK)
        {
            return HDB_IPC_ERR_FIELD;
        }
        result.columns.push_back(column);
    }
    // 未全部解码，视为协议字段损坏
    return offset == length ? HDB_IPC_OK : HDB_IPC_ERR_FIELD;
}

int HdbIpcEncodeResultRows(const HdbIpcResultSet& result, std::vector<unsigned char>& outData)
{
    unsigned int row;
    int ret;

    outData.clear();
    // 行数据保留 NULL 标记，空字符串和数据库 NULL 分开处理
    if (result.rows.size() > HDB_IPC_MAX_RESULT_ROWS)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    ret = HdbIpcCodecAppendUInt32(outData, (unsigned int)result.rows.size());
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    for (row = 0; row < (unsigned int)result.rows.size(); ++row)
    {
        unsigned int col;
        if (result.rows[row].size() > HDB_IPC_MAX_RESULT_COLUMNS ||
            (!result.columns.empty() && result.rows[row].size() != result.columns.size()))
        {
            return HDB_IPC_ERR_FIELD;
        }
        ret = HdbIpcCodecAppendUInt32(outData, (unsigned int)result.rows[row].size());
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
        for (col = 0; col < (unsigned int)result.rows[row].size(); ++col)
        {
            ret = HdbIpcCodecAppendUInt32(outData, result.rows[row][col].isNull ? 1u : 0u);
            if (ret != HDB_IPC_OK)
            {
                return ret;
            }
            ret = HdbIpcCodecAppendString(outData, result.rows[row][col].value);
            if (ret != HDB_IPC_OK)
            {
                return ret;
            }
        }
    }
    return HDB_IPC_OK;
}

int HdbIpcDecodeResultRows(const void* data, unsigned int length, HdbIpcResultSet& result)
{
    const unsigned char* bytes;
    unsigned int offset;
    unsigned int rowCount;
    unsigned int row;
    int ret;

    result.rows.clear();
    if (length > 0 && data == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    bytes = (const unsigned char*)data;
    offset = 0;
    ret = HdbIpcCodecReadUInt32At(bytes, length, &offset, &rowCount);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    if (rowCount > HDB_IPC_MAX_RESULT_ROWS)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    for (row = 0; row < rowCount; ++row)
    {
        std::vector<HdbIpcResultCell> cells;
        unsigned int cellCount;
        unsigned int col;

        ret = HdbIpcCodecReadUInt32At(bytes, length, &offset, &cellCount);
        if (ret != HDB_IPC_OK)
        {
            return ret;
        }
        if (cellCount > HDB_IPC_MAX_RESULT_COLUMNS)
        {
            return HDB_IPC_ERR_BODY_SIZE;
        }
        if (!result.columns.empty() && cellCount != result.columns.size())
        {
            return HDB_IPC_ERR_FIELD;
        }
        for (col = 0; col < cellCount; ++col)
        {
            HdbIpcResultCell cell;
            unsigned int isNull;

            ret = HdbIpcCodecReadUInt32At(bytes, length, &offset, &isNull);
            if (ret != HDB_IPC_OK)
            {
                return ret;
            }
            ret = HdbIpcCodecReadStringAt(bytes, length, &offset, cell.value);
            if (ret != HDB_IPC_OK)
            {
                return ret;
            }
            if (isNull != 0 && isNull != 1)
            {
                return HDB_IPC_ERR_FIELD;
            }
            cell.isNull = isNull != 0 ? 1 : 0;
            cells.push_back(cell);
        }
        if (!result.columns.empty() && cells.size() != result.columns.size())
        {
            return HDB_IPC_ERR_FIELD;
        }
        result.rows.push_back(cells);
    }
    // rows 也要求完全消费，避免尾部垃圾被静默忽略
    return offset == length ? HDB_IPC_OK : HDB_IPC_ERR_FIELD;
}
