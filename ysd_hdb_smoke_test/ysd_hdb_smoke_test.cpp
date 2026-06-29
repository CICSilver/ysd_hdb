#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../dll/ysd_hdb.h"

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <windows.h>

static WORD g_defaultConsoleAttr = 0;
static int g_consoleAttrReady = 0;

static const char* ReadDefaultConnInfo()
{
    const char* envConn;

    envConn = getenv("HDB_PG_CONNINFO");
    if (envConn != NULL && envConn[0] != '\0')
    {
        return envConn;
    }
    return "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=postgres";
}

static HdbInt64 MakeLocalTimeMs(int year, int month, int day, int hour, int minute, int second, int millis)
{
    struct tm tmValue;
    time_t secondsValue;

    memset(&tmValue, 0, sizeof(tmValue));
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = minute;
    tmValue.tm_sec = second;
    tmValue.tm_isdst = -1;
    secondsValue = mktime(&tmValue);
    return ((HdbInt64)secondsValue) * 1000 + millis;
}

static void InitConsoleAttr()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle;

    if (g_consoleAttrReady != 0)
    {
        return;
    }
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(handle, &info))
    {
        g_defaultConsoleAttr = info.wAttributes;
    }
    else
    {
        g_defaultConsoleAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }
    g_consoleAttrReady = 1;
}

static void SetConsoleAttr(WORD attr)
{
    HANDLE handle;

    InitConsoleAttr();
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE)
    {
        SetConsoleTextAttribute(handle, attr);
    }
}

static void ResetConsoleAttr()
{
    SetConsoleAttr(g_defaultConsoleAttr);
}

static void PrintTestTitle(const char* name, const char* target)
{
    printf("\n[%s]\n", name);
    printf("目标: %s\n", target);
}

static void PrintOk(const char* text)
{
    SetConsoleAttr(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("[√] %s\n", text);
    ResetConsoleAttr();
}

static void PrintFail(const char* text)
{
    SetConsoleAttr(FOREGROUND_RED | FOREGROUND_INTENSITY);
    printf("[x] %s\n", text);
    ResetConsoleAttr();
}

static int SmokePauseDisabled()
{
    const char* value;

    value = getenv("HDB_SMOKE_NOPAUSE");
    return value != NULL && value[0] == '1';
}

static void PauseBeforeExit(int exitCode)
{
    if (SmokePauseDisabled())
    {
        return;
    }
    printf("\n");
    if (exitCode == 0)
    {
        printf("测试完成，按回车退出");
    }
    else
    {
        printf("测试失败，按回车退出");
    }
    fflush(stdout);
    getchar();
}

static void PrintSessionError(HDB_SESSION session, const char* stage, int ret)
{
    char errorText[512];
    int requiredSize;

    errorText[0] = '\0';
    requiredSize = 0;
    if (session != NULL)
    {
        HdbGetLastError(session, errorText, sizeof(errorText), &requiredSize);
        errorText[sizeof(errorText) - 1] = '\0';
    }
    PrintFail(stage);
    printf("ret=%d error=%s\n", ret, errorText);
}

static int ExpectHdbOk(HDB_SESSION session, int ret, const char* stage)
{
    if (ret == HDB_OK)
    {
        return 0;
    }
    PrintSessionError(session, stage, ret);
    return 1;
}

static int ExpectHdbError(HDB_SESSION session, int ret, int expected, const char* stage)
{
    if (ret == expected)
    {
        return 0;
    }
    PrintSessionError(session, stage, ret);
    printf("%s expected ret=%d\n", stage, expected);
    return 1;
}

static int ExpectQueryStep(HDB_SESSION session, HDB_QUERY query, int ret, const char* stage)
{
    if (ExpectHdbOk(session, ret, stage) == 0)
    {
        return 0;
    }
    if (query != NULL)
    {
        HdbQueryFree(query);
    }
    return 1;
}

static int PgExecCommand(PGconn* conn, const char* sql)
{
    PGresult* result;
    ExecStatusType status;

    result = PQexec(conn, sql);
    if (result == NULL)
    {
        PrintFail("postgres command failed");
        printf("%s\n", PQerrorMessage(conn));
        return 1;
    }
    status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK)
    {
        PrintFail("postgres command failed");
        printf("%s\nsql=%s\n", PQerrorMessage(conn), sql);
        PQclear(result);
        return 1;
    }
    PQclear(result);
    return 0;
}

