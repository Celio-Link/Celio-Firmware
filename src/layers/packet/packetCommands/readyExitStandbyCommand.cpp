#include "readyExitStandbyCommand.hpp"
#include "../../../link_defines.h"

struct SendExitStandbyContext : TransmitContext
{
    uint8_t index = 0;
};

std::array<uint16_t, 3> readyExitStandbyCommandTransive(TransmitContext* ctx)
{
    auto* usbCtx = static_cast<SendExitStandbyContext*>(ctx);
    std::array<uint16_t, 3> ret = {};

    switch(usbCtx->index)
    {
        case 0:
            usbCtx->index++;
            ret[0] = LINKCMD_READY_EXIT_STANDBY;
            break;
        case 7:
            usbCtx->index = 0;
            ret[0] = 0x00;
            break;
        default:
            usbCtx->index++;
            ret[0] = 0x00;
            break;
    }

    return ret;
}

TransmitBehaviour readyExitStandbyCommand()
{
    std::unique_ptr<SendExitStandbyContext> c = std::make_unique<SendExitStandbyContext>();
    c->index = 0;

    static TransmitBehaviour transive
    {
        .context = std::move(c),
        .transmitCallback = readyExitStandbyCommandTransive,
        .transmitDoneCallback = [](TransmitContext* ctx){ (void)ctx; return CommandState::done; }
    };

    return std::move(transive);
}