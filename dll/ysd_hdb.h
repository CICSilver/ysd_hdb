#ifndef YSD_HDB_DLL_API_H
#define YSD_HDB_DLL_API_H

#include "../common/HdbCommon.h"

#ifdef _WIN32
#define HDB_CALL __stdcall
#if defined(YSD_HDB_DLL_EXPORTS) || defined(YSDHDBDLL_EXPORTS)
#define HDB_API extern "C" __declspec(dllexport)
#else
#define HDB_API extern "C" __declspec(dllimport)
#endif
#else
#define HDB_CALL
#if defined(__GNUC__) && (defined(YSD_HDB_DLL_EXPORTS) || defined(YSDHDBDLL_EXPORTS))
#define HDB_API extern "C" __attribute__((visibility("default")))
#else
#define HDB_API extern "C"
#endif
#endif

typedef struct HdbSessionTag* HDB_SESSION;
typedef struct HdbQueryTag* HDB_QUERY;
typedef struct HdbResultTag* HDB_RESULT;

// XXX 当前版本 HDB_SESSION 不保证多线程并发调用安全
// XXX HDB_QUERY 和 HDB_RESULT 只支持单线程构造、执行和遍历
// XXX 多线程场景请每个线程独立创建 HDB_SESSION，且不要共享 HDB_QUERY/HDB_RESULT

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
