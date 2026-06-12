#ifndef YSD_HDB_IPC_PROTOCOL_H
#define YSD_HDB_IPC_PROTOCOL_H

#include <stddef.h>

#include <string>
#include <vector>

typedef long long HdbIpcInt64;

#define HDB_IPC_MAGIC 0x49424448u
#define HDB_IPC_VERSION 1
#define HDB_IPC_MAX_BODY_LENGTH (16u * 1024u * 1024u)

// 当前帧是请求帧。
#define HDB_IPC_FLAG_REQUEST 0x00000001u
// 当前帧是响应帧。
#define HDB_IPC_FLAG_RESPONSE 0x00000002u
// 响应状态为错误，status 中保存错误码。
#define HDB_IPC_FLAG_ERROR 0x00000004u

// IPC 协议解析和组包错误码，与数据库错误码分开。
enum HdbIpcErrorCode
{
    HDB_IPC_OK = 0, // 成功。
    HDB_IPC_ERR_PARAM = -1001, // 参数为空、命令号非法或字段类型非法。
    HDB_IPC_ERR_INCOMPLETE = -1002, // 接收缓冲区还不够一个完整帧。
    HDB_IPC_ERR_MAGIC = -1003, // magic 不匹配，说明不是历史库 IPC 帧。
    HDB_IPC_ERR_VERSION = -1004, // 协议版本不匹配。
    HDB_IPC_ERR_HEADER = -1005, // 帧头长度、字段值或请求/响应标记非法。
    HDB_IPC_ERR_BODY_SIZE = -1006, // body 超过协议允许的最大长度。
    HDB_IPC_ERR_CHECKSUM = -1007, // body checksum 校验失败。
    HDB_IPC_ERR_FIELD = -1008, // TLV 字段长度、类型或读取格式非法。
    HDB_IPC_ERR_BUFFER = -1009 // 输出缓冲区不足或缓存处理失败。
};

// DLL 请求侧和 SERVER 分发侧共用的命令号。
enum HdbIpcCommand
{
    HDB_IPC_CMD_NONE = 0, // 空命令，不允许作为有效请求发送。
    HDB_IPC_CMD_PING = 1, // IPC 层连通性探测，不访问数据库。
    HDB_IPC_CMD_DB_OPEN = 10, // 打开数据库连接，请求体携带连接串。
    HDB_IPC_CMD_DB_CLOSE = 11, // 关闭数据库连接。
    HDB_IPC_CMD_DB_PING = 12, // 数据库连接探测，对应 SERVER 内部 Ping。
    HDB_IPC_CMD_MODEL_INSERT = 100, // 插入一条 Model 数据。
    HDB_IPC_CMD_MODEL_UPDATE = 101, // 按主键更新一条 Model 数据。
    HDB_IPC_CMD_MODEL_DELETE = 102, // 按主键删除一条 Model 数据。
    HDB_IPC_CMD_MODEL_SELECT_BY_PK = 103, // 按主键查询单条 Model 数据。
    HDB_IPC_CMD_MODEL_SELECT_LIST = 104, // 查询 Model 列表，后续用于条件查询、分页和批量读取。
    HDB_IPC_CMD_DATASET_INSERT = 150, // 按逻辑数据集插入一条数据。
    HDB_IPC_CMD_DATASET_BATCH_INSERT = 151, // 按逻辑数据集批量插入数据。
    HDB_IPC_CMD_QUERY_EXECUTE = 200, // 执行逻辑查询描述，不传递 SQL。
    HDB_IPC_CMD_RESULT_FETCH = 201, // 分页读取 SERVER 保存的查询结果。
    HDB_IPC_CMD_RESULT_CLOSE = 202 // 关闭 SERVER 保存的查询结果。
};

