#include <span>

#include "../layers/packetLayer.hpp"

#include "nextSectionState.hpp"
#include "sectionConnect.hpp"

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
    TradeDisconnect(PacketLayer& layer, bool& cancel) : m_packetLayer(layer), m_cancel(cancel)
    {
        //section::connectAsSlave(m_packetLayer, m_cancel); // will connect but we currently don't react correctlt to exit standby or something
        section::connectAsMaster(m_packetLayer, m_cancel);
    }

    ~TradeDisconnect()
    {
        while(!m_packetLayer.idle()) {};
    }

    NextSection process();

private:
    PacketLayer& m_packetLayer;
    bool& m_cancel;
    BlockCommandState m_blockState = BlockCommandState::None;
};