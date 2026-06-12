#ifndef YSD_HDB_DLL_API_H
#define YSD_HDB_DLL_API_H

#ifdef _WIN32
#define HDB_CALL __stdcall
#if defined(YSD_HDB_DLL_EXPORTS) || defined(YSDHDBDLL_EXPORTS)
#define HDB_API extern "C" __declspec(dllexport)
#else
#define HDB_API extern "C" __declspec(dllimport)
#endif
#else
#define HDB_CALL
#define HDB_API extern "C"
#endif

typedef long long HdbInt64;

typedef struct HdbSessionTag* HDB_SESSION;
typedef struct HdbQueryTag* HDB_QUERY;
typedef struct HdbResultTag* HDB_RESULT;

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

HDB_API int HDB_CALL HdbOpen(const char* profileName, HDB_SESSION* outSession);
HDB_API int HDB_CALL HdbOpenByConnInfo(const char* connInfo, HDB_SESSION* outSession);
HDB_API int HDB_CALL HdbClose(HDB_SESSION session);
HDB_API int HDB_CALL HdbPing(HDB_SESSION session);

HDB_API int HDB_CALL HdbGetLastError(HDB_SESSION session,
    char* buffer,
    int bufferSize,
    int* requiredSize);

HDB_API int HDB_CALL HdbInsertRow(HDB_SESSION session,
    const char* datasetName,
    const void* row,
    int rowSize);

HDB_API int HDB_CALL HdbBatchInsertRows(HDB_SESSION session,
    const char* datasetName,
    const void* rows,
    int rowSize,
    int rowCount);

HDB_API int HDB_CALL HdbQueryCreate(HDB_SESSION session,
    const char* datasetName,
    HDB_QUERY* outQuery);
HDB_API int HDB_CALL HdbQueryFree(HDB_QUERY query);
HDB_API int HDB_CALL HdbQueryTimeRange(HDB_QUERY query, HdbInt64 beginMs, HdbInt64 endMs);
HDB_API int HDB_CALL HdbQuerySelectPath(HDB_QUERY query, const char* fieldPath, const char* outputName);
HDB_API int HDB_CALL HdbQueryWhereInt32(HDB_QUERY query, const char* fieldPath, int op, int value);
HDB_API int HDB_CALL HdbQueryWhereInt64(HDB_QUERY query, const char* fieldPath, int op, HdbInt64 value);
HDB_API int HDB_CALL HdbQueryWhereDouble(HDB_QUERY query, const char* fieldPath, int op, double value);
HDB_API int HDB_CALL HdbQueryWhereStringEq(HDB_QUERY query, const char* fieldPath, const char* value);
HDB_API int HDB_CALL HdbQueryWhereStringLike(HDB_QUERY query, const char* fieldPath, const char* pattern);
HDB_API int HDB_CALL HdbQueryOrderBy(HDB_QUERY query, const char* fieldPath, int orderType);
HDB_API int HDB_CALL HdbQueryLimit(HDB_QUERY query, int limit, int offset);
HDB_API int HDB_CALL HdbQueryExecute(HDB_QUERY query, HDB_RESULT* outResult);

HDB_API int HDB_CALL HdbResultFree(HDB_RESULT result);
HDB_API int HDB_CALL HdbResultNext(HDB_RESULT result, int* hasRow);
HDB_API int HDB_CALL HdbResultIsNull(HDB_RESULT result, const char* outputName, int* isNull);
HDB_API int HDB_CALL HdbResultGetInt32(HDB_RESULT result, const char* outputName, int* value);
HDB_API int HDB_CALL HdbResultGetInt64(HDB_RESULT result, const char* outputName, HdbInt64* value);
HDB_API int HDB_CALL HdbResultGetDouble(HDB_RESULT result, const char* outputName, double* value);
HDB_API int HDB_CALL HdbResultGetString(HDB_RESULT result,
    const char* outputName,
    char* buffer,
    int bufferSize,
    int* requiredSize);

#endif
