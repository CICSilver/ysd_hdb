# ysd_hdb 历史库模块设计说明

## 模块定位

`ysd_hdb` 按 DLL + SERVER 两侧组织。DLL 侧只提供对外 C ABI、请求组包和结果读取，不直接访问数据库。SERVER 侧管理数据库连接、元数据、分片路由、参数化 SQL 生成和执行。

调用链保持：

```text
调用方
  -> dll/ysd_hdb.h / CHdbQueryBuilder
  -> dll/ysd_hdb_c.h 低层 C ABI
  -> DLL 侧 HDB_SESSION / HDB_QUERY / HDB_SOURCE
  -> CHdbQueryAst version 2
  -> CHdbQueryAstCodec 文本序列化
  -> HDB_IPC_CMD_QUERY_EXECUTE
  -> SERVER 侧 CHdbIpcCommandHandler
  -> CHdbQueryExecutor
  -> CHdbQuerySqlBuilder
  -> CHdbDbAdapter / CHdbPgAdapter
  -> HdbIpcResultCodec
  -> DLL 侧 HDB_RESULT
```

DLL 不发送 SQL。SERVER 不信任 DLL 传入的数据库标识符，表名和列名只能来自 SERVER metadata。

## 工程结构

```text
ysd_hdb/
  common/
    HdbCommon.h
    HdbIpcProtocol.h/.cpp
    HdbIpcResultCodec.h/.cpp
    HdbIpcSocket.h/.cpp
    HdbQueryAst.h/.cpp
    HdbQueryAstCodec.h/.cpp
  dll/
    ysd_hdb.h
    ysd_hdb_c.h
    HdbQueryBuilder.h
    ysd_hdb_api.cpp
  svr/
    HdbDatasetRegistry.h/.cpp
    HdbDbAdapter.h/.cpp
    HdbPgAdapter.h/.cpp
    HdbModelDef.h
    HdbModelCrud.h/.cpp
    HdbQuerySqlBuilder.h/.cpp
    HdbQueryExecutor.h/.cpp
    HdbShardRouter.h/.cpp
    HdbIpcCommandHandler.h/.cpp
    HdbIpcServerContext.h/.cpp
    ysd_hdb_svr.cpp
  test/
    HdbSvrSelfTest.h/.cpp
  ysd_hdb_smoke_test/
    ysd_hdb_smoke_test.cpp
```

## 查询概念

Dataset：

调用方可作为 ROOT 使用的逻辑数据集，同时描述字段 metadata 和物理表路由。示例：`alarm`、`point`、`device`。

Association：

两个 Dataset 之间可供查询显式 JOIN 使用的命名关联。Association 只描述稳定数据关系，例如：

```cpp
{ "alarm", "point", "point", "point_id", "id" }
```

含义是 `alarm.point_id -> point.id`。Association 不自动触发 JOIN，不决定 JOIN 类型，也不是点号路径片段。

Source：

某一次查询中的具体表来源或 JOIN 实例。ROOT source 固定为 `sourceId = 0`，JOIN source 按创建顺序递增。每个 JOIN 调用都会创建新的 source；同一 Association 可以被显式 JOIN 多次，并生成不同 SQL alias。

Join：

某一次查询选择从哪个 source 通过哪个 Association 使用 `HDB_JOIN_LEFT` 或 `HDB_JOIN_INNER`。JOIN 类型属于具体查询，不属于 Association metadata。

Field：

某个 Source 所属 Dataset 上的字段。字段名只作为 metadata 查找键，不能包含点号导航。Select 不会自动触发 JOIN。

## 用户入口

普通调用方包含 `dll/ysd_hdb.h`，并使用 `CHdbQueryBuilder` 构造查询。`dll/ysd_hdb_c.h` 是低层 C ABI 头文件，用于 DLL 导出、跨语言绑定和底层边界测试；业务代码不需要直接调用低层查询函数。

`dll/HdbQueryBuilder.h` 是 header-only 包装层，只转发到底层 C ABI，不导出 C++ ABI，不维护独立查询模型，不抛异常。

```cpp
HDB_SOURCE alarm = NULL;
HDB_SOURCE point = NULL;
HDB_SOURCE device = NULL;
HDB_RESULT result = NULL;
CHdbQueryBuilder query(session);

query
    .From("alarm", alarm)
    .LeftJoin(alarm, "point", point)
    .LeftJoin(point, "device", device)
    .Select(alarm, "id", "id")
    .Select(point, "name", "pointName")
    .Select(device, "name", "deviceName")
    .WhereInt32(alarm, "level", HDB_OP_GE, 2)
    .OrderBy(alarm, "occur_time", HDB_ORDER_DESC)
    .TimeRange(beginMs, endMs)
    .Limit(10, 0);

int ret = query.Execute(&result);
```

