
#include "../layers/packetLayer.hpp"
#include "nextSectionState.hpp"

#pragma once 

class Section 
{

public:
    virtual NextSection process() = 0;

    void cancel() 
    {
        m_packetLayer.cancel();
        m_cancel = true;
    }

protected:
    inline void connectAsMaster()
    {
        m_packetLayer.setMode(PacketLayer::Mode::master);

        while(m_packetLayer.getReceivedHandshake() != LINK_SLAVE_HANDSHAKE) 
        {
            if (m_cancel) return;
        }

        m_packetLayer.enableHandshake();
        k_sleep(K_MSEC(500));
        m_packetLayer.connectHandshake();
    }

    inline void connectAsSlave()
    {
        m_packetLayer.setMode(PacketLayer::Mode::slave);

        while(m_packetLayer.getReceivedHandshake() != LINK_SLAVE_HANDSHAKE) 
        {
            if (m_cancel) return;
        }

        m_packetLayer.enableHandshake();
    }

    bool m_cancel;
    PacketLayer m_packetLayer;
};

