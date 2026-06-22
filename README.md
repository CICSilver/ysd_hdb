# ysd_hdb 历史库模块设计说明

## 模块定位

`ysd_hdb` 是历史数据访问模块，按 DLL + SERVER 两侧组织。

DLL 侧负责对外提供稳定的调用入口，调用方只依赖 DLL 导出的 C/C++ 接口，不直接连接数据库，不直接拼接 SQL。

SERVER 侧负责数据库连接、SQL 执行、事务控制、Model 元数据解析和数据读写。数据库实现封装在 SERVER 内部，DLL 侧通过接口或 IPC 调用 SERVER，不暴露具体数据库驱动。

当前工程结构：

```text
ysd_hdb/
  ysd_hdb.slnx
  common/
    HdbCommon.h
    HdbIpcProtocol.h
    HdbIpcProtocol.cpp
    HdbIpcSocket.h
    HdbIpcSocket.cpp
    HdbIpcResultCodec.h
    HdbIpcResultCodec.cpp
    HdbQueryAst.h
    HdbQueryAst.cpp
    HdbQueryAstCodec.h
    HdbQueryAstCodec.cpp
  dll/
    ysd_hdb_dll.vcxproj
    ysd_hdb.h
    dllmain.cpp
    ysd_hdb_api.cpp
  svr/
    ysd_hdb_svr.vcxproj
    ysd_hdb_svr.cpp
    HdbCommon.h
    HdbDbAdapter.h
    HdbDbAdapter.cpp
    HdbPgAdapter.h
    HdbPgAdapter.cpp
    HdbModelDef.h
    HdbModelCrud.h
    HdbModelCrud.cpp
    HdbDatasetRegistry.h
    HdbDatasetRegistry.cpp
    HdbFieldPathResolver.h
    HdbFieldPathResolver.cpp
    HdbShardRouter.h
    HdbShardRouter.cpp
    HdbQuerySqlBuilder.h
    HdbQuerySqlBuilder.cpp
    HdbQueryExecutor.h
    HdbQueryExecutor.cpp
    HdbIpcCommandHandler.h
    HdbIpcCommandHandler.cpp
    HdbIpcServerContext.h
    HdbIpcServerContext.cpp
    include/PostgreSQL/
  test/
    HdbSvrSelfTest.h
    HdbSvrSelfTest.cpp
  ysd_hdb_smoke_test/
    ysd_hdb_smoke_test.vcxproj
    ysd_hdb_smoke_test.cpp
```

## 分层结构

### DLL 侧

`ysd_hdb_dll` 是对外接口层。

职责：

- 导出历史库调用接口。
- 保持调用方 ABI 稳定。
- 将请求转发到 SERVER。
- 管理调用参数的边界检查和返回码转换。
- 不持有数据库连接。
- 不包含数据库驱动头文件和数据库库文件依赖。

DLL 导出接口使用 C 风格或稳定 POD 结构体，避免 STL 类型、异常、模板类型穿过 DLL 边界。

当前 `HdbOpen` 只创建 DLL 侧 IPC session，真实数据库连接由 SERVER 启动时建立。`HdbOpenByConnInfo` 保留为兼容预留接口，当前不作为正式调用链路。

### SERVER 侧

`ysd_hdb_svr` 是历史库服务进程。

职责：

- 管理数据库连接。
- 管理事务。
- 执行参数化 SQL。
- 根据 Model 元数据生成通用 CRUD SQL。
- 维护数据库错误信息和模块错误码。
- 承接 IPC 请求处理。

SERVER 内部主要分为：

```text
入口层
  ysd_hdb_svr.cpp

IPC 服务层
  CHdbIpcTcpServer / CHdbIpcTcpConnection
  CHdbIpcCommandHandler

逻辑查询层
  CHdbQueryAst / CHdbQueryAstCodec
  CHdbQueryExecutor
  CHdbQuerySqlBuilder

元数据和路由层
  CHdbDatasetRegistry
  CHdbFieldPathResolver
  CHdbShardRouter

Model CRUD 层
  CHdbModelCrud
  HdbModelDef / HdbFieldDef / HdbDatasetDef

数据库适配层
  CHdbDbAdapter
  CHdbPgAdapter
```

## IPC 协议层

