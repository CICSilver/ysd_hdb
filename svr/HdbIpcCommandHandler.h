#ifndef YSD_HDB_IPC_COMMAND_HANDLER_H
#define YSD_HDB_IPC_COMMAND_HANDLER_H

#include "HdbIpcServerContext.h"
#include "../common/HdbIpcProtocol.h"

#include <vector>

// SERVER IPC 命令分发入口
class CHdbIpcCommandHandler
{
public:
    explicit CHdbIpcCommandHandler(CHdbIpcServerContext* context);

    // 输入是完整 IPC frame 字节，输出也是完整 response frame 字节
    int HandleRequest(const std::vector<unsigned char>& requestBytes,
        std::vector<unsigned char>& responseFrame);

private:
    int DispatchRequest(const HdbIpcFrame& requestFrame,
        std::vector<unsigned char>& responseFrame);
    int HandlePing(const HdbIpcFrame& requestFrame,
        std::vector<unsigned char>& responseFrame);
    int HandleDbPing(const HdbIpcFrame& requestFrame,
        std::vector<unsigned char>& responseFrame);
    int HandleQueryExecute(const HdbIpcFrame& requestFrame,
        std::vector<unsigned char>& responseFrame);
    int HandleQueryExecuteAffected(const HdbIpcFrame& requestFrame,
        std::vector<unsigned char>& responseFrame);
    // response 统一在这里组包，避免各 HandleXxx 手写 header
    int BuildIpcResponse(unsigned int command,
        unsigned int sequence,
        int status,
        const std::vector<unsigned char>& body,
        std::vector<unsigned char>& responseFrame);
    int BuildIpcErrorResponse(unsigned int command,
        unsigned int sequence,
        int status,
        const char* errorText,
        std::vector<unsigned char>& responseFrame);

private:
    CHdbIpcServerContext* m_context; // SERVER 运行上下文
};

#endif