调用代码能直接看出：

```text
alarm
  LEFT JOIN alarm.point -> point
  LEFT JOIN point.device -> device
```

Builder 记录第一次失败的返回码；第一次失败后后续链式方法不再修改 query。析构时释放仍持有的 `HDB_QUERY`。

## AST version 2

AST 只支持 version 2，Decoder 遇到旧版本会返回不支持。

文本格式示例：

```text
ast_version=2
source=root|0|alarm
source=join|1|0|point|2
source=join|2|1|device|2
time=1781193600000,1781366400000
select=0|id|id
select=1|name|pointName
select=2|name|deviceName
where=0|level|4|1|2
order=0|occur_time|2
limit=10,0
```

SERVER 解析后重新校验：

- 必须且只能有一个 ROOT source
- ROOT sourceId 必须为 0
- sourceId 不得重复，JOIN parent 必须已存在
- Association 必须属于 parent source 的 Dataset
- target Dataset 只能由 Association metadata 推导
- joinType 只能是 `HDB_JOIN_LEFT` 或 `HDB_JOIN_INNER`
- select / where / order 的 sourceId 必须存在
- fieldName 必须属于该 source 对应 Dataset
- where 值类型必须与字段类型匹配
- 表名和列名只来自 SERVER metadata
- 日分片 ROOT 必须提供时间范围
- 当前不支持日分片 Dataset 作为 JOIN target

## SQL Builder

`CHdbQuerySqlBuilder` 将 source 解析成稳定 SQL alias：

```text
s0, s1, s2, ...
```

每个 JOIN source 都生成一次 JOIN，不按 Association 自动合并：

```sql
left join hdb_point s1 on s0.point_id = s1.id
left join hdb_device s2 on s1.device_id = s2.id
```

字段表达式直接由 `sourceId + fieldName` 解析：

```text
sourceId 2, field name -> s2.name
```

日分片 ROOT 仍通过 `union all` 子查询实现。ROOT 子查询包含 ROOT 上 select / where / order 用到的列、route field，以及从 ROOT 发出的 Association local field。

## Metadata 维护

`HdbDatasetDef` 描述逻辑数据集、字段 metadata 和物理表路由。

`HdbFieldDef` 描述字段名、数据库列名、字段类型、内存 offset 和字段标记。新增数据库字段需要在需要查询、写入或结果类型校验时同步字段 metadata。

`HdbAssociationDef` 描述可显式 JOIN 的命名关联。修改 Association 定义会影响所有通过该 Association 发起的显式 Join 调用。

## 错误码

公共错误码定义在 `common/HdbCommon.h`。

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
HDB_ERR_FIELD_REF = -11
HDB_ERR_ASSOCIATION_NOT_FOUND = -12
HDB_ERR_QUERY_NEED_TIME_RANGE = -13
HDB_ERR_QUERY_RANGE = -14
HDB_ERR_SHARD_DEF = -15
HDB_ERR_SHARD_NOT_FOUND = -16
HDB_ERR_NOT_IMPLEMENTED = -17
HDB_ERR_NULL_VALUE = -18
HDB_ERR_TYPE_MISMATCH = -19
HDB_ERR_INTERNAL = -20
```

## 编译兼容

源码按 C++03 风格编写：

- 不使用 `auto`
- 不使用 lambda
- 不使用 `nullptr`
- 不使用范围 `for`
- 不使用智能指针
- DLL 边界不传 STL 对象
- 不让异常穿过 DLL ABI 边界

当前 PG 依赖位于：

```text
svr/include/PostgreSQL/
```

x64 配置需要匹配 x64 版本 `libpq.lib` 和运行时 DLL。

## 自检

`ysd_hdb_svr --selftest-unit` 执行不依赖活动数据库连接的单元自检，覆盖 IPC 协议、AST codec、result codec、分片路由、Association metadata、SQL builder、query executor 和 IPC handler。

`ysd_hdb_svr --selftest [connInfo]` 执行数据库自检，先跑 unit selftest，再创建测试表并验证 CRUD、日分片查询、显式 JOIN、时间条件和结果转换。

全流程 smoke test 位于 `ysd_hdb_smoke_test` 工程，用于验证调用方通过 DLL 访问 SERVER 的实际路径。运行前需要先启动 SERVER。

自动化运行 smoke test 时可设置：

```powershell
$env:HDB_SMOKE_NOPAUSE='1'
```

## 本地 PG 测试库

默认连接信息：

```text
host=127.0.0.1 port=5432 dbname=postgres user=postgres password=postgres
```

SERVER 服务模式和自检模式会优先读取 `HDB_PG_CONNINFO`。