`HdbIpcProtocol` 是 DLL 和 SERVER 共用的 IPC 协议定义。

协议层负责内存帧格式、命令号、请求/响应标记、状态码、payload 长度、checksum 校验和 TLV 字段编解码。协议层不负责 socket、命名管道、线程和连接生命周期。

帧结构：

```text
HdbIpcFrameHeader
  magic
  version
  headerSize
  command
  flags
  sequence
  status
  bodyLength
  bodyChecksum

body
  HdbIpcFieldHeader + field data
  HdbIpcFieldHeader + field data
  ...
```

帧头字段：

- `magic`：协议标识。
- `version`：协议版本。
- `headerSize`：帧头长度。
- `command`：命令号。
- `flags`：请求、响应、错误等标记。
- `sequence`：请求序号，用于匹配响应。
- `status`：响应状态码。
- `bodyLength`：payload 长度。
- `bodyChecksum`：payload 校验。

当前 SERVER 已分发处理的命令：

- `HDB_IPC_CMD_PING`：IPC 层连通性探测，不访问数据库。
- `HDB_IPC_CMD_DB_PING`：通过 SERVER 侧 adapter 探测数据库连接。
- `HDB_IPC_CMD_QUERY_EXECUTE`：执行逻辑查询描述，DLL 只发送 Query AST，不发送 SQL。

当前协议已定义但尚未作为正式链路处理的命令：

- `HDB_IPC_CMD_DB_OPEN`
- `HDB_IPC_CMD_DB_CLOSE`
- `HDB_IPC_CMD_MODEL_INSERT`
- `HDB_IPC_CMD_MODEL_UPDATE`
- `HDB_IPC_CMD_MODEL_DELETE`
- `HDB_IPC_CMD_MODEL_SELECT_BY_PK`
- `HDB_IPC_CMD_MODEL_SELECT_LIST`
- `HDB_IPC_CMD_DATASET_INSERT`
- `HDB_IPC_CMD_DATASET_BATCH_INSERT`
- `HDB_IPC_CMD_RESULT_FETCH`
- `HDB_IPC_CMD_RESULT_CLOSE`

当前字段类型：

- `HDB_IPC_FIELD_CONN_INFO`
- `HDB_IPC_FIELD_MODEL_NAME`
- `HDB_IPC_FIELD_MODEL_DATA`
- `HDB_IPC_FIELD_KEY_DATA`
- `HDB_IPC_FIELD_RESULT_DATA`
- `HDB_IPC_FIELD_ERROR_TEXT`
- `HDB_IPC_FIELD_FOUND`
- `HDB_IPC_FIELD_AFFECTED_ROWS`
- `HDB_IPC_FIELD_LIMIT`
- `HDB_IPC_FIELD_OFFSET`
- `HDB_IPC_FIELD_DATASET_NAME`
- `HDB_IPC_FIELD_QUERY_AST`
- `HDB_IPC_FIELD_RESULT_SCHEMA`
- `HDB_IPC_FIELD_RESULT_ROWS`
- `HDB_IPC_FIELD_CURSOR_ID`
- `HDB_IPC_FIELD_HAS_MORE`
- `HDB_IPC_FIELD_PAGE_SIZE`

DLL 侧按命令组装请求帧，SERVER 侧解析请求帧、调用内部数据库封装，再组装响应帧返回。请求和响应使用相同 `sequence`。

`HDB_IPC_FIELD_CONN_INFO` 当前只用于协议自检和预留 `DB_OPEN` 语义，不参与正式 DLL 查询链路。

## DLL 到 SERVER 查询链路

当前完整查询链路已经走通：

```text
调用方
  -> ysd_hdb_dll 导出 API
  -> DLL 侧 HDB_SESSION / HDB_QUERY
  -> CHdbQueryAst 组装逻辑查询
  -> CHdbQueryAstCodec 序列化为文本
  -> HDB_IPC_CMD_QUERY_EXECUTE
  -> SERVER 侧 CHdbIpcCommandHandler
  -> CHdbQueryExecutor
  -> CHdbQuerySqlBuilder 生成参数化 SQL
  -> CHdbDbAdapter / CHdbPgAdapter
  -> HdbIpcResultCodec 编码 schema 和 rows
  -> DLL 侧 HDB_RESULT 读取结果
```

