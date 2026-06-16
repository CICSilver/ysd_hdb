#ifndef YSD_HDB_IPC_SERVER_CONTEXT_H
#define YSD_HDB_IPC_SERVER_CONTEXT_H

#include "HdbDatasetRegistry.h"
#include "HdbDbAdapter.h"

// SERVER 单进程上下文
class CHdbIpcServerContext
{
public:
    CHdbIpcServerContext();

public:
    CHdbDbAdapter* adapter;      // 外部创建的数据库适配器
    CHdbDatasetRegistry registry; // 逻辑元数据注册表
};

#endif