// 准备 SERVER 查询用的固定分表数据
static int PrepareHistoryFixture(const char* connInfo)
{
    PGconn* conn;
    const char* sqlList[] =
    {
        "begin",
        "drop table if exists hdb_alarm_20260612",
        "drop table if exists hdb_alarm_20260613",
        "drop table if exists hdb_point",
        "drop table if exists hdb_device",
        "create table hdb_alarm_20260612 (id bigint not null primary key, point_id bigint, level integer not null, message varchar(128), occur_time timestamp not null)",
        "create table hdb_alarm_20260613 (id bigint not null primary key, point_id bigint, level integer not null, message varchar(128), occur_time timestamp not null)",
        "create table hdb_point (id bigint not null primary key, device_id bigint, name varchar(128) not null)",
        "create table hdb_device (id bigint not null primary key, name varchar(128) not null)",
        "insert into hdb_device(id, name) values (200, 'device A')",
        "insert into hdb_point(id, device_id, name) values (100, 200, 'point A')",
        "insert into hdb_point(id, device_id, name) values (101, 999, 'point B')",
        "insert into hdb_alarm_20260612(id, point_id, level, message, occur_time) values (1, 100, 3, 'alarm day one', '2026-06-12 10:00:00.000')",
        "insert into hdb_alarm_20260613(id, point_id, level, message, occur_time) values (2, 101, 4, 'alarm day two', '2026-06-13 11:00:00.000')",
        "commit"
    };
    int index;

    PrintTestTitle("准备数据", "准备日分片和关联表的固定数据");
    conn = PQconnectdb(connInfo);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        PrintFail("open postgres failed");
        printf("%s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return 1;
    }
    for (index = 0; index < (int)HDB_ARRAY_COUNT(sqlList); ++index)
    {
        if (PgExecCommand(conn, sqlList[index]) != 0)
        {
            PgExecCommand(conn, "rollback");
            PQfinish(conn);
            return 1;
        }
    }
    PQfinish(conn);
    PrintOk("数据准备完成");
    return 0;
}

static int ExpectNext(HDB_SESSION session, HDB_RESULT result, int expected, const char* stage)
{
    int hasRow;
    int ret;

    hasRow = 0;
    ret = HdbResultNext(result, &hasRow);
    if (ret != HDB_OK)
    {
        PrintSessionError(session, stage, ret);
        return 1;
    }
    if (hasRow != expected)
    {
        PrintFail(stage);
        printf("hasRow=%d expected=%d\n", hasRow, expected);
        return 1;
    }
    return 0;
}

static int ExpectInt32(HDB_SESSION session, HDB_RESULT result, const char* name, int expected)
{
    int value;
    int ret;

    value = 0;
    ret = HdbResultGetInt32(result, name, &value);
    if (ret != HDB_OK)
    {
        PrintSessionError(session, name, ret);
        return 1;
    }
    if (value != expected)
    {
        PrintFail(name);
        printf("value=%d expected=%d\n", value, expected);
        return 1;
    }
    return 0;
}

static int ExpectInt64(HDB_SESSION session, HDB_RESULT result, const char* name, HdbInt64 expected)
{
    HdbInt64 value;
    int ret;

    value = 0;
    ret = HdbResultGetInt64(result, name, &value);
    if (ret != HDB_OK)
    {
        PrintSessionError(session, name, ret);
        return 1;
    }
    if (value != expected)
    {
        PrintFail(name);
        printf("int64 value mismatch\n");
        return 1;
    }
    return 0;
}

