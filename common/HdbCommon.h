#ifndef YSD_HDB_COMMON_PUBLIC_H
#define YSD_HDB_COMMON_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef long long HdbInt64; // DLL 对外 64 位整数

// DLL 对外错误码
enum HdbErrorCode
{
    HDB_OK = 0, // 成功
    HDB_ERR_PARAM = -1, // 参数为空、长度非法或字段值不符合接口要求
    HDB_ERR_NOT_CONNECTED = -2, // 数据库连接尚未建立
    HDB_ERR_DB_CONNECT = -3, // 数据库连接失败
    HDB_ERR_DB_EXEC = -4, // SQL 执行失败或影响行数不符合预期
    HDB_ERR_NO_RECORD = -5, // 查询成功但没有匹配记录
    HDB_ERR_MODEL_DEF = -6, // Model 元数据定义非法
    HDB_ERR_BUFFER = -7, // 缓冲区长度不足或数据写入缓冲区失败
    HDB_ERR_DATASET_DEF = -8, // 逻辑数据集元数据定义非法
    HDB_ERR_DATASET_NOT_FOUND = -9, // 调用方传入的逻辑数据集不存在
    HDB_ERR_FIELD_NOT_FOUND = -10, // 字段名在指定数据集内不存在
    HDB_ERR_FIELD_REF = -11, // 字段引用格式非法或无法解析到字段
    HDB_ERR_QUERY_NEED_TIME_RANGE = -13, // 按时间分片的数据集查询缺少时间范围
    HDB_ERR_QUERY_RANGE = -14, // 查询时间范围、分页范围或排序条件非法
    HDB_ERR_SHARD_DEF = -15, // 分片元数据定义非法
    HDB_ERR_SHARD_NOT_FOUND = -16, // 要访问的物理分片表不存在
    HDB_ERR_NOT_IMPLEMENTED = -17, // 预留接口尚未实现
    HDB_ERR_NULL_VALUE = -18, // 当前值为 NULL，不按普通值读取
    HDB_ERR_TYPE_MISMATCH = -19, // 字段类型或文本转换结果与读取接口不匹配
    HDB_ERR_INTERNAL = -20 // 模块内部发生未预期异常
};

enum HdbCompareOp
{
    HDB_OP_EQ = 1, // 等于
    HDB_OP_NE = 2, // 不等于
    HDB_OP_GT = 3, // 大于
    HDB_OP_GE = 4, // 大于等于
    HDB_OP_LT = 5, // 小于
    HDB_OP_LE = 6, // 小于等于
    HDB_OP_LIKE = 7 // 字符串 LIKE 匹配
};

enum HdbOrderType
{
    HDB_ORDER_ASC = 1, // 升序
    HDB_ORDER_DESC = 2 // 降序
};

enum HdbJoinType
{
    HDB_JOIN_INNER = 1, // 内连接，右侧无匹配时整行不返回
    HDB_JOIN_LEFT = 2 // 左连接，右侧无匹配时右侧字段返回 NULL
};

// DLL 和 result schema 共用字段类型
enum HdbFieldType
{
    HDB_FT_INT32 = 1, // 32 位有符号整数，对应数据库 integer/int4
    HDB_FT_INT64 = 2, // 64 位有符号整数，对应数据库 bigint/int8
    HDB_FT_DOUBLE = 3, // 双精度浮点数，对应数据库 double precision
    HDB_FT_SMALLINT = 4, // 16 位有符号整数，对应数据库 smallint/int2
    HDB_FT_CHAR_ARRAY = 5, // 固定长度 char 数组，按字符串字段处理
    HDB_FT_TIMESTAMP_MS = 6 // 毫秒时间戳，Model 中存 epoch ms，数据库按 timestamp 写入
};

#define HDB_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define HDB_QUERY_DEFAULT_LIMIT 1000 // 默认查询行数
#define HDB_QUERY_MAX_LIMIT 10000 // 查询行数硬上限

#ifdef __cplusplus
}
#endif

#endif
