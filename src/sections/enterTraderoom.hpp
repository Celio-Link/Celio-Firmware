#include <span>

#include "../layers/packetLayer.hpp"

#pragma once

class EnterTradeRoom
{

    enum class BlockCommandState : uint8_t
    {
        LinkPlayer = 0x00,
        TrainerCard = 0x01
    };

public:
    EnterTradeRoom(PacketLayer& layer) : m_packetLayer(layer)
    {}

    void process();

private:

    BlockCommandState m_blockState = BlockCommandState::LinkPlayer;
    PacketLayer& m_packetLayer;
    struct k_sem m_commandSemaphore;
    std::array<uint16_t, 8> m_currentCommand;
};