static int ExpectString(HDB_SESSION session, HDB_RESULT result, const char* name, const char* expected)
{
    char value[128];
    int requiredSize;
    int ret;

    value[0] = '\0';
    requiredSize = 0;
    ret = HdbResultGetString(result, name, value, sizeof(value), &requiredSize);
    if (ret != HDB_OK)
    {
        PrintSessionError(session, name, ret);
        return 1;
    }
    value[sizeof(value) - 1] = '\0';
    if (strcmp(value, expected) != 0)
    {
        PrintFail(name);
        printf("value=%s expected=%s\n", value, expected);
        return 1;
    }
    return 0;
}

static int ExpectNull(HDB_SESSION session, HDB_RESULT result, const char* name, int expected)
{
    int isNull;
    int ret;

    isNull = 0;
    ret = HdbResultIsNull(result, name, &isNull);
    if (ret != HDB_OK)
    {
        PrintSessionError(session, name, ret);
        return 1;
    }
    if (isNull != expected)
    {
        PrintFail(name);
        printf("isNull=%d expected=%d\n", isNull, expected);
        return 1;
    }
    return 0;
}

// 测试 SERVER 和数据库连接
static int RunPingSmoke(HDB_SESSION session)
{
    int ret;

    PrintTestTitle("连接探测", "通过 DLL 调用 HdbPing 验证 SERVER 和数据库连接");
    ret = HdbPing(session);
    if (ExpectHdbOk(session, ret, "HdbPing") != 0)
    {
        return 1;
    }
    PrintOk("连接探测通过");
    return 0;
}

// 测试日分片查询和两级关联
static int RunHistoryQuerySmoke(HDB_SESSION session)
{
    HDB_QUERY query;
    HDB_SOURCE alarmSource;
    HDB_SOURCE pointSource;
    HDB_SOURCE deviceSource;
    HDB_RESULT result;
    int ret;

    PrintTestTitle("历史查询", "覆盖日分片查询、两级联查、NULL 和类型读取");
    query = NULL;
    alarmSource = NULL;
    pointSource = NULL;
    deviceSource = NULL;
    result = NULL;

    ret = HdbQueryCreate(session, &query);
    if (ExpectHdbOk(session, ret, "create history query") != 0)
    {
        return 1;
    }
    ret = HdbQueryFrom(query, "alarm", &alarmSource);
    if (ExpectQueryStep(session, query, ret, "history from alarm") != 0)
    {
        return 1;
    }
    ret = HdbQueryJoinOn(query, alarmSource, "point", HDB_JOIN_LEFT, "point_id", "id", &pointSource);
    if (ExpectQueryStep(session, query, ret, "history join point") != 0)
    {
        return 1;
    }
    ret = HdbQueryJoinOn(query, pointSource, "device", HDB_JOIN_LEFT, "device_id", "id", &deviceSource);
    if (ExpectQueryStep(session, query, ret, "history join device") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, alarmSource, "id", "id");
    if (ExpectQueryStep(session, query, ret, "history select id") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, alarmSource, "level", "level");
    if (ExpectQueryStep(session, query, ret, "history select level") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, alarmSource, "occur_time", "time");
    if (ExpectQueryStep(session, query, ret, "history select time") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, pointSource, "name", "pointName");
    if (ExpectQueryStep(session, query, ret, "history select point") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, deviceSource, "name", "deviceName");
    if (ExpectQueryStep(session, query, ret, "history select device") != 0)
    {
        return 1;
    }
    ret = HdbQueryWhereInt32(query, alarmSource, "level", HDB_OP_GE, 2);
    if (ExpectQueryStep(session, query, ret, "history where level") != 0)
    {
        return 1;
    }
    ret = HdbQueryOrderBy(query, alarmSource, "occur_time", HDB_ORDER_DESC);
    if (ExpectQueryStep(session, query, ret, "history order time") != 0)
    {
        return 1;
    }
    ret = HdbQueryTimeRange(query, MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 14, 0, 0, 0, 0));
    if (ExpectQueryStep(session, query, ret, "history time range") != 0)
    {
        return 1;
    }
    ret = HdbQueryLimit(query, 10, 0);
    if (ExpectQueryStep(session, query, ret, "history limit") != 0)
    {
        return 1;
    }
    ret = HdbQueryExecute(query, &result);
    HdbQueryFree(query);
    if (ExpectHdbOk(session, ret, "execute history query") != 0)
    {
        return 1;
    }
    if (ExpectNext(session, result, 1, "history row 1") != 0 ||
        ExpectInt64(session, result, "id", 2) != 0 ||
        ExpectInt32(session, result, "level", 4) != 0 ||
        ExpectInt64(session, result, "time", MakeLocalTimeMs(2026, 6, 13, 11, 0, 0, 0)) != 0 ||
        ExpectString(session, result, "pointName", "point B") != 0 ||
        ExpectNull(session, result, "deviceName", 1) != 0 ||
        ExpectNext(session, result, 1, "history row 2") != 0 ||
        ExpectInt64(session, result, "id", 1) != 0 ||
        ExpectString(session, result, "pointName", "point A") != 0 ||
        ExpectString(session, result, "deviceName", "device A") != 0 ||
        ExpectNext(session, result, 0, "history row end") != 0)
    {
        HdbResultFree(result);
        return 1;
    }
    HdbResultFree(result);
    PrintOk("历史查询通过");
    return 0;
}