查询描述只包含逻辑数据集、字段路径、where 条件、排序、分页和时间范围。DLL 不拼 SQL，SERVER 也不信任 DLL 侧传入的数据库标识符，表名和列名只能来自 SERVER 元数据。

## 数据库适配层

`CHdbDbAdapter` 是数据库访问抽象接口，SERVER 内部代码只依赖该接口。

当前接口：

```cpp
int Open(const char* connInfo);
int Close();
int Ping();
int Begin();
int Commit();
int Rollback();
const char* GetLastError() const;

int ExecCommand(const char* sql, int* affectedRows);
int ExecParams(const char* sql, int paramCount, const char* const* paramValues, int* affectedRows);
int QueryParams(const char* sql, int paramCount, const char* const* paramValues, CHdbQueryResult& result);
```

`CHdbPgAdapter` 是 PostgreSQL/libpq 实现。当前使用 `PQconnectdb`、`PQexec`、`PQexecParams` 完成连接、命令执行、参数化执行和查询结果读取。

后续切换到金仓时，SERVER 侧新增适配器实现。同一套 Model CRUD 继续依赖 `CHdbDbAdapter`，上层接口不直接改成数据库私有 API。

## 连接策略

数据库连接由 SERVER 进程管理。服务模式下 SERVER 按以下顺序读取连接串：

1. 环境变量 `HDB_PG_CONNINFO`。
2. 默认本机 PostgreSQL 连接串。

服务模式下，SERVER 启动后先打开数据库连接并执行 `Ping`，成功后才开始监听 IPC。DLL 侧 `HdbOpen` 只读取 IPC 地址和端口并创建 session 状态，不直接连接数据库。

`--selftest` 模式允许在命令行第二个参数传入连接串；未传入时同样使用 `HDB_PG_CONNINFO` 和默认连接串。

`HdbOpenByConnInfo` 当前返回 `HDB_ERR_NOT_IMPLEMENTED`，用于保留 ABI 位置。后续如果需要由调用方动态选择数据库，应先设计 SERVER 侧连接上下文和生命周期，再启用 `DB_OPEN` / `DB_CLOSE`。

## Model 元数据

Model 使用 C++03 POD 结构体定义。字段顺序由结构体控制，数据库列信息由静态元数据控制。

示例：

```cpp
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
```

`HdbModelDef` 描述表级信息：

- `tableName`：数据库表名。
- `modelSize`：结构体大小。
- `fields`：字段定义数组。
- `fieldCount`：字段数量。

`HdbDatasetDef` 描述逻辑数据集：

- `datasetName`：DLL 调用方看到的逻辑数据集名。
- `modelSize`：数据集 row 结构体大小。
- `fields`：字段定义数组。
- `fieldCount`：字段数量。
- `shard`：物理表路由规则。

`HdbFieldDef` 描述字段级信息：

- `columnName`：数据库列名。
- `type`：字段类型。
- `offset`：字段在结构体内的偏移。
- `size`：字符数组长度。
- `flags`：字段标记。

当前字段类型：

- `HDB_FT_INT32`
- `HDB_FT_INT64`
- `HDB_FT_DOUBLE`
- `HDB_FT_SMALLINT`
- `HDB_FT_CHAR_ARRAY`
- `HDB_FT_TIMESTAMP_MS`

当前字段标记：

- `HDB_FIELD_PK`：主键字段，用于 `WHERE` 条件。
- `HDB_FIELD_INSERT`：参与 `INSERT`。
- `HDB_FIELD_UPDATE`：参与 `UPDATE`。
- `HDB_FIELD_READONLY`：只读字段，不参与 `UPDATE`。

字段元数据显式记录列名、类型、偏移、长度和字段标记。结构体只解决内存布局问题，元数据解决数据库映射、SQL 生成和字段权限问题。

当前 `CHdbDatasetRegistry` 仍使用手写 schema 注册 `alarm`、`point`、`device` 以及 relation。下一步应把正式业务 Model / Dataset 定义收敛到稳定定义文件中，查询和写入共用同一套元数据。

## CRUD 生成规则

`CHdbModelCrud` 根据 `HdbModelDef` 生成 SQL。

