#include "readyExitStandbyCommand.hpp"
#include "../../../link_defines.h"

struct SendLinkTypeContext : TransmitContext
{
    uint8_t index = 0;
    uint16_t type = 0;
};

static std::array<uint16_t, 3> sendLinkTypeCommandTransive(TransmitContext* ctx)
{
    auto* usbCtx = static_cast<SendLinkTypeContext*>(ctx);
    std::array<uint16_t, 3> ret = {};

    switch(usbCtx->index)
    {
        case 0:
            usbCtx->index++;
            ret[0] = LINKCMD_SEND_LINK_TYPE;
            break;
        case 1:
            usbCtx->index++;
            ret[0] = usbCtx->type;
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

TransmitBehaviour sendLinkTypeCommand(uint16_t type)
{
    std::unique_ptr<SendLinkTypeContext> c = std::make_unique<SendLinkTypeContext>();
    c->index = 0;
    c->type = type;

    static TransmitBehaviour transive
    {
        .context = std::move(c),
        .transmitCallback = sendLinkTypeCommandTransive,
        .transmitDoneCallback = [](TransmitContext* ctx){ return CommandState::done; }
    };

    return std::move(transive);
}