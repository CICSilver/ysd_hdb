# ysd_hdb 历史库模块设计说明

## 模块定位

`ysd_hdb` 按 DLL + SERVER 两侧组织。DLL 侧只提供对外 C ABI、请求组包和结果读取，不直接访问数据库。SERVER 侧管理数据库连接、元数据、分片路由、参数化 SQL 生成和执行。

调用链保持：

```text
调用方
  -> dll/ysd_hdb.h / CHdbQueryBuilder
  -> dll/ysd_hdb_c.h 低层 C ABI
  -> DLL 侧 HDB_SESSION / HDB_QUERY / HDB_SOURCE
  -> CHdbQueryAst 文本协议
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

调用方可作为 ROOT 或 JOIN target 使用的逻辑数据集，同时描述字段 metadata 和物理表路由。示例：`alarm`、`point`、`device`。

Source：

某一次查询中的具体表来源或 JOIN 实例。ROOT source 固定为 `sourceId = 0`，JOIN source 按创建顺序递增。每个 JOIN 调用都会创建新的 source；同一目标数据集可以通过显式 ON 多次 JOIN，并生成不同 SQL alias。

Join：

某一次查询选择从哪个 source JOIN 到哪个目标 Dataset，并显式指定 ON 两侧字段。JOIN 类型使用 `HDB_JOIN_LEFT` 或 `HDB_JOIN_INNER`。

Field：

某个 Source 所属 Dataset 上的字段。字段名只作为 metadata 查找键，不能包含点号导航。Select 不会自动触发 JOIN。

## 用户入口

普通调用方包含 `dll/ysd_hdb.h` 和生成器生成的 `GeneratedMetaFiles/HdbGeneratedDsl.h`，并使用 `CHdbDslContext` 构造查询。`dll/ysd_hdb_c.h` 是低层 C ABI 头文件，用于 DLL 导出、跨语言绑定和底层边界测试；业务代码不需要直接调用低层查询函数。

`dll/HdbQueryBuilder.h` 是 header-only 包装层，只转发到底层 C ABI，不导出 C++ ABI，不维护独立查询模型，不抛异常。`CHdbDslTable` 和 `CHdbDslField` 是 Builder 层的轻量值对象，不拥有底层句柄，也不跨 DLL ABI 边界传递。

```cpp
CHdbDslContext create(session);
CHdbDslResult result;

int ret = create.select(HdbDsl::ALARM.ID)
    .select(HdbDsl::POINT.NAME)
    .select(HdbDsl::DEVICE.NAME)
    .from(HdbDsl::ALARM)
    .leftJoin(HdbDsl::POINT)
    .on(HdbDsl::ALARM.POINT_ID.eq(HdbDsl::POINT.ID))
    .leftJoin(HdbDsl::DEVICE)
    .on(HdbDsl::POINT.DEVICE_ID.eq(HdbDsl::DEVICE.ID))
    .where(HdbDsl::ALARM.LEVEL.ge(2))
    .orderBy(HdbDsl::ALARM.OCCUR_TIME.desc())
    .timeRange(beginMs, endMs)
    .limit(10, 0)
    .fetch(&result);
```

调用代码能直接看出：

```text
alarm
  LEFT JOIN point ON alarm.point_id = point.id
  LEFT JOIN device ON point.device_id = device.id
```

Builder 记录第一次失败的返回码；第一次失败后后续链式方法不再修改 query。析构时释放仍持有的 `HDB_QUERY`。

## AST 文本协议

AST Encode 不写版本字段。Decoder 仍接受旧 `source=join_on`，并转换成字段-字段等值条件。

文本格式示例：

```text
statement=1
source=root|0|alarm
source=join_condition|1|0|point|2|0
source=join_condition|2|1|device|2|1
time=1781193600000,1781366400000
select=0|id|id
select=1|name|pointName
select=2|name|deviceName
condition=field_compare|0|0|point_id|1|1|id
condition=field_compare|1|1|device_id|1|2|id
condition=compare|2|0|level|4|1|2
where_root=2
order=0|occur_time|2
limit=10,0
```

SERVER 解析后重新校验：

- 必须且只能有一个 ROOT source
- ROOT sourceId 必须为 0
- sourceId 不得重复，JOIN parent 必须已存在
- JOIN target Dataset 必须存在
- JOIN source 必须带 onRootNodeId，且指向条件树节点
- JOIN ON 只能引用当前 JOIN 目标 source，以及 ROOT 和之前已 JOIN 的 source
- JOIN ON 每个 OR 分支都必须包含当前目标 source 与已有 source 的字段-字段关联谓词
- JOIN ON 支持字段-字段比较和字段-值过滤，不支持 between / in / null 条件
- joinType 只能是 `HDB_JOIN_LEFT` 或 `HDB_JOIN_INNER`
- select / where / order 的 sourceId 必须存在
- fieldName 必须属于该 source 对应 Dataset
- where 值类型必须与字段类型匹配，字段-字段比较两侧类型必须兼容
- 表名和列名只来自 SERVER metadata
- 日分片 ROOT 必须提供时间范围
- 当前不支持日分片 Dataset 作为 JOIN target

## SQL Builder

`CHdbQuerySqlBuilder` 将 source 解析成稳定 SQL alias：

```text
s0, s1, s2, ...
```

每个 JOIN source 都生成一次 JOIN，不自动合并：

```sql
left join hdb_point s1 on s0.point_id = s1.id
left join hdb_device s2 on s1.device_id = s2.id
```

字段表达式直接由 `sourceId + fieldName` 解析：

```text
sourceId 2, field name -> s2.name
```

日分片 ROOT 仍通过 `union all` 子查询实现。ROOT 子查询包含 ROOT 上 select / where / order / join on 用到的列和 route field。日分片查询仍必须显式传入 `timeRange`，where 中的时间条件只作为 SQL 过滤条件，不自动替代分片路由范围。

## Metadata 维护

`HdbDatasetDef` 描述逻辑数据集、字段 metadata 和物理表路由。

`HdbFieldDef` 描述字段名、数据库列名、字段类型、内存 offset 和字段标记。新增数据库字段需要在需要查询、写入或结果类型校验时同步字段 metadata。

## 错误码

公共错误码以 `common/HdbCommon.h` 为准。README 不再复制错误码清单，避免文档和头文件定义脱节。

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

`ysd_hdb_svr --selftest-unit` 执行不依赖活动数据库连接的单元自检，覆盖 IPC 协议、AST codec、result codec、分片路由、SQL builder、query executor 和 IPC handler。

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
