#include <span>

#include "../layers/packetLayer.hpp"

#pragma once

class TradeDisconnect
{
    enum class BlockCommandState : uint8_t
    {
        None = 0x00,
        LinkPlayer = 0x01
    };

    void exchangeTrainerData();

    void handleDisconnect();

public:
    TradeDisconnect(PacketLayer& layer) : m_packetLayer(layer)
    {}

    void process();

private:
    PacketLayer& m_packetLayer;
    BlockCommandState m_blockState = BlockCommandState::None;
};