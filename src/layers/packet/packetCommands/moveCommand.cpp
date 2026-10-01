#include "moveCommand.hpp"

struct MoveContext : TransmitContext
{
    uint8_t index = 0;
    size_t repeats = 0;
    uint16_t move;
};

static std::array<uint16_t, 3> moveCommandTransive(TransmitContext* ctx)
{
    auto* moveCtx = static_cast<MoveContext*>(ctx);
    std::array<uint16_t, 3> ret = {};

    switch(moveCtx->index)
    {
        case 0:
            moveCtx->index++;
            ret[0] = 0xCAFE;
            break;
        case 1:
            moveCtx->index++;
            ret[0] = moveCtx->move;
            break;
        case 7:
            moveCtx->index = 0;
            ret[0] = 0x00;
            break;
        default:
            moveCtx->index++;
            ret[0] = 0x00;
            break;
    }

    return ret;
}

TransmitBehaviour moveCommand(int16_t data)
{
    std::unique_ptr<MoveContext> c = std::make_unique<MoveContext>();
    c->move = data;

    static TransmitBehaviour transive
    {
        .context = std::move(c),
        .transmitCallback = moveCommandTransive,
        .transmitDoneCallback = [](TransmitContext* ctx)
        { 
            auto* moveCtx = static_cast<MoveContext*>(ctx);
            if (moveCtx->repeats != 20)
            {
                moveCtx->repeats++;
                return CommandState::resume;   
            }
            return CommandState::done; 
        }
    };

    return std::move(transive);
}