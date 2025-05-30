#include "moveCommand.hpp"


static uint8_t g_index = 0;
static uint8_t g_move = 0x11;

void moveCommandInit(std::span<const uint8_t> data)
{
    g_index = 0;
    g_move = data[0];
}

uint16_t moveCommandTransive()
{
    switch(g_index)
    {
        case 0:
            g_index++;
            return 0xCAFE;
        case 1:
            g_index++;
            return g_move;
        case 7:
            g_index = 0;
            return 0x00;
        default:
            g_index++;
         return 0x00;
    }
}

TransiveStruct moveCommand()
{
    static TransiveStruct transive
    {
        .init = moveCommandInit,
        .transive = moveCommandTransive,
        .transiveDone = [](){ return CommandState::done; }
    };

    return transive;
}