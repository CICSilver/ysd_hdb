#ifndef YSD_HDB_IPC_COMMAND_HANDLER_H
#define YSD_HDB_IPC_COMMAND_HANDLER_H

#include "HdbIpcServerContext.h"
#include "../common/HdbIpcProtocol.h"

#include <vector>

class CHdbIpcCommandHandler
{
public:
    explicit CHdbIpcCommandHandler(CHdbIpcServerContext* context);

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
    CHdbIpcServerContext* m_context;
};

#endif
