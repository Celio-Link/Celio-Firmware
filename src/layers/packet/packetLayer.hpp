#include "../link/multiMode/multiMode.hpp"
#include "../link/linkLayer.hpp"
#include "./packetCommands/commands.hpp"
#include "../../link_defines.h"

#include <cstdint>
#include <span>
#include <optional>
#include <utility>
#include <zephyr/drivers/gpio.h>

#pragma once

class PacketLayer
{
public:
    enum class Mode
    {
        master,
        slave
    };

    struct TransiveResult
    {
        std::span<const uint16_t> received;
        std::span<const uint16_t> transmitted;
    };

private:

    enum class TransiveState
    {
        crc,
        command,
        handshake
    };

    enum class HandShakeState
    {
        disabled,
        enabled,
        connect
    };

    static constexpr uint32_t timingHandshake = 30097;
    static constexpr uint32_t timingCommandBytes = 1378;
    static constexpr uint32_t timingBetweenCommands = 12953;

public:
    PacketLayer()
    {
        link_setTransmitCallback(&transmitCallback, this);
        link_setReceiveCallback(&receiveCallback, this);
        link_setTransiveDoneCallback(&transiveDoneCallback, this);
        
        k_sem_init(&m_commandTransiveSemaphore, 0, 1);
        k_sem_init(&m_handshakeSemaphore, 0, 1);
        k_sem_init(&m_saveToDisableSemaphore, 0, 1);
        #ifdef CONFIG_STM32F0
        k_timer_init(&m_timeoutTimer, &packetTimeout, nullptr);
        k_timer_user_data_set(&m_timeoutTimer, this);
        #endif
    }

    ~PacketLayer()
    {
        awaitDisable();
    }

    void sendCommand(TransmitBehaviour handler)
    {
        m_handler = std::move(handler);
        m_idle = false;
    }
    
    TransiveResult awaitTransiveResults()
    {
        return *awaitTransiveResults(K_FOREVER);
    }

    std::optional<TransiveResult> awaitTransiveResults(k_timeout_t timeout)
    {
        if (k_sem_take(&m_commandTransiveSemaphore, timeout) != 0) return std::nullopt;
        return TransiveResult
        {
            std::span(m_receivedCommand),
            std::span(m_transmittedCommand)
        };
    }

    uint16_t getReceivedHandshake()
    {
        return *getReceivedHandshake(K_FOREVER);
    }

    std::optional<uint16_t> getReceivedHandshake(k_timeout_t timeout)
    {
        if (k_sem_take(&m_handshakeSemaphore, timeout) != 0) return std::nullopt;
        return m_receivedHandshake;
    }

    uint16_t getTransmittedHandshake()
    {
        k_sem_take(&m_handshakeSemaphore, K_FOREVER);
        return m_transmitedHandShake;
    }

    void cancel() 
    {  
        // just give to every semaphore so we can make sure we don't softlock,
        // object will be destroyed shorty anway so no concern about state
        k_sem_give(&m_handshakeSemaphore); 
        k_sem_give(&m_commandTransiveSemaphore);
        k_sem_give(&m_saveToDisableSemaphore);
    }

    bool idle() { return m_idle; }

    void enableHandshake() { m_handshakeState = HandShakeState::enabled; }

    void connectHandshake() { m_handshakeState = HandShakeState::connect; }

    bool isHandshakeEnabled() { return m_transmitedHandShake == LINK_SLAVE_HANDSHAKE; }

    void setSeatNumber(uint8_t seatNumber)
    {   
        if (seatNumber == 0)
        {
             
            multiMode_selectMode(SLAVE); 
        }
        else
        {
            multiMode_selectMode(MASTER);
        }

       m_seatNumber = seatNumber;
    }

    uint8_t getSeatNumber() { return m_seatNumber; }

private:

    bool awaitDisable();

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void onReceive(uint16_t rxBytes)
    {
        switch(m_state)
        {
            case TransiveState::crc: return receiveCrc(rxBytes);
            case TransiveState::command: return receiveCommand(rxBytes);
            case TransiveState::handshake: return receiveHandshake(rxBytes);
            default: return;
        };
    }

