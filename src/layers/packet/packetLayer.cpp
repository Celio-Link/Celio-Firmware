#include "packetLayer.hpp"
#include "../link/multiMode/multiMode.hpp"

#include "syscalls/kernel.h"
#include <cerrno>

void PacketLayer::onTransiveDone(uint16_t rxBytes, uint16_t txBytes)
{
    switch (m_state)
    {
        case TransiveState::handshake:
            m_transmitedHandShake = txBytes;
            m_receivedHandshake = rxBytes;
            k_sem_give(&m_handshakeSemaphore);
            m_timingUs = timingHandshake;

            if (rxBytes == LINK_MASTER_HANDSHAKE 
                || txBytes == LINK_MASTER_HANDSHAKE)
            {
                m_state = TransiveState::crc;
                m_timingUs = timingCommandBytes;
            }
            break;

        case TransiveState::crc:
            m_state = TransiveState::command;
            m_crc = 0x00;
            break;

        case TransiveState::command:
            
            if (m_commandIndex == 7)
            {
                m_timingUs = timingBetweenCommands;
            }

            if (m_commandIndex != 8) return;

            m_commandIndex = 0;
            m_transmitCommandIndex = 0;
            m_state = TransiveState::crc;
            m_timingUs = timingCommandBytes;
            k_sem_give(&m_commandTransiveSemaphore);
            if (m_waitForDisable >= 1)
            {
                m_waitForDisable = 0;
                k_sem_give(&m_saveToDisableSemaphore);
            }
            if (m_handler.transmitDone() == CommandState::done)
            {
                m_idle = true;
                m_handler = emptyCommand();
            }
            break;
        
        default: break;
    }
}

bool PacketLayer::awaitDisable()
{
    // As slave, just disable since we can't gurantee that the command will be completed
    // As master, let's shutdown gracefully and wait for last command to complete
    if (m_seatNumber == 0)
    {
        m_waitForDisable = 1;
        if (k_sem_take(&m_saveToDisableSemaphore, K_MSEC(100)) == -EAGAIN) return false;
    }
    
    multiMode_selectMode(DISABLED);
    return true;
}
