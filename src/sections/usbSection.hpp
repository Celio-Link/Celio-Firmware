
#include "../layers/packet/packetLayer.hpp"
#include "../layers/usbLayer.hpp"
#include "../layers/transport.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#pragma once

class UsbSection
{
public:
    UsbSection(uint8_t playerSeat, uint8_t playerCount) : m_playerSeat(playerSeat), m_playerCount(playerCount)
    {
        m_packetLayer.sendCommand(usbLinkCommand(playerCount));
        m_packetLayer.setSeatNumber(playerSeat);
    }

    bool process();

    void startHandshake() { m_packetLayer.enableHandshake(); }

    void cancel() 
    {
        m_cancel = true;
        m_packetLayer.cancel(); 
    }

    void connectLink() 
    { 
        if (m_playerSeat == 0) m_packetLayer.connectHandshake();
    }

private:

    void bufferReceivedPackets(const std::span<const uint16_t>& recievedPacket)
    {
        auto packetsBuffer = std::span(m_packets).subspan(
            m_numberOfWaitingPackets * packetSize,
            packetSize
        );

        std::ranges::copy(recievedPacket, packetsBuffer.begin());
        
        // skip directional input of case "no button was pressed" as
        // this will just strain the usb/network connection
        
        if ((packetsBuffer[0] == 0xCAFE && packetsBuffer[1] == 0x11))
        {
            m_noDirectionCommandStreak++;
            if (m_noDirectionCommandStreak > 1)
            {
                packetsBuffer[0] = 0x00;
                packetsBuffer[1] = 0x00;
            }
        }
        else
        {
            m_noDirectionCommandStreak = 0;
        }
        

        m_numberOfWaitingPackets++;

        if (m_numberOfWaitingPackets == maxBufferedPackets)
        {
            flush();
        }
    }

    bool flush()
    {
        // case for end of session
        if (m_numberOfWaitingPackets == 0) return true;
        
        // skip empty commands
        const bool emptyCommands = std::all_of(
            std::begin(m_packets), 
            std::end(m_packets),
            [](uint16_t byte) { return byte == 0x00; }
        );
        
        if (!emptyCommands)
        {
            Transport::sendData(std::span(reinterpret_cast<const uint8_t*>(m_packets.data()), 64));
        }

        std::memset(reinterpret_cast<uint8_t*>(m_packets.data()), 0x00, 64);
        m_numberOfWaitingPackets = 0;

        return true;
    }

    static constexpr uint8_t maxBufferedPackets = 4;
    static constexpr uint8_t packetSize = 8;

    bool m_cancel = false;
    void establishConncection();
    
    std::array<uint16_t, 32> m_packets = {};
    size_t m_numberOfWaitingPackets = 0;
    size_t m_noDirectionCommandStreak = 0;

    PacketLayer m_packetLayer;
    uint8_t m_playerSeat;
    uint8_t m_playerCount;
};