#include "emptyCommand.hpp"

std::array<uint16_t, 3> emptyCommandTransive(TransmitContext* ctx)
{
    (void)ctx;
    std::array<uint16_t, 3> ret = {};
    return ret;
}

TransmitBehaviour emptyCommand()
{
    static TransmitBehaviour transive
    {
        .context = nullptr,
        .transmitCallback = emptyCommandTransive,
        .transmitDoneCallback = [](TransmitContext* ctx){ (void)ctx; return CommandState::done; }
    };

    return std::move(transive);
}