#include "readyCloseLinkCommand.hpp"
#include "../../../link_defines.h"

struct SendReadyExitStandbyContext : TransmitContext
{
    uint8_t index = 0;
};

std::array<uint16_t, 3> readyCloseLinkTransive(TransmitContext* ctx)
{
    auto* usbCtx = static_cast<SendReadyExitStandbyContext*>(ctx);
    std::array<uint16_t, 3> ret = {};

    switch(usbCtx->index)
    {
        case 0:
            usbCtx->index++;
            ret[0] = LINKCMD_READY_CLOSE_LINK;
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

TransmitBehaviour readyCloseLinkCommand()
{
    std::unique_ptr<SendReadyExitStandbyContext> c = std::make_unique<SendReadyExitStandbyContext>();
    c->index = 0;

    static TransmitBehaviour transive
    {
        .context = std::move(c),
        .transmitCallback = readyCloseLinkTransive,
        .transmitDoneCallback = [](TransmitContext* ctx){ return CommandState::done; }
    };

    return std::move(transive);
}