// 测试生成 DSL 链式查询
static int RunDslQuerySmoke(HDB_SESSION session)
{
    CHdbCreate create(session);
    CHdbDslResult result;
    HdbInt64 alarmId;
    std::string pointName;
    int hasRow;
    int ret;

    PrintTestTitle("DSL 查询", "覆盖生成字段、显式 JOIN 和字段读取");
    alarmId = 0;
    hasRow = 0;

    ret = create.select(HdbDsl::ALARM.ID)
        .select(HdbDsl::POINT.NAME)
        .from(HdbDsl::ALARM)
        .leftJoin(HdbDsl::POINT)
        .on(HdbDsl::ALARM.POINT_ID.eq(HdbDsl::POINT.ID).And(HdbDsl::POINT.NAME.like("point%")))
        .where(HdbDsl::ALARM.ID.eq((HdbInt64)1).And(HdbDsl::ALARM.POINT_ID.eq(HdbDsl::POINT.ID)))
        .orderBy(HdbDsl::ALARM.ID.asc())
        .timeRange(MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 13, 0, 0, 0, 0))
        .fetch(&result);
    if (ExpectHdbOk(session, ret, "execute dsl query") != 0)
    {
        return 1;
    }
    ret = result.Next(&hasRow);
    if (ExpectHdbOk(session, ret, "dsl row 1") != 0)
    {
        return 1;
    }
    if (hasRow != 1)
    {
        PrintFail("dsl row 1");
        printf("hasRow=%d expected=1\n", hasRow);
        return 1;
    }
    ret = result.Get(HdbDsl::ALARM.ID, alarmId);
    if (ExpectHdbOk(session, ret, "dsl alarm id") != 0)
    {
        return 1;
    }
    ret = result.Get(HdbDsl::POINT.NAME, pointName);
    if (ExpectHdbOk(session, ret, "dsl point name") != 0)
    {
        return 1;
    }
    if (alarmId != 1 || pointName != "point A")
    {
        PrintFail("dsl values");
        printf("alarmId mismatch or pointName mismatch\n");
        return 1;
    }
    ret = result.Next(&hasRow);
    if (ExpectHdbOk(session, ret, "dsl row end") != 0)
    {
        return 1;
    }
    if (hasRow != 0)
    {
        PrintFail("dsl row end");
        printf("hasRow=%d expected=0\n", hasRow);
        return 1;
    }
    PrintOk("DSL 查询通过");
    return 0;
}

