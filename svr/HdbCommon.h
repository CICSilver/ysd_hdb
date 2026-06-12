#ifndef YSD_HDB_COMMON_H
#define YSD_HDB_COMMON_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef _WIN32
#define HDB_SNPRINTF _snprintf
#define HDB_INT64_FORMAT "%I64d"
#else
#define HDB_SNPRINTF snprintf
#define HDB_INT64_FORMAT "%lld"
#endif

typedef long long HdbInt64;

enum HdbErrorCode
{
    HDB_OK = 0, // 成功。
    HDB_ERR_PARAM = -1, // 参数为空、长度非法或字段值不符合接口要求。
    HDB_ERR_NOT_CONNECTED = -2, // 数据库连接尚未建立。
    HDB_ERR_DB_CONNECT = -3, // 数据库连接失败。
    HDB_ERR_DB_EXEC = -4, // SQL 执行失败或影响行数不符合预期。
    HDB_ERR_NO_RECORD = -5, // 查询成功，但没有匹配记录。
    HDB_ERR_MODEL_DEF = -6, // Model 元数据定义非法。
    HDB_ERR_BUFFER = -7, // 缓冲区长度不足或数据写入缓冲区失败。
    HDB_ERR_DATASET_DEF = -8, // 逻辑数据集元数据定义非法。
    HDB_ERR_DATASET_NOT_FOUND = -9, // 调用方传入的逻辑数据集不存在。
    HDB_ERR_FIELD_NOT_FOUND = -10, // 字段名在指定数据集内不存在。
    HDB_ERR_FIELD_PATH = -11, // 字段路径格式非法或无法解析到字段。
    HDB_ERR_RELATION_NOT_FOUND = -12, // 字段路径中的 Relation 不存在。
    HDB_ERR_QUERY_NEED_TIME_RANGE = -13, // 按时间分片的数据集查询缺少时间范围。
    HDB_ERR_QUERY_RANGE = -14, // 查询时间范围、分页范围或排序条件非法。
    HDB_ERR_SHARD_DEF = -15, // 分片元数据定义非法。
    HDB_ERR_SHARD_NOT_FOUND = -16, // 必须访问的物理分片表不存在。
    HDB_ERR_NOT_IMPLEMENTED = -17 // 接口已预留但当前阶段尚未实现。
};

enum HdbCompareOp
{
    HDB_OP_EQ = 1, // 等于。
    HDB_OP_NE = 2, // 不等于。
    HDB_OP_GT = 3, // 大于。
    HDB_OP_GE = 4, // 大于等于。
    HDB_OP_LT = 5, // 小于。
    HDB_OP_LE = 6, // 小于等于。
    HDB_OP_LIKE = 7 // 字符串 LIKE 匹配。
};

enum HdbOrderType
{
    HDB_ORDER_ASC = 1, // 升序。
    HDB_ORDER_DESC = 2 // 降序。
};

#define HDB_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#endif