// body 使用 TLV 字段，后续扩展命令时不需要修改帧头。
enum HdbIpcFieldType
{
    HDB_IPC_FIELD_NONE = 0, // 空字段类型，不允许作为有效字段写入。
    HDB_IPC_FIELD_CONN_INFO = 1, // 数据库连接串，字符串。
    HDB_IPC_FIELD_MODEL_NAME = 2, // Model 名称或表模型标识，字符串。
    HDB_IPC_FIELD_MODEL_DATA = 3, // Model 结构体原始二进制数据。
    HDB_IPC_FIELD_KEY_DATA = 4, // 主键 Model 或主键字段原始二进制数据。
    HDB_IPC_FIELD_RESULT_DATA = 5, // 查询结果 Model 原始二进制数据。
    HDB_IPC_FIELD_ERROR_TEXT = 6, // 错误文本，字符串。
    HDB_IPC_FIELD_FOUND = 7, // 是否查到记录，int32，0 表示未找到，非 0 表示找到。
    HDB_IPC_FIELD_AFFECTED_ROWS = 8, // SQL 影响行数，int32。
    HDB_IPC_FIELD_LIMIT = 9, // 列表查询最大返回条数，int32。
    HDB_IPC_FIELD_OFFSET = 10, // 列表查询起始偏移，int32。
    HDB_IPC_FIELD_DATASET_NAME = 20, // 逻辑数据集名称，字符串。
    HDB_IPC_FIELD_QUERY_AST = 21, // 查询 AST 序列化内容，不是 SQL。
    HDB_IPC_FIELD_RESULT_SCHEMA = 22, // 查询结果 schema，包含 outputName 和类型信息。
    HDB_IPC_FIELD_RESULT_ROWS = 23, // 查询结果行数据。
    HDB_IPC_FIELD_CURSOR_ID = 24, // SERVER 侧结果游标编号，int64。
    HDB_IPC_FIELD_HAS_MORE = 25, // SERVER 是否还有后续结果页，int32。
    HDB_IPC_FIELD_PAGE_SIZE = 26 // 单次 fetch 返回的最大行数，int32。
};

// 固定帧头。当前用于本机 IPC，多字节字段按本机字节序保存。
#pragma pack(push, 1)
struct HdbIpcFrameHeader
{
    unsigned int magic;
    unsigned short version;
    unsigned short headerSize;
    unsigned int command;
    unsigned int flags;
    unsigned int sequence;
    int status;
    unsigned int bodyLength;
    unsigned int bodyChecksum;
};

// body 字段头。每个字段头后面紧跟 length 字节原始数据。
struct HdbIpcFieldHeader
{
    unsigned short type;
    unsigned short flags;
    unsigned int length;
};
#pragma pack(pop)

struct HdbIpcFrame
{
    HdbIpcFrameHeader header;
    const unsigned char* body;
    unsigned int bodyLength;
};

struct HdbIpcField
{
    unsigned short type;
    unsigned short flags;
    const unsigned char* data;
    unsigned int length;
};

class CHdbIpcFieldReader
{
public:
    CHdbIpcFieldReader();

    int Reset(const unsigned char* body, unsigned int bodyLength);
    // 读到 body 末尾时返回 hasField=0。
    int Next(HdbIpcField& field, int* hasField);

private:
    const unsigned char* m_body;
    unsigned int m_bodyLength;
    unsigned int m_offset;
};

unsigned int HdbIpcCalcChecksum(const void* data, unsigned int length);

// 构造完整帧缓冲区，传输层可以直接写出。
int HdbIpcBuildFrame(unsigned int command,
    unsigned int flags,
    unsigned int sequence,
    int status,
    const void* body,
    unsigned int bodyLength,
    std::vector<unsigned char>& outFrame);

int HdbIpcBuildRequest(unsigned int command,
    unsigned int sequence,
    const void* body,
    unsigned int bodyLength,
    std::vector<unsigned char>& outFrame);

int HdbIpcBuildResponse(unsigned int command,
    unsigned int sequence,
    int status,
    const void* body,
    unsigned int bodyLength,
    std::vector<unsigned char>& outFrame);

// 检查接收缓冲区；数据足够时返回完整帧长度。
int HdbIpcGetFrameSize(const unsigned char* data,
    unsigned int dataLength,
    unsigned int* frameSize);

// 解析并校验完整帧。返回的 body 指针引用输入缓冲区。
int HdbIpcParseFrame(const unsigned char* data,
    unsigned int dataLength,
    HdbIpcFrame& frame);

// 向 body 追加一个 TLV 字段。
int HdbIpcAppendField(std::vector<unsigned char>& body,
    unsigned short type,
    const void* data,
    unsigned int length);

int HdbIpcAppendInt32(std::vector<unsigned char>& body,
    unsigned short type,
    int value);

int HdbIpcAppendInt64(std::vector<unsigned char>& body,
    unsigned short type,
    HdbIpcInt64 value);

int HdbIpcAppendString(std::vector<unsigned char>& body,
    unsigned short type,
    const char* value);

int HdbIpcReadInt32(const HdbIpcField& field, int* value);
int HdbIpcReadInt64(const HdbIpcField& field, HdbIpcInt64* value);
int HdbIpcReadString(const HdbIpcField& field, std::string& value);

#endif