// 测试 DSL 增、删、改
static int RunDslDmlSmoke(HDB_SESSION session)
{
    CHdbCreate create(session);
    CHdbDslResult result;
    std::string pointName;
    int affectedRows;
    int hasRow;
    int ret;

    PrintTestTitle("DSL DML", "覆盖 insert/update/delete 和影响行数");
    affectedRows = 0;
    ret = create.insertInto(HdbDsl::POINT)
        .set(HdbDsl::POINT.ID, (HdbInt64)2000)
        .set(HdbDsl::POINT.DEVICE_ID, (HdbInt64)200)
        .set(HdbDsl::POINT.NAME, "point C")
        .execute(&affectedRows);
    if (ExpectHdbOk(session, ret, "dsl insert point") != 0)
    {
        return 1;
    }
    if (affectedRows != 1)
    {
        PrintFail("dsl insert affected rows");
        printf("affectedRows=%d expected=1\n", affectedRows);
        return 1;
    }

    affectedRows = 0;
    ret = create.update(HdbDsl::POINT)
        .set(HdbDsl::POINT.NAME, "point C updated")
        .where(HdbDsl::POINT.ID.eq((HdbInt64)2000))
        .execute(&affectedRows);
    if (ExpectHdbOk(session, ret, "dsl update point") != 0)
    {
        return 1;
    }
    if (affectedRows != 1)
    {
        PrintFail("dsl update affected rows");
        printf("affectedRows=%d expected=1\n", affectedRows);
        return 1;
    }

    ret = create.select(HdbDsl::POINT.NAME)
        .from(HdbDsl::POINT)
        .where(HdbDsl::POINT.ID.eq((HdbInt64)2000))
        .fetch(&result);
    if (ExpectHdbOk(session, ret, "dsl select updated point") != 0)
    {
        return 1;
    }
    hasRow = 0;
    ret = result.Next(&hasRow);
    if (ExpectHdbOk(session, ret, "dsl updated point row") != 0 || hasRow != 1)
    {
        return 1;
    }
    ret = result.Get(HdbDsl::POINT.NAME, pointName);
    if (ExpectHdbOk(session, ret, "dsl updated point name") != 0)
    {
        return 1;
    }
    if (pointName != "point C updated")
    {
        PrintFail("dsl updated point name");
        printf("point name mismatch\n");
        return 1;
    }

    affectedRows = 0;
    ret = create.deleteFrom(HdbDsl::POINT)
        .where(HdbDsl::POINT.ID.eq((HdbInt64)2000))
        .execute(&affectedRows);
    if (ExpectHdbOk(session, ret, "dsl delete point") != 0)
    {
        return 1;
    }
    if (affectedRows != 1)
    {
        PrintFail("dsl delete affected rows");
        printf("affectedRows=%d expected=1\n", affectedRows);
        return 1;
    }

    PrintOk("DSL 增、删、改 测试通过");
    return 0;
}

