#include <span>

#include "../layers/packetLayer.hpp"

#include "nextSectionState.hpp"

#pragma once

class TradeDisconnect
{
    enum class BlockCommandState : uint8_t
    {
        None = 0x00,
        LinkPlayer = 0x01,
        FinishTrade = 0x02
    };

    void exchangeTrainerData();

    NextSection handleDisconnect();

public:
    TradeDisconnect(PacketLayer& layer) : m_packetLayer(layer)
    {
        //m_packetLayer.setMode(PacketLayer::Mode::slave);
    }

    ~TradeDisconnect()
    {
        while(!m_packetLayer.idle()) {};
    }

    NextSection process();

private:
    PacketLayer& m_packetLayer;
    BlockCommandState m_blockState = BlockCommandState::None;
};