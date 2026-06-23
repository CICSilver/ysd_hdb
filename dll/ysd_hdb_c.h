#ifndef YSD_HDB_C_API_H
#define YSD_HDB_C_API_H

#include "../common/HdbCommon.h"

#ifdef __cplusplus
#define HDB_EXTERN_C extern "C"
#else
#define HDB_EXTERN_C extern
#endif

#ifdef _WIN32
#define HDB_CALL __stdcall
#if defined(YSD_HDB_DLL_EXPORTS) || defined(YSDHDBDLL_EXPORTS)
#define HDB_API HDB_EXTERN_C __declspec(dllexport)
#else
#define HDB_API HDB_EXTERN_C __declspec(dllimport)
#endif
#else
#define HDB_CALL
#if defined(__GNUC__) && (defined(YSD_HDB_DLL_EXPORTS) || defined(YSDHDBDLL_EXPORTS))
#define HDB_API HDB_EXTERN_C __attribute__((visibility("default")))
#else
#define HDB_API HDB_EXTERN_C
#endif
#endif

typedef struct HdbSessionTag* HDB_SESSION;     // DLL 到 SERVER 的 IPC 会话, 由 HdbClose 释放
typedef struct HdbQueryTag* HDB_QUERY;         // 逻辑查询句柄, 由 HdbQueryFree 释放
typedef struct HdbQuerySourceTag* HDB_SOURCE;  // query 内显式 source 句柄, 随 HDB_QUERY 释放
typedef struct HdbResultTag* HDB_RESULT;       // 查询结果句柄, 由 HdbResultFree 释放

// 同一个 HDB_QUERY、HDB_SOURCE 或 HDB_RESULT 按单线程使用
// DLL 内部 active source 表已加锁，避免不同 query 注册 source 时互相破坏
// 注意，调用DLL不需要该头文件，仅供DLL导出和跨边界使用
// 目前HdbSource不支持并发

// 创建 DLL 侧 session 句柄，当前不会直接连接数据库
HDB_API int HDB_CALL HdbOpen(const char* profileName, HDB_SESSION* outSession);
// 当前连接串不从 DLL 传入，数据库连接由 SERVER 启动配置决定
HDB_API int HDB_CALL HdbOpenByConnInfo(const char* connInfo, HDB_SESSION* outSession);
// 释放 HDB_SESSION，返回后 session 句柄不可再用于任何接口
HDB_API int HDB_CALL HdbClose(HDB_SESSION session);
// 通过 IPC 探测 SERVER 和数据库 adapter 状态
HDB_API int HDB_CALL HdbPing(HDB_SESSION session);

// 复制最近错误文本，requiredSize 包含结尾零，buffer 不足时返回 HDB_ERR_BUFFER
HDB_API int HDB_CALL HdbGetLastError(HDB_SESSION session,
    char* buffer,
    int bufferSize,
    int* requiredSize);

// 当前预留接口
// datasetName 是逻辑数据集名，不是物理表名
// row 按连续内存解释，rowSize 对齐 SERVER 元数据里的 modelSize
HDB_API int HDB_CALL HdbInsertRow(HDB_SESSION session,
    const char* datasetName,
    const void* row,
    int rowSize);

// 当前预留接口
// rows 是 rowCount 条连续 row 内存，每条 row 的大小都按 rowSize 解释
HDB_API int HDB_CALL HdbBatchInsertRows(HDB_SESSION session,
    const char* datasetName,
    const void* rows,
    int rowSize,
    int rowCount);

// 创建逻辑查询句柄，ROOT 数据集必须通过 HdbQueryFrom 显式设置
HDB_API int HDB_CALL HdbQueryCreate(HDB_SESSION session, HDB_QUERY* outQuery);
// 释放未执行或已执行的查询句柄，同时释放 query 创建的所有 HDB_SOURCE
HDB_API int HDB_CALL HdbQueryFree(HDB_QUERY query);
// 设置查询 ROOT source，一个 query 中只允许成功调用一次
HDB_API int HDB_CALL HdbQueryFrom(HDB_QUERY query,
    const char* datasetName,
    HDB_SOURCE* outRootSource);
// 通过 parent source 的 Association 显式创建 JOIN source，joinType 取 HdbJoinType
HDB_API int HDB_CALL HdbQueryJoin(HDB_QUERY query,
    HDB_SOURCE fromSource,
    const char* associationName,
    int joinType,
    HDB_SOURCE* outTargetSource);
// 查询时间范围使用 epoch milliseconds，SERVER 侧按左闭右开范围拼条件
HDB_API int HDB_CALL HdbQueryTimeRange(HDB_QUERY query, HdbInt64 beginMs, HdbInt64 endMs);
// select 字段必须属于传入 source，不接受点号路径
HDB_API int HDB_CALL HdbQuerySelect(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* outputName);
// where 条件只描述 source 字段、比较符和值，SERVER 侧还会校验字段类型
HDB_API int HDB_CALL HdbQueryWhereInt32(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, int value);
HDB_API int HDB_CALL HdbQueryWhereInt64(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, HdbInt64 value);
HDB_API int HDB_CALL HdbQueryWhereDouble(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int op, double value);
HDB_API int HDB_CALL HdbQueryWhereStringEq(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* value);
HDB_API int HDB_CALL HdbQueryWhereStringLike(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, const char* pattern);
// 排序字段同样必须属于传入 source，不接收 SQL order 片段
HDB_API int HDB_CALL HdbQueryOrderBy(HDB_QUERY query, HDB_SOURCE source, const char* fieldName, int orderType);
// limit 为 0 时使用 SERVER 默认上限，offset 保持非负
HDB_API int HDB_CALL HdbQueryLimit(HDB_QUERY query, int limit, int offset);
// 执行成功后 outResult 交给调用方，后续用 HdbResultFree 释放
HDB_API int HDB_CALL HdbQueryExecute(HDB_QUERY query, HDB_RESULT* outResult);

// 释放查询结果句柄，释放后所有列名和单元格缓存都失效
HDB_API int HDB_CALL HdbResultFree(HDB_RESULT result);
// 游标前进一行，hasRow 为 0 表示已经到末尾
HDB_API int HDB_CALL HdbResultNext(HDB_RESULT result, int* hasRow);
// 读取值前先判断 NULL，数值 getter 遇到 NULL 会返回 HDB_ERR_NULL_VALUE
HDB_API int HDB_CALL HdbResultIsNull(HDB_RESULT result, const char* outputName, int* isNull);
// 数值 getter 按 result schema 校验类型后再转换文本值
HDB_API int HDB_CALL HdbResultGetInt32(HDB_RESULT result, const char* outputName, int* value);
// HDB_FT_TIMESTAMP_MS 对外按 epoch milliseconds 的 HdbInt64 读取
HDB_API int HDB_CALL HdbResultGetInt64(HDB_RESULT result, const char* outputName, HdbInt64* value);
HDB_API int HDB_CALL HdbResultGetDouble(HDB_RESULT result, const char* outputName, double* value);
// 字符串 getter 采用调用方缓冲区，requiredSize 同样包含结尾零
HDB_API int HDB_CALL HdbResultGetString(HDB_RESULT result,
    const char* outputName,
    char* buffer,
    int bufferSize,
    int* requiredSize);

#endif