// 测试时间字段条件
static int RunTimestampWhereSmoke(HDB_SESSION session)
{
    HDB_QUERY query;
    HDB_SOURCE alarmSource;
    HDB_RESULT result;
    int ret;

    PrintTestTitle("时间条件", "覆盖时间字段 where 条件和分片过滤");
    query = NULL;
    alarmSource = NULL;
    result = NULL;

    ret = HdbQueryCreate(session, &query);
    if (ExpectHdbOk(session, ret, "create timestamp query") != 0)
    {
        return 1;
    }
    ret = HdbQueryFrom(query, "alarm", &alarmSource);
    if (ExpectQueryStep(session, query, ret, "timestamp from alarm") != 0)
    {
        return 1;
    }
    ret = HdbQueryTimeRange(query, MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 14, 0, 0, 0, 0));
    if (ExpectQueryStep(session, query, ret, "timestamp time range") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, alarmSource, "id", "id");
    if (ExpectQueryStep(session, query, ret, "timestamp select id") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, alarmSource, "occur_time", "time");
    if (ExpectQueryStep(session, query, ret, "timestamp select time") != 0)
    {
        return 1;
    }
    ret = HdbQueryWhereInt64(query, alarmSource, "occur_time", HDB_OP_GE, MakeLocalTimeMs(2026, 6, 13, 0, 0, 0, 0));
    if (ExpectQueryStep(session, query, ret, "timestamp where time") != 0)
    {
        return 1;
    }
    ret = HdbQueryOrderBy(query, alarmSource, "occur_time", HDB_ORDER_DESC);
    if (ExpectQueryStep(session, query, ret, "timestamp order time") != 0)
    {
        return 1;
    }
    ret = HdbQueryLimit(query, 10, 0);
    if (ExpectQueryStep(session, query, ret, "timestamp limit") != 0)
    {
        return 1;
    }
    ret = HdbQueryExecute(query, &result);
    HdbQueryFree(query);
    if (ExpectHdbOk(session, ret, "execute timestamp query") != 0)
    {
        return 1;
    }
    if (ExpectNext(session, result, 1, "timestamp row") != 0 ||
        ExpectInt64(session, result, "id", 2) != 0 ||
        ExpectNext(session, result, 0, "timestamp row end") != 0)
    {
        HdbResultFree(result);
        return 1;
    }
    HdbResultFree(result);
    PrintOk("时间条件查询通过");
    return 0;
}

// 测试固定表查询和 LIKE
static int RunPointQuerySmoke(HDB_SESSION session)
{
    HDB_QUERY query;
    HDB_SOURCE pointSource;
    HDB_RESULT result;
    int ret;

    PrintTestTitle("固定表查询", "覆盖固定表 dataset 查询和 LIKE 条件");
    query = NULL;
    pointSource = NULL;
    result = NULL;

    ret = HdbQueryCreate(session, &query);
    if (ExpectHdbOk(session, ret, "create point query") != 0)
    {
        return 1;
    }
    ret = HdbQueryFrom(query, "point", &pointSource);
    if (ExpectQueryStep(session, query, ret, "point from point") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, pointSource, "id", "id");
    if (ExpectQueryStep(session, query, ret, "point select id") != 0)
    {
        return 1;
    }
    ret = HdbQuerySelect(query, pointSource, "name", "pointName");
    if (ExpectQueryStep(session, query, ret, "point select name") != 0)
    {
        return 1;
    }
    ret = HdbQueryWhereStringLike(query, pointSource, "name", "point%");
    if (ExpectQueryStep(session, query, ret, "point like name") != 0)
    {
        return 1;
    }
    ret = HdbQueryOrderBy(query, pointSource, "name", HDB_ORDER_ASC);
    if (ExpectQueryStep(session, query, ret, "point order name") != 0)
    {
        return 1;
    }
    ret = HdbQueryLimit(query, 3, 0);
    if (ExpectQueryStep(session, query, ret, "point limit") != 0)
    {
        return 1;
    }
    ret = HdbQueryExecute(query, &result);
    HdbQueryFree(query);
    if (ExpectHdbOk(session, ret, "execute point query") != 0)
    {
        return 1;
    }
    if (ExpectNext(session, result, 1, "point row 1") != 0 ||
        ExpectInt64(session, result, "id", 100) != 0 ||
        ExpectString(session, result, "pointName", "point A") != 0 ||
        ExpectNext(session, result, 1, "point row 2") != 0 ||
        ExpectInt64(session, result, "id", 101) != 0 ||
        ExpectString(session, result, "pointName", "point B") != 0 ||
        ExpectNext(session, result, 0, "point row end") != 0)
    {
        HdbResultFree(result);
        return 1;
    }
    HdbResultFree(result);
    PrintOk("固定表查询通过");
    return 0;
}