当前接口：

```cpp
int InsertModel(const HdbModelDef& def, const void* model);
int UpdateModel(const HdbModelDef& def, const void* model);
int DeleteModel(const HdbModelDef& def, const void* model);
int SelectModelByPk(const HdbModelDef& def, const void* keyModel, void* outModel, int* found);
int SelectModelList(const HdbModelDef& def, HdbModelRowCallback cb, void* userData);
```

当前已实现：

- `InsertModel`：插入带 `HDB_FIELD_INSERT` 标记的字段。
- `UpdateModel`：按主键更新带 `HDB_FIELD_UPDATE` 标记且非只读的字段。
- `DeleteModel`：按主键删除。
- `SelectModelByPk`：按主键读取单条记录。
- `CHdbQueryExecutor`：执行逻辑查询并返回 schema + rows。
- `CHdbQuerySqlBuilder`：按 dataset、field path、relation、分片规则生成参数化 SELECT。

`SelectModelList` 保留接口位置，后续承接条件查询、范围查询、分页和回调读取。

SQL 生成规则：

- 表名和字段名只来自静态元数据。
- 表名和字段名执行合法性校验。
- 字段值全部使用参数化 SQL。
- `INSERT`、`UPDATE`、`DELETE` 校验影响行数。
- `SELECT` 查不到记录时通过 `found = 0` 返回。
- 逻辑查询的 `SELECT` 输出名来自 DLL 指定的 `outputName`。
- `HDB_FT_TIMESTAMP_MS` 查询结果在 SERVER 侧转换为 epoch milliseconds 文本，再由 DLL 按 `HdbInt64` 读取。

DLL 对外写入接口 `HdbInsertRow` 和 `HdbBatchInsertRows` 当前仍是预留接口，下一步应通过 `DATASET_INSERT` / `DATASET_BATCH_INSERT` 接入 SERVER 侧 dataset/model 元数据和 CRUD 写入能力。

## 错误码

公共错误码定义在 `HdbCommon.h`。

```cpp
HDB_OK = 0
HDB_ERR_PARAM = -1
HDB_ERR_NOT_CONNECTED = -2
HDB_ERR_DB_CONNECT = -3
HDB_ERR_DB_EXEC = -4
HDB_ERR_NO_RECORD = -5
HDB_ERR_MODEL_DEF = -6
HDB_ERR_BUFFER = -7
HDB_ERR_DATASET_DEF = -8
HDB_ERR_DATASET_NOT_FOUND = -9
HDB_ERR_FIELD_NOT_FOUND = -10
HDB_ERR_FIELD_PATH = -11
HDB_ERR_RELATION_NOT_FOUND = -12
HDB_ERR_QUERY_NEED_TIME_RANGE = -13
HDB_ERR_QUERY_RANGE = -14
HDB_ERR_SHARD_DEF = -15
HDB_ERR_SHARD_NOT_FOUND = -16
HDB_ERR_NOT_IMPLEMENTED = -17
HDB_ERR_NULL_VALUE = -18
HDB_ERR_TYPE_MISMATCH = -19
HDB_ERR_INTERNAL = -20
```

返回码用于程序判断，`GetLastError()` 用于输出详细错误文本。

## 编译兼容

源码按 C++03 风格编写。

约束：

- 不使用 `auto`。
- 不使用 lambda。
- 不使用 `nullptr`。
- 不使用范围 `for`。
- 不使用智能指针。
- 对外结构体使用 POD。
- DLL 边界不传 STL 对象。

当前 PG 依赖位于：

```text
ysd_hdb/svr/include/PostgreSQL/
```

当前已验证 Win32 Debug 编译通过。x64 配置需要匹配 x64 版本 `libpq.lib` 和运行时 DLL。

## 运行自检

当前 `ysd_hdb_svr.cpp` 提供 SERVER 侧自检入口和服务入口。

连接串由 SERVER 侧读取。服务模式使用 `HDB_PG_CONNINFO` 或默认连接串，`--selftest` 模式可通过命令行第二个参数覆盖连接串。

`--selftest-unit` 执行不依赖活动数据库连接的单元自检，覆盖 IPC 协议、AST codec、result codec、分片路由、字段路径、SQL builder、query executor 和 IPC handler。

