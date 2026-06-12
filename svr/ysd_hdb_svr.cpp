#include "HdbModelCrud.h"
#include "HdbPgAdapter.h"
#include "HdbDatasetRegistry.h"
#include "HdbFieldPathResolver.h"
#include "HdbQueryExecutor.h"
#include "HdbQuerySqlBuilder.h"
#include "HdbShardRouter.h"
#include "../common/HdbIpcProtocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>

struct HdbTestModel
{
    HdbInt64 id;
    int type;
    char name[128];
    HdbInt64 create_time;
};

static HdbFieldDef g_hdbTestFields[] =
{
    HDB_FIELD_INT64_PK(HdbTestModel, id, "id"),
    HDB_FIELD_INT32(HdbTestModel, type, "type"),
    HDB_FIELD_CHAR(HdbTestModel, name, "name", 128),
    HDB_FIELD_TIMESTAMP_MS(HdbTestModel, create_time, "create_time")
};

static HdbModelDef g_hdbTestModelDef =
{
    "hdb_model_crud_test",
    sizeof(HdbTestModel),
    g_hdbTestFields,
    (int)HDB_ARRAY_COUNT(g_hdbTestFields)
};

static void SetText(char* buffer, int bufferSize, const char* text)
{
    if (buffer == NULL || bufferSize <= 0)
    {
        return;
    }
    memset(buffer, 0, bufferSize);
    if (text != NULL)
    {
        strncpy(buffer, text, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    }
}

static HdbInt64 MakeLocalTimeMs(int year, int month, int day, int hour, int minute, int second, int millis)
{
    struct tm tmValue;
    time_t seconds;

    memset(&tmValue, 0, sizeof(tmValue));
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = minute;
    tmValue.tm_sec = second;
    tmValue.tm_isdst = -1;
    seconds = mktime(&tmValue);
    return ((HdbInt64)seconds) * 1000 + millis;
}

static const char* ReadConnInfo(int argc, char* argv[])
{
    const char* envConn;
    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0')
    {
        return argv[1];
    }

    envConn = getenv("HDB_PG_CONNINFO");
    if (envConn != NULL && envConn[0] != '\0')
    {
        return envConn;
    }

    return "host=127.0.0.1 port=5432 dbname=postgres user=postgres password=postgres";
}

static int CreateTestTable(CHdbPgAdapter& adapter)
{
    int ret;
    ret = adapter.ExecCommand("drop table if exists hdb_model_crud_test", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }

    return adapter.ExecCommand(
        "create table hdb_model_crud_test ("
        "id bigint not null primary key,"
        "type integer not null,"
        "name varchar(128) not null,"
        "create_time timestamp not null"
        ")",
        NULL);
}

static int RunCrudSelfTest(CHdbPgAdapter& adapter)
{
    CHdbModelCrud crud(&adapter);
    HdbTestModel model;
    HdbTestModel key;
    HdbTestModel out;
    int found;
    int ret;

    memset(&model, 0, sizeof(model));
    model.id = 1001;
    model.type = 7;
    SetText(model.name, sizeof(model.name), "O'Reilly alarm test");
    model.create_time = 1717470000789LL;

    ret = adapter.Begin();
    if (ret != HDB_OK)
    {
        printf("begin failed: %s\n", adapter.GetLastError());
        return ret;
    }

    ret = crud.InsertModel(g_hdbTestModelDef, &model);
    if (ret != HDB_OK)
    {
        printf("insert failed: %s\n", crud.GetLastError());
        adapter.Rollback();
        return ret;
    }

    ret = adapter.Commit();
    if (ret != HDB_OK)
    {
        printf("commit failed: %s\n", adapter.GetLastError());
        return ret;
    }
    printf("insert ok\n");

    memset(&key, 0, sizeof(key));
    key.id = model.id;
    memset(&out, 0, sizeof(out));
    found = 0;
    ret = crud.SelectModelByPk(g_hdbTestModelDef, &key, &out, &found);
    if (ret != HDB_OK || found == 0)
    {
        printf("select after insert failed: ret=%d found=%d error=%s\n", ret, found, crud.GetLastError());
        return ret == HDB_OK ? HDB_ERR_NO_RECORD : ret;
    }
    printf("select ok: id=" HDB_INT64_FORMAT " type=%d name=%s create_time=" HDB_INT64_FORMAT "\n",
        out.id, out.type, out.name, out.create_time);

    model.type = 9;
    SetText(model.name, sizeof(model.name), "updated name with quote ' ok");
    ret = crud.UpdateModel(g_hdbTestModelDef, &model);
    if (ret != HDB_OK)
    {
        printf("update failed: %s\n", crud.GetLastError());
        return ret;
    }
    printf("update ok\n");

    memset(&out, 0, sizeof(out));
    found = 0;
    ret = crud.SelectModelByPk(g_hdbTestModelDef, &key, &out, &found);
    if (ret != HDB_OK || found == 0 || out.type != 9)
    {
        printf("select after update failed: ret=%d found=%d type=%d error=%s\n",
            ret, found, out.type, crud.GetLastError());
        return ret == HDB_OK ? HDB_ERR_DB_EXEC : ret;
    }
    printf("select updated ok: id=" HDB_INT64_FORMAT " type=%d name=%s\n", out.id, out.type, out.name);

    ret = crud.DeleteModel(g_hdbTestModelDef, &key);
    if (ret != HDB_OK)
    {
        printf("delete failed: %s\n", crud.GetLastError());
        return ret;
    }
    printf("delete ok\n");

    memset(&out, 0, sizeof(out));
    found = 0;
    ret = crud.SelectModelByPk(g_hdbTestModelDef, &key, &out, &found);
    if (ret != HDB_OK)
    {
        printf("select after delete failed: %s\n", crud.GetLastError());
        return ret;
    }
    if (found != 0)
    {
        printf("select after delete expected no record\n");
        return HDB_ERR_DB_EXEC;
    }
    printf("select no record ok\n");

    return HDB_OK;
}

static int CreateHistoryQueryTables(CHdbPgAdapter& adapter)
{
    int ret;

    ret = adapter.ExecCommand("drop table if exists hdb_alarm_20260612", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand("drop table if exists hdb_alarm_20260613", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand("drop table if exists hdb_point", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand("drop table if exists hdb_device", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }

    ret = adapter.ExecCommand(
        "create table hdb_alarm_20260612 ("
        "id bigint not null primary key,"
        "point_id bigint,"
        "level integer not null,"
        "message varchar(128),"
        "occur_time timestamp not null"
        ")",
        NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand(
        "create table hdb_alarm_20260613 ("
        "id bigint not null primary key,"
        "point_id bigint,"
        "level integer not null,"
        "message varchar(128),"
        "occur_time timestamp not null"
        ")",
        NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand(
        "create table hdb_point ("
        "id bigint not null primary key,"
        "device_id bigint,"
        "name varchar(128) not null"
        ")",
        NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand(
        "create table hdb_device ("
        "id bigint not null primary key,"
        "name varchar(128) not null"
        ")",
        NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }

    ret = adapter.ExecCommand("insert into hdb_device(id, name) values (200, 'device A')", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand("insert into hdb_point(id, device_id, name) values (100, 200, 'point A')", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand("insert into hdb_point(id, device_id, name) values (101, 999, 'point B')", NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = adapter.ExecCommand(
        "insert into hdb_alarm_20260612(id, point_id, level, message, occur_time) "
        "values (1, 100, 3, 'alarm day one', '2026-06-12 10:00:00.000')",
        NULL);
    if (ret != HDB_OK)
    {
        return ret;
    }
    return adapter.ExecCommand(
        "insert into hdb_alarm_20260613(id, point_id, level, message, occur_time) "
        "values (2, 101, 4, 'alarm day two', '2026-06-13 11:00:00.000')",
        NULL);
}

static int RunQueryResultSelfTest()
{
    CHdbQueryResult result;
    std::vector<HdbQueryCell> row;
    HdbQueryCell emptyText;
    HdbQueryCell nullText;

    result.AddColumn("emptyText");
    result.AddColumn("nullText");
    emptyText.value = "";
    emptyText.isNull = 0;
    nullText.value = "";
    nullText.isNull = 1;
    row.push_back(emptyText);
    row.push_back(nullText);
    result.AddRow(row);

    if (result.IsNull(0, 0) != 0 || result.IsNull(0, 1) != 1)
    {
        printf("query result null self-test failed\n");
        return HDB_ERR_DB_EXEC;
    }
    if (result.FindColumn("nullText") != 1)
    {
        printf("query result column lookup self-test failed\n");
        return HDB_ERR_DB_EXEC;
    }
    printf("query result null self-test ok\n");
    return HDB_OK;
}

static int RunShardRouterSelfTest()
{
    CHdbDatasetRegistry registry;
    CHdbShardRouter router;
    const HdbDatasetDef* alarmDataset;
    const HdbDatasetDef* pointDataset;
    std::string tableName;
    std::vector<std::string> tables;
    HdbInt64 beginMs;
    HdbInt64 endMs;
    int ret;

    alarmDataset = registry.FindDataset("alarm");
    pointDataset = registry.FindDataset("point");
    if (alarmDataset == NULL || pointDataset == NULL)
    {
        printf("shard router dataset self-test failed\n");
        return HDB_ERR_DATASET_NOT_FOUND;
    }
    beginMs = MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0);
    endMs = MakeLocalTimeMs(2026, 6, 14, 0, 0, 0, 0);

    ret = router.BuildDayTableName(*alarmDataset, beginMs, tableName);
    if (ret != HDB_OK || tableName != "hdb_alarm_20260612")
    {
        printf("day table name self-test failed: %s\n", tableName.c_str());
        return ret == HDB_OK ? HDB_ERR_SHARD_DEF : ret;
    }
    ret = router.ResolveQueryTables(*alarmDataset, beginMs, endMs, tables);
    if (ret != HDB_OK || tables.size() != 2 ||
        tables[0] != "hdb_alarm_20260612" ||
        tables[1] != "hdb_alarm_20260613")
    {
        printf("day query tables self-test failed\n");
        return ret == HDB_OK ? HDB_ERR_SHARD_DEF : ret;
    }
    ret = router.ResolveQueryTables(*pointDataset, beginMs, endMs, tables);
    if (ret != HDB_OK || tables.size() != 1 || tables[0] != "hdb_point")
    {
        printf("none shard query tables self-test failed\n");
        return ret == HDB_OK ? HDB_ERR_SHARD_DEF : ret;
    }
    ret = router.ResolveQueryTables(*alarmDataset, endMs, beginMs, tables);
    if (ret != HDB_ERR_QUERY_RANGE)
    {
        printf("invalid shard range self-test failed: %d\n", ret);
        return HDB_ERR_QUERY_RANGE;
    }
    printf("shard router self-test ok\n");
    return HDB_OK;
}

static int RunFieldPathSelfTest()
{
    CHdbDatasetRegistry registry;
    CHdbFieldPathResolver resolver(&registry);
    const HdbDatasetDef* alarmDataset;
    HdbResolvedFieldPath path;
    int ret;

    alarmDataset = registry.FindDataset("alarm");
    if (alarmDataset == NULL)
    {
        return HDB_ERR_DATASET_NOT_FOUND;
    }
    ret = resolver.Resolve(*alarmDataset, "level", path);
    if (ret != HDB_OK || path.relations.size() != 0 || strcmp(path.field->fieldName, "level") != 0)
    {
        printf("root field path self-test failed\n");
        return ret == HDB_OK ? HDB_ERR_FIELD_PATH : ret;
    }
    ret = resolver.Resolve(*alarmDataset, "point.name", path);
    if (ret != HDB_OK || path.relations.size() != 1 || strcmp(path.field->fieldName, "name") != 0)
    {
        printf("one relation field path self-test failed\n");
        return ret == HDB_OK ? HDB_ERR_FIELD_PATH : ret;
    }
    ret = resolver.Resolve(*alarmDataset, "point.device.name", path);
    if (ret != HDB_OK || path.relations.size() != 2 || strcmp(path.field->fieldName, "name") != 0)
    {
        printf("two relation field path self-test failed\n");
        return ret == HDB_OK ? HDB_ERR_FIELD_PATH : ret;
    }
    ret = resolver.Resolve(*alarmDataset, "point..name", path);
    if (ret != HDB_ERR_FIELD_PATH)
    {
        printf("invalid field path self-test failed: %d\n", ret);
        return HDB_ERR_FIELD_PATH;
    }
    printf("field path self-test ok\n");
    return HDB_OK;
}

static int RunQueryBuilderSelfTest()
{
    CHdbDatasetRegistry registry;
    CHdbQuerySqlBuilder builder(&registry);
    CHdbQueryAst ast;
    HdbBuiltQuery query;
    int ret;

    ast.SetRootDataset("alarm");
    ast.SetTimeRange(MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 14, 0, 0, 0, 0));
    ast.AddSelect("occur_time", "time");
    ast.AddSelect("point.device.name", "deviceName;drop table x");
    ast.AddWhereInt32("level", HDB_OP_GE, 2);
    ast.AddOrder("occur_time", HDB_ORDER_DESC);
    ast.SetLimit(10, 0);
    ret = builder.BuildSelect(ast, query);
    if (ret != HDB_OK)
    {
        printf("query builder self-test failed: %s\n", builder.GetLastError());
        return ret;
    }
    if (query.sql.find("left join hdb_point") == std::string::npos ||
        query.sql.find("left join hdb_device") == std::string::npos ||
        query.sql.find("deviceName;drop table x") != std::string::npos ||
        query.params.size() < 5)
    {
        printf("query builder generated sql self-test failed: %s\n", query.sql.c_str());
        return HDB_ERR_DB_EXEC;
    }

    ast.Clear();
    ast.SetRootDataset("alarm");
    ast.AddSelect("level", "level");
    ret = builder.BuildSelect(ast, query);
    if (ret != HDB_ERR_QUERY_NEED_TIME_RANGE)
    {
        printf("query need time range self-test failed: %d\n", ret);
        return HDB_ERR_QUERY_NEED_TIME_RANGE;
    }

    ast.Clear();
    ast.SetRootDataset("alarm;drop");
    ast.SetTimeRange(MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 13, 0, 0, 0, 0));
    ast.AddSelect("level", "level");
    ret = builder.BuildSelect(ast, query);
    if (ret == HDB_OK)
    {
        printf("dangerous dataset self-test failed\n");
        return HDB_ERR_DATASET_NOT_FOUND;
    }
    printf("query builder self-test ok\n");
    return HDB_OK;
}

static int RunServerQueryUnitSelfTests()
{
    int ret;

    ret = RunQueryResultSelfTest();
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = RunShardRouterSelfTest();
    if (ret != HDB_OK)
    {
        return ret;
    }
    ret = RunFieldPathSelfTest();
    if (ret != HDB_OK)
    {
        return ret;
    }
    return RunQueryBuilderSelfTest();
}

static int RunHistoryQuerySelfTest(CHdbPgAdapter& adapter)
{
    CHdbDatasetRegistry registry;
    CHdbQueryExecutor executor(&adapter, &registry);
    CHdbQueryAst ast;
    CHdbQueryResult result;
    int ret;
    int deviceCol;

    ast.SetRootDataset("alarm");
    ast.SetTimeRange(MakeLocalTimeMs(2026, 6, 12, 0, 0, 0, 0), MakeLocalTimeMs(2026, 6, 14, 0, 0, 0, 0));
    ast.AddSelect("id", "id");
    ast.AddSelect("point.name", "pointName");
    ast.AddSelect("point.device.name", "deviceName");
    ast.AddWhereInt32("level", HDB_OP_GE, 2);
    ast.AddOrder("occur_time", HDB_ORDER_DESC);
    ast.SetLimit(10, 0);

    ret = executor.Execute(ast, result);
    if (ret != HDB_OK)
    {
        printf("history query execute failed: %s\n", executor.GetLastError());
        return ret;
    }
    if (result.RowCount() != 2)
    {
        printf("history query row count failed: %d\n", result.RowCount());
        return HDB_ERR_DB_EXEC;
    }
    deviceCol = result.FindColumn("deviceName");
    if (deviceCol < 0)
    {
        printf("history query output name mapping failed\n");
        return HDB_ERR_DB_EXEC;
    }
    if (result.IsNull(0, deviceCol) != 1 || result.IsNull(1, deviceCol) != 0)
    {
        printf("history query left join null failed: first=%d second=%d\n",
            result.IsNull(0, deviceCol),
            result.IsNull(1, deviceCol));
        return HDB_ERR_DB_EXEC;
    }
    printf("history query self-test ok: rows=%d firstPoint=%s\n",
        result.RowCount(),
        result.GetValue(0, result.FindColumn("pointName")));
    return HDB_OK;
}

static const unsigned char* GetVectorBuffer(const std::vector<unsigned char>& data)
{
    if (data.empty())
    {
        return NULL;
    }
    return &data[0];
}

static int RunIpcProtocolSelfTest()
{
    std::vector<unsigned char> body;
    std::vector<unsigned char> packet;
    HdbIpcFrame frame;
    CHdbIpcFieldReader reader;
    HdbIpcField field;
    std::string connInfo;
    int hasField;
    int affectedRows;
    int ret;

    ret = HdbIpcAppendString(body, HDB_IPC_FIELD_CONN_INFO, "host=127.0.0.1 port=5432 dbname=postgres");
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    ret = HdbIpcAppendInt32(body, HDB_IPC_FIELD_AFFECTED_ROWS, 1);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }

    ret = HdbIpcBuildRequest(HDB_IPC_CMD_DB_OPEN,
        1001,
        GetVectorBuffer(body),
        (unsigned int)body.size(),
        packet);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }

    ret = HdbIpcParseFrame(GetVectorBuffer(packet), (unsigned int)packet.size(), frame);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    if (frame.header.command != HDB_IPC_CMD_DB_OPEN ||
        frame.header.sequence != 1001 ||
        (frame.header.flags & HDB_IPC_FLAG_REQUEST) == 0)
    {
        return HDB_IPC_ERR_HEADER;
    }

    ret = reader.Reset(frame.body, frame.bodyLength);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }

    ret = reader.Next(field, &hasField);
    if (ret != HDB_IPC_OK || hasField == 0 || field.type != HDB_IPC_FIELD_CONN_INFO)
    {
        return ret == HDB_IPC_OK ? HDB_IPC_ERR_FIELD : ret;
    }
    ret = HdbIpcReadString(field, connInfo);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    if (connInfo != "host=127.0.0.1 port=5432 dbname=postgres")
    {
        return HDB_IPC_ERR_FIELD;
    }

    ret = reader.Next(field, &hasField);
    if (ret != HDB_IPC_OK || hasField == 0 || field.type != HDB_IPC_FIELD_AFFECTED_ROWS)
    {
        return ret == HDB_IPC_OK ? HDB_IPC_ERR_FIELD : ret;
    }
    ret = HdbIpcReadInt32(field, &affectedRows);
    if (ret != HDB_IPC_OK)
    {
        return ret;
    }
    if (affectedRows != 1)
    {
        return HDB_IPC_ERR_FIELD;
    }

    ret = reader.Next(field, &hasField);
    if (ret != HDB_IPC_OK || hasField != 0)
    {
        return ret == HDB_IPC_OK ? HDB_IPC_ERR_FIELD : ret;
    }

    printf("ipc protocol self-test ok\n");
    return HDB_OK;
}

int main(int argc, char* argv[])
{
    const char* connInfo;
    CHdbPgAdapter adapter;
    int ret;

    connInfo = ReadConnInfo(argc, argv);
    printf("ysd_hdb_svr pg crud self-test\n");
    printf("conninfo: %s\n", connInfo);

    ret = RunIpcProtocolSelfTest();
    if (ret != HDB_OK)
    {
        printf("ipc protocol self-test failed: %d\n", ret);
        return 1;
    }

    ret = RunServerQueryUnitSelfTests();
    if (ret != HDB_OK)
    {
        printf("server query unit self-test failed: %d\n", ret);
        return 1;
    }

    ret = adapter.Open(connInfo);
    if (ret != HDB_OK)
    {
        printf("open postgres failed: %s\n", adapter.GetLastError());
        return 1;
    }

    ret = adapter.Ping();
    if (ret != HDB_OK)
    {
        printf("ping postgres failed: %s\n", adapter.GetLastError());
        return 2;
    }
    printf("ping ok\n");

    ret = CreateTestTable(adapter);
    if (ret != HDB_OK)
    {
        printf("create test table failed: %s\n", adapter.GetLastError());
        return 3;
    }
    printf("test table ready\n");

    ret = RunCrudSelfTest(adapter);
    if (ret != HDB_OK)
    {
        printf("crud self-test failed: %d\n", ret);
        return 4;
    }

    ret = CreateHistoryQueryTables(adapter);
    if (ret != HDB_OK)
    {
        printf("create history query tables failed: %s\n", adapter.GetLastError());
        return 5;
    }
    ret = RunHistoryQuerySelfTest(adapter);
    if (ret != HDB_OK)
    {
        printf("history query self-test failed: %d\n", ret);
        return 6;
    }

    adapter.Close();
    printf("crud self-test ok\n");
    printf("query self-test ok\n");
    return 0;
}
