#include "emptyCommand.hpp"
#include "TransiveStruct.hpp"

TxValues emptyCommandTransive(TransmitContext* ctx)
{
    (void)ctx;
    TxValues txValues;
    txValues.push(0x00);
    return txValues;
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