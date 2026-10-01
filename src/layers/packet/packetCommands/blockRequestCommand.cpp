#include "blockRequestCommand.hpp"

struct BlockRequestContext : TransmitContext
{
    uint8_t index = 0;
    uint16_t blockCommandContent[8] = {0xCCCC, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
};


std::array<uint16_t, 3> blockRequestTransive(TransmitContext* ctx)
{
    auto* blockCtx = static_cast<BlockRequestContext*>(ctx);
    uint16_t value = blockCtx->blockCommandContent[blockCtx->index];
    std::array<uint16_t, 3> ret = {value, 0x00, 0x00};

    blockCtx->index++;
    if (blockCtx->index == 8)
    {
        blockCtx->index = 0;
    } 

    return ret;
}

TransmitBehaviour blockRequest()
{
    std::unique_ptr<BlockRequestContext> c = std::make_unique<BlockRequestContext>();
    c->index = 0;

    static TransmitBehaviour transive
    {
        .context = std::move(c),
        .transmitCallback = blockRequestTransive,
        .transmitDoneCallback = [](TransmitContext* ctx){ (void)ctx; return CommandState::done; }
    };

    return std::move(transive);
}