    struct NextTransmit onTransmit()
    {
        switch(m_state)
        {
            case TransiveState::crc: return {m_playerCount, m_seatNumber, transmitCrc(), m_timingUs};
            case TransiveState::command: return {m_playerCount, m_seatNumber, transmitCommand(), m_timingUs};
            case TransiveState::handshake: return {m_playerCount, m_seatNumber, transmitHandshake(), m_timingUs};
            default: return {4, 0, {}, m_timingUs};
        };
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void receiveHandshake(uint16_t rxBytes)
    {
        m_receivedHandshake = rxBytes;
    }

    std::array<uint16_t, 3> transmitHandshake()
    {
        std::array<uint16_t, 3> handshakes = {};

        switch(m_handshakeState)
        {
            case HandShakeState::disabled:
                std::fill(handshakes.begin(), handshakes.end(), LINK_HANDSHAKE_DISABLE);
                break;
            case HandShakeState::enabled:
                std::fill(handshakes.begin(), handshakes.end(), LINK_SLAVE_HANDSHAKE);
                break;
            case HandShakeState::connect:
                std::fill(handshakes.begin(), handshakes.end(), LINK_SLAVE_HANDSHAKE);
                handshakes[0] = LINK_MASTER_HANDSHAKE;
                break;
        }

        return handshakes;
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void receiveCrc(uint16_t rxBytes)
    {
        (void)rxBytes;
        // don't care
    }

    std::array<uint16_t, 3> transmitCrc()
    {
        std::array<uint16_t, 3> crcList = {m_crc, m_crc, m_crc};
        return crcList;
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void receiveCommand(uint16_t rxBytes)
    {
        m_receivedCommand[m_commandIndex] = rxBytes;
        m_crc += rxBytes;
        m_commandIndex++;
    }

    std::array<uint16_t, 3> transmitCommand()
    {
        std::array<uint16_t, 3> txBytes = m_handler.transmit();
        for (uint8_t i = 0; i < m_playerCount - 1; i++)
        {
            m_crc += txBytes[i];
        }
        return txBytes;
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void onTransiveDone(uint16_t rxBytes, uint16_t txBytes);

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

private:
    atomic_t m_waitForDisable = 0;

    bool m_idle = true;
    uint8_t m_seatNumber = 1;
    uint8_t m_playerCount;

    uint16_t m_receivedHandshake = LINK_HANDSHAKE_DISABLE;
    uint16_t m_transmitedHandShake = LINK_HANDSHAKE_DISABLE;
    uint16_t m_crc = LINK_SLAVE_HANDSHAKE; //first crc is always handshake

    uint32_t m_timingUs = 0;

    struct k_sem m_commandTransiveSemaphore;
    struct k_sem m_handshakeSemaphore;
    struct k_sem m_saveToDisableSemaphore;

    int m_commandIndex = 0;
    std::array<uint16_t, 8> m_receivedCommand = {};
    int m_transmitCommandIndex = 0;
    std::array<uint16_t, 8> m_transmittedCommand = {};

    TransmitBehaviour m_handler = emptyCommand();
    TransiveState m_state = TransiveState::handshake;
    HandShakeState m_handshakeState = HandShakeState::disabled;

    struct k_timer m_timeoutTimer;
    #ifdef CONFIG_STM32F0
    MasterClock m_masterClock;
    #endif
    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
    // Callbacks
    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    static void receiveCallback(uint16_t rxBytes, void* userData)
    {
        PacketLayer* self = static_cast<PacketLayer*>(userData);
        return self->onReceive(rxBytes);
    }

    static struct NextTransmit transmitCallback(void* userData)
    {
        PacketLayer* self = static_cast<PacketLayer*>(userData);
        return self->onTransmit();
    }

    static void transiveDoneCallback(uint16_t rxBytes, uint16_t txBytes, void* userData)
    {
        PacketLayer* self = static_cast<PacketLayer*>(userData);
        self->onTransiveDone(rxBytes, txBytes);
    }

    #ifdef CONFIG_STM32F0
    static void packetTimeout(struct k_timer *timer)
    {
        void* userData = k_timer_user_data_get(timer);
        PacketLayer* self = static_cast<PacketLayer*>(userData);
        self->reset();
    }
    #endif
};