// 测试 SERVER 返回的查询错误
static int RunServerErrorSmoke(HDB_SESSION session)
{
    HDB_QUERY query;
    HDB_SOURCE source;
    HDB_RESULT result;
    int ret;

    PrintTestTitle("错误响应", "覆盖 SERVER 返回的查询错误和 DLL 错误映射");
    query = NULL;
    source = NULL;
    result = NULL;
    ret = HdbQueryCreate(session, &query);
    if (ExpectHdbOk(session, ret, "create missing time query") != 0)
    {
        return 1;
    }
    HdbQueryFrom(query, "alarm", &source);
    HdbQuerySelect(query, source, "level", "level");
    ret = HdbQueryExecute(query, &result);
    if (ExpectHdbError(session, ret, HDB_ERR_QUERY_NEED_TIME_RANGE, "missing time range") != 0)
    {
        HdbQueryFree(query);
        return 1;
    }
    HdbQueryFree(query);

    query = NULL;
    source = NULL;
    ret = HdbQueryCreate(session, &query);
    if (ExpectHdbOk(session, ret, "create missing dataset query") != 0)
    {
        return 1;
    }
    HdbQueryFrom(query, "missing_dataset", &source);
    HdbQuerySelect(query, source, "id", "id");
    ret = HdbQueryExecute(query, &result);
    if (ExpectHdbError(session, ret, HDB_ERR_DATASET_NOT_FOUND, "missing dataset") != 0)
    {
        HdbQueryFree(query);
        return 1;
    }
    HdbQueryFree(query);

    query = NULL;
    source = NULL;
    ret = HdbQueryCreate(session, &query);
    if (ExpectHdbOk(session, ret, "create type mismatch query") != 0)
    {
        return 1;
    }
    HdbQueryFrom(query, "alarm", &source);
    HdbQueryTimeRange(query, MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 13, 0, 0, 0, 0));
    HdbQuerySelect(query, source, "id", "id");
    HdbQueryWhereStringEq(query, source, "level", "2");
    ret = HdbQueryExecute(query, &result);
    if (ExpectHdbError(session, ret, HDB_ERR_TYPE_MISMATCH, "where type mismatch") != 0)
    {
        HdbQueryFree(query);
        return 1;
    }
    HdbQueryFree(query);
    PrintOk("错误响应检查通过");
    return 0;
}

int main(int argc, char* argv[])
{
    const char* connInfo;
    HDB_SESSION session;
    int exitCode;
    int ret;

    connInfo = argc > 1 && argv[1] != NULL && argv[1][0] != '\0' ? argv[1] : ReadDefaultConnInfo();
    session = NULL;
    exitCode = 1;

    printf("ysd_hdb smoke test\n");
    printf("conninfo: %s\n", connInfo);

    // 打开 DLL 会话并检查 SERVER 连接
    ret = HdbOpen(NULL, &session);
    if (ret != HDB_OK)
    {
        PrintFail("HdbOpen failed");
        printf("ret=%d\n", ret);
        goto done;
    }
    if (RunPingSmoke(session) != 0)
    {
        goto done;
    }

    // 准备用于查询的固定数据
    ret = PrepareHistoryFixture(connInfo);
    if (ret != 0)
    {
        exitCode = ret;
        goto done;
    }

    // 走 DLL 查询链路
    if (RunHistoryQuerySmoke(session) != 0 ||
        RunDslQuerySmoke(session) != 0 ||
        RunDslDmlSmoke(session) != 0 ||
        RunTimestampWhereSmoke(session) != 0 ||
        RunPointQuerySmoke(session) != 0)
    {
        goto done;
    }

    // 阶段四 检查 SERVER 错误响应
    if (RunServerErrorSmoke(session) != 0)
    {
        goto done;
    }

    exitCode = 0;

done:
    if (session != NULL)
    {
        HdbClose(session);
    }
    if (exitCode == 0)
    {
        PrintOk("ysd_hdb smoke test ok");
    }
    else
    {
        PrintFail("ysd_hdb smoke test failed");
    }
    PauseBeforeExit(exitCode);
    return exitCode;
}
