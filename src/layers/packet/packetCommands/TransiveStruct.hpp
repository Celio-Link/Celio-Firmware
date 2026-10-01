
#include <memory>
#include <optional>
#include <span>

#pragma once

enum class CommandState
{
    resume,
    done
};

struct TransmitContext
{
    virtual ~TransmitContext() = default;
};

class TransmitBehaviour
{
public:
    using TransiveCallback = std::array<uint16_t, 3>(*)(TransmitContext*);
    using TransiveDoneCallback = CommandState(*)(TransmitContext*);

    std::unique_ptr<TransmitContext> context;

    TransiveCallback transmitCallback;
    TransiveDoneCallback transmitDoneCallback;

    std::array<uint16_t, 3> transmit()
    {
        return transmitCallback(context.get());
    }

    CommandState transmitDone()
    {
        return transmitDoneCallback(context.get());
    }

};