`--selftest` 执行数据库自检，流程为：

1. 先执行 `--selftest-unit` 覆盖的单元自检。
2. 打开数据库连接。
3. 执行 `Ping`。
4. 创建测试表 `hdb_model_crud_test`。
5. 执行 `InsertModel`。
6. 执行 `SelectModelByPk`。
7. 执行 `UpdateModel`。
8. 执行 `DeleteModel`。
9. 删除后再次查询，确认 `found = 0`。
10. 执行固定测试数据的历史查询、时间条件查询和结果转换检查。

测试表由自检入口创建和重建，仅用于验证 PG 封装与 Model CRUD 框架。

全流程 smoke test 位于 `ysd_hdb_smoke_test` 工程，用于验证调用方通过 DLL 访问 SERVER 的实际路径。运行前需要先启动 SERVER，smoke test 会先执行 `HdbPing` 确认 SERVER 可达，再准备固定测试表数据，并覆盖日分片查询、两级 relation、时间 where、固定表查询、SERVER 错误响应和 DLL 结果读取。

自动化运行 smoke test 时可设置：

```powershell
$env:HDB_SMOKE_NOPAUSE='1'
```

## 本地 PG 测试库

当前本机测试库使用 scoop 安装的 PostgreSQL，数据库数据目录放在 CA8150 项目外。

路径：

```text
PG 程序目录：%USERPROFILE%/scoop/apps/postgresql/current/bin
PG 数据目录：E:/WorkFiles/workSpace/pgdata/ysd_hdb_pg
PG 日志目录：E:/WorkFiles/workSpace/pgdata/logs
```

连接信息：

```text
host=127.0.0.1 port=5432 dbname=postgres user=postgres password=postgres
```

常用脚本：

```powershell
ysd_hdb/tools/start_pg_test.ps1
ysd_hdb/tools/stop_pg_test.ps1
ysd_hdb/tools/run_pg_selftest.ps1
```

测试库监听 `127.0.0.1:5432`，认证方式使用 `md5`，用于兼容当前工程内的旧版 `libpq.dll`。

## 扩展顺序

当前已经完成 SERVER IPC 入口、DLL 查询转发、逻辑查询 AST、参数化查询生成、结果 schema/rows 编解码和全流程 smoke test。后续建议按以下顺序扩展：

1. 增加正式 Dataset / Model 定义文件，替换当前手写测试 schema 的临时形态。
2. 实现 `HdbInsertRow`，通过 `HDB_IPC_CMD_DATASET_INSERT` 走 DLL -> SERVER -> Model CRUD 写入链路。
3. 实现 `HdbBatchInsertRows`，通过 `HDB_IPC_CMD_DATASET_BATCH_INSERT` 支持批量写入。
4. 实现 `HDB_IPC_CMD_RESULT_FETCH` / `HDB_IPC_CMD_RESULT_CLOSE`，支持大结果集分页读取和 SERVER 侧结果资源释放。
5. 完善连接管理策略，包括 SERVER 工作线程、独立连接或连接池、超时和断线处理。
6. 如需支持 x64，补齐匹配 x64 的 `libpq.lib` 和运行时 DLL。
7. 增加金仓适配器或复用 PG 协议适配路径。

## 性能边界

第一阶段以正确性和接口稳定为主，当前使用单条 CRUD 和显式事务。

后续性能实现点：

- 写入路径使用批量接口。
- 高频查询使用范围条件和索引字段。
- SERVER 工作线程使用独立连接或连接池。
- 参数化 SQL 保持复用路径。
- 大结果集使用分页或回调分批读取。
- IPC 请求结构保持定长头和明确 payload，减少跨进程拷贝次数。

## 命名规则

模块名使用 `ysd_hdb`。

工程名：

- `ysd_hdb_dll`
- `ysd_hdb_svr`

类型命名：

- 类名使用 `CHdb` 前缀。
- 结构体、枚举、类型别名使用 `Hdb` 前缀。
- 宏和错误码使用 `HDB_` 前缀。
- 文件名使用 `Hdb` 前缀表达模块归属。

数据库表名和列名在 Model 元数据中显式声明，SERVER 代码不从结构体成员名自动推导数据库标识符。
