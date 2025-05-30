
#include <span>
#pragma once

enum class CommandState
{
    resume,
    done
};

struct TransiveStruct
{
    using InitCallback = void(*)(std::span<const uint8_t>);
    using TransiveCallback = uint16_t(*)();
    using TransiveDoneCallback = CommandState(*)();

    std::span<const uint8_t> userData;

    InitCallback init;
    TransiveCallback transive;
    TransiveDoneCallback transiveDone;
};