#include "HdbIpcProtocol.h"

#include <string.h>

// TLV 整数 value 按低字节在前写入，不直接拷贝整数对象
static void HdbIpcWriteUInt32(unsigned char* buffer, unsigned int value)
{
    buffer[0] = (unsigned char)(value & 0xffu);
    buffer[1] = (unsigned char)((value >> 8) & 0xffu);
    buffer[2] = (unsigned char)((value >> 16) & 0xffu);
    buffer[3] = (unsigned char)((value >> 24) & 0xffu);
}

static unsigned int HdbIpcReadUInt32(const unsigned char* buffer)
{
    return ((unsigned int)buffer[0]) |
        (((unsigned int)buffer[1]) << 8) |
        (((unsigned int)buffer[2]) << 16) |
        (((unsigned int)buffer[3]) << 24);
}

static void HdbIpcWriteUInt64(unsigned char* buffer, unsigned long long value)
{
    int i;
    for (i = 0; i < 8; ++i)
    {
        buffer[i] = (unsigned char)((value >> (i * 8)) & 0xffu);
    }
}

static unsigned long long HdbIpcReadUInt64(const unsigned char* buffer)
{
    unsigned long long value;
    int i;

    value = 0;
    for (i = 0; i < 8; ++i)
    {
        value |= ((unsigned long long)buffer[i]) << (i * 8);
    }
    return value;
}

static int HdbIpcCheckBodyPointer(const void* body, unsigned int bodyLength)
{
    // 空 body 可以是 NULL，非空 body 传有效地址
    if (bodyLength > HDB_IPC_MAX_BODY_LENGTH)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    if (bodyLength > 0 && body == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    return HDB_IPC_OK;
}

CHdbIpcFieldReader::CHdbIpcFieldReader()
{
    m_body = NULL;
    m_bodyLength = 0;
    m_offset = 0;
}

int CHdbIpcFieldReader::Reset(const unsigned char* body, unsigned int bodyLength)
{
    if (bodyLength > 0 && body == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }

    m_body = body;
    m_bodyLength = bodyLength;
    m_offset = 0;
    return HDB_IPC_OK;
}

int CHdbIpcFieldReader::Next(HdbIpcField& field, int* hasField)
{
    HdbIpcFieldHeader header;
    unsigned int nextOffset;

    if (hasField == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }

    *hasField = 0;
    memset(&field, 0, sizeof(field));

    if (m_offset == m_bodyLength)
    {
        return HDB_IPC_OK;
    }
    if (m_offset > m_bodyLength ||
        m_bodyLength - m_offset < (unsigned int)sizeof(HdbIpcFieldHeader))
    {
        return HDB_IPC_ERR_FIELD;
    }

    memcpy(&header, m_body + m_offset, sizeof(header));
    nextOffset = m_offset + (unsigned int)sizeof(HdbIpcFieldHeader);
    if (header.length > m_bodyLength - nextOffset)
    {
        return HDB_IPC_ERR_FIELD;
    }

    field.type = header.type;
    field.flags = header.flags;
    field.data = m_body + nextOffset;
    field.length = header.length;
    m_offset = nextOffset + header.length;
    *hasField = 1;
    return HDB_IPC_OK;
}

unsigned int HdbIpcCalcChecksum(const void* data, unsigned int length)
{
    const unsigned char* bytes;
    unsigned int hash;
    unsigned int i;

    bytes = (const unsigned char*)data;
    // FNV-1a 实现小、结果稳定，用于发现传输过程中的帧体损坏
    hash = 2166136261u;
    for (i = 0; i < length; ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

int HdbIpcBuildFrame(unsigned int command,
    unsigned int flags,
    unsigned int sequence,
    int status,
    const void* body,
    unsigned int bodyLength,
    std::vector<unsigned char>& outFrame)
{
    HdbIpcFrameHeader header;
    unsigned int frameSize;
    int ret;

    ret = HdbIpcCheckBodyPointer(body, bodyLength);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }

    if (command == HDB_IPC_CMD_NONE)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (bodyLength > 0xffffffffu - (unsigned int)sizeof(HdbIpcFrameHeader))
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }

    memset(&header, 0, sizeof(header));
    // 帧头保持固定长度，命令相关数据全部放入 body
    header.magic = HDB_IPC_MAGIC;
    header.version = HDB_IPC_VERSION;
    header.headerSize = (unsigned short)sizeof(HdbIpcFrameHeader);
    header.command = command;
    header.flags = flags;
    header.sequence = sequence;
    header.status = status;
    header.bodyLength = bodyLength;
    header.bodyChecksum = HdbIpcCalcChecksum(body, bodyLength);

    frameSize = (unsigned int)sizeof(HdbIpcFrameHeader) + bodyLength;
    outFrame.resize(frameSize);
    memcpy(&outFrame[0], &header, sizeof(header));
    if (bodyLength > 0)
    {
        memcpy(&outFrame[sizeof(HdbIpcFrameHeader)], body, bodyLength);
    }
    return HDB_IPC_OK;
}

int HdbIpcBuildRequest(unsigned int command,
    unsigned int sequence,
    const void* body,
    unsigned int bodyLength,
    std::vector<unsigned char>& outFrame)
{
    return HdbIpcBuildFrame(command,
        HDB_IPC_FLAG_REQUEST,
        sequence,
        HDB_IPC_OK,
        body,
        bodyLength,
        outFrame);
}

int HdbIpcBuildResponse(unsigned int command,
    unsigned int sequence,
    int status,
    const void* body,
    unsigned int bodyLength,
    std::vector<unsigned char>& outFrame)
{
    unsigned int flags;

    flags = HDB_IPC_FLAG_RESPONSE;
    if (status != HDB_IPC_OK)
    {
        flags |= HDB_IPC_FLAG_ERROR;
    }

    return HdbIpcBuildFrame(command,
        flags,
        sequence,
        status,
        body,
        bodyLength,
        outFrame);
}

int HdbIpcGetFrameSize(const unsigned char* data,
    unsigned int dataLength,
    unsigned int* frameSize)
{
    HdbIpcFrameHeader header;

    if (frameSize == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    *frameSize = 0;

    if (data == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (dataLength < (unsigned int)sizeof(HdbIpcFrameHeader))
    {
        return HDB_IPC_ERR_INCOMPLETE;
    }

    memcpy(&header, data, sizeof(header));
    // 先校验帧头，再信任对端传入的 bodyLength
    if (header.magic != HDB_IPC_MAGIC)
    {
        return HDB_IPC_ERR_MAGIC;
    }
    if (header.version != HDB_IPC_VERSION)
    {
        return HDB_IPC_ERR_VERSION;
    }
    if (header.headerSize != (unsigned short)sizeof(HdbIpcFrameHeader))
    {
        return HDB_IPC_ERR_HEADER;
    }
    if (header.bodyLength > HDB_IPC_MAX_BODY_LENGTH)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }
    if (header.bodyLength > 0xffffffffu - (unsigned int)sizeof(HdbIpcFrameHeader))
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }

    *frameSize = (unsigned int)sizeof(HdbIpcFrameHeader) + header.bodyLength;
    if (dataLength < *frameSize)
    {
        return HDB_IPC_ERR_INCOMPLETE;
    }

    return HDB_IPC_OK;
}

int HdbIpcParseFrame(const unsigned char* data,
    unsigned int dataLength,
    HdbIpcFrame& frame)
{
    unsigned int frameSize;
    unsigned int checksum;
    int ret;

    memset(&frame, 0, sizeof(frame));

    ret = HdbIpcGetFrameSize(data, dataLength, &frameSize);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }

    memcpy(&frame.header, data, sizeof(HdbIpcFrameHeader));
    frame.body = data + sizeof(HdbIpcFrameHeader);
    frame.bodyLength = frame.header.bodyLength;

    checksum = HdbIpcCalcChecksum(frame.body, frame.bodyLength);
    if (checksum != frame.header.bodyChecksum)
    {
        return HDB_IPC_ERR_CHECKSUM;
    }

    return HDB_IPC_OK;
}

int HdbIpcAppendField(std::vector<unsigned char>& body,
    unsigned short type,
    const void* data,
    unsigned int length)
{
    HdbIpcFieldHeader header;
    unsigned int offset;
    unsigned int addLength;

    if (type == HDB_IPC_FIELD_NONE)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (length > 0 && data == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (length > HDB_IPC_MAX_BODY_LENGTH)
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }

    addLength = (unsigned int)sizeof(HdbIpcFieldHeader) + length;
    if (body.size() > HDB_IPC_MAX_BODY_LENGTH ||
        addLength > HDB_IPC_MAX_BODY_LENGTH - (unsigned int)body.size())
    {
        return HDB_IPC_ERR_BODY_SIZE;
    }

    memset(&header, 0, sizeof(header));
    header.type = type;
    header.flags = 0;
    header.length = length;

    offset = (unsigned int)body.size();
    body.resize(offset + addLength);
    memcpy(&body[offset], &header, sizeof(header));
    if (length > 0)
    {
        memcpy(&body[offset + sizeof(HdbIpcFieldHeader)], data, length);
    }
    return HDB_IPC_OK;
}

int HdbIpcAppendInt32(std::vector<unsigned char>& body,
    unsigned short type,
    int value)
{
    unsigned char buffer[4];

    HdbIpcWriteUInt32(buffer, (unsigned int)value);
    return HdbIpcAppendField(body, type, buffer, (unsigned int)sizeof(buffer));
}

int HdbIpcAppendInt64(std::vector<unsigned char>& body,
    unsigned short type,
    HdbIpcInt64 value)
{
    unsigned char buffer[8];

    HdbIpcWriteUInt64(buffer, (unsigned long long)value);
    return HdbIpcAppendField(body, type, buffer, (unsigned int)sizeof(buffer));
}

int HdbIpcAppendString(std::vector<unsigned char>& body,
    unsigned short type,
    const char* value)
{
    unsigned int length;

    if (value == NULL)
    {
        length = 0;
    }
    else
    {
        length = (unsigned int)strlen(value);
    }

    return HdbIpcAppendField(body, type, value, length);
}

int HdbIpcReadInt32(const HdbIpcField& field, int* value)
{
    if (value == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (field.data == NULL || field.length != 4)
    {
        return HDB_IPC_ERR_FIELD;
    }

    *value = (int)HdbIpcReadUInt32(field.data);
    return HDB_IPC_OK;
}

int HdbIpcReadInt64(const HdbIpcField& field, HdbIpcInt64* value)
{
    if (value == NULL)
    {
        return HDB_IPC_ERR_PARAM;
    }
    if (field.data == NULL || field.length != 8)
    {
        return HDB_IPC_ERR_FIELD;
    }

    *value = (HdbIpcInt64)HdbIpcReadUInt64(field.data);
    return HDB_IPC_OK;
}

int HdbIpcReadString(const HdbIpcField& field, std::string& value)
{
    value.clear();
    if (field.length == 0)
    {
        return HDB_IPC_OK;
    }
    if (field.data == NULL)
    {
        return HDB_IPC_ERR_FIELD;
    }

    value.assign((const char*)field.data, field.length);
    return HDB_IPC_OK;
}
