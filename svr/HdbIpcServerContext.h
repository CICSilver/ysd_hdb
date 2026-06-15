#ifndef YSD_HDB_IPC_SERVER_CONTEXT_H
#define YSD_HDB_IPC_SERVER_CONTEXT_H

#include "HdbDatasetRegistry.h"
#include "HdbDbAdapter.h"

class CHdbIpcServerContext
{
public:
    CHdbIpcServerContext();

public:
    CHdbDbAdapter* adapter;
    CHdbDatasetRegistry registry;
};

#endif
