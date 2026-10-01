#include "blockCommandRequestCommand.hpp"
#include "../../../link_defines.h"

struct BloclCommandRequestContext : TransmitContext
{
    uint8_t index = 0;
    uint16_t type = 0;
};

std::array<uint16_t, 3> blockCommandRequestTransive(TransmitContext* ctx)
{
    auto* blkCxt = static_cast<BloclCommandRequestContext*>(ctx);
    std::array<uint16_t, 3> ret = {};

    switch(blkCxt->index)
    {
        case 0:
            blkCxt->index++;
            ret[0] = LINKCMD_SEND_LINK_TYPE;
            break;
        case 1:
            blkCxt->index++;
            ret[0] = blkCxt->type;
            break;
        case 7:
            blkCxt->index = 0;
            ret[0] = 0x00;
            break;
        default:
            blkCxt->index++;
            ret[0] = 0x00;
            break;
    }

    return ret;
}

TransmitBehaviour sendBlockCommandRequestCommand(uint16_t type)
{
    std::unique_ptr<BloclCommandRequestContext> c = std::make_unique<BloclCommandRequestContext>();
    c->index = 0;
    c->type = type;

    static TransmitBehaviour transive
    {
        .context = std::move(c),
        .transmitCallback = blockCommandRequestTransive,
        .transmitDoneCallback = [](TransmitContext* ctx){ return CommandState::done; }
    };

    return std::move(transive);
}