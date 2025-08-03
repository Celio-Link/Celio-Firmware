#include "link.hpp"
#include "../linkStatus.hpp"
#include "../callbacks/commands.hpp"
#include <zephyr/irq.h>

namespace 
{
    constexpr uint32_t modeEvent = BIT(0);
    constexpr uint32_t enableHandshaleEvent = BIT(1);
    constexpr uint32_t masterConnectEvent = BIT(2);
}

void LinkModule::execute()
{
    m_packetLayer.reset();
    m_packetLayer.disableHandshake();
    sendLinkStatus(LinkStatus::HandshakeWaiting);
    k_event_wait(&m_connectEvent, modeEvent, true, K_FOREVER);
    establishConncection();
    m_packetLayer.setTransiveHandler(usbLinkCommand());

    bool keepAlive = true;
    
    while (true)
    {
        auto usbCommand = usbLink_loadTransivePacket();
        auto linkCommand = m_packetLayer.getCommand();

        NVIC_EnableIRQ(USB_IRQn);

        UsbLayer::getInstance().sendData(std::span(reinterpret_cast<const uint8_t*>(linkCommand.data()), 16));

        if ((linkCommand[0] == LINKCMD_SEND_HELD_KEYS) && (linkCommand[1] == LINK_KEY_CODE_EXIT_ROOM))
        {
            keepAlive = false;
        } 

        if (usbCommand.has_value() && ((usbCommand.value()[0] == LINKCMD_SEND_HELD_KEYS) && (usbCommand.value()[1] == LINK_KEY_CODE_EXIT_ROOM)))
        {
            keepAlive = false;
        }

        if (linkCommand[0] == LINKCMD_READY_CLOSE_LINK)
        {
            NVIC_DisableIRQ(USB_IRQn);

            m_packetLayer.setTransiveHandler(readyCloseLinkCommand());

            while (!m_packetLayer.idle()) { }
            
            NVIC_EnableIRQ(USB_IRQn);
            
            if (keepAlive)
            {
                sendLinkStatus(LinkStatus::LinkReconnecting);
                m_packetLayer.disableHandshake();
                k_sleep(K_MSEC(40));
                m_packetLayer.reset();
                k_sleep(K_MSEC(200));
                if (m_packetLayer.getMode() == PacketLayer::Mode::master)
                {
                    m_packetLayer.setMode(PacketLayer::Mode::master); // enable sync again
                }
                establishConncection();
                m_packetLayer.setTransiveHandler(usbLinkCommand());
            }
            else 
            {
                sendLinkStatus(LinkStatus::LinkClosed);
                break;
            }
            
        }
        k_sleep(K_MSEC(5));
        NVIC_DisableIRQ(USB_IRQn);
    }
}

void LinkModule::receiveCommand(std::span<const uint8_t> command)
{
    switch (static_cast<LinkModeCommand>(command[0]))
    {
        case LinkModeCommand::SetModeMaster:
            m_packetLayer.setMode(PacketLayer::Mode::slave);
            k_event_post(&m_connectEvent, modeEvent);
            break;
        case LinkModeCommand::SetModeSlave:
            m_packetLayer.setMode(PacketLayer::Mode::master);
            k_event_post(&m_connectEvent, modeEvent);
            break;
        case LinkModeCommand::StartHandshake: return m_packetLayer.enableHandshake();
        case LinkModeCommand::ConnectLink: return m_packetLayer.connect();
        default: break;
    }
}

void LinkModule::establishConncection()
{
    while(m_packetLayer.getReceivedHandshake() != LINK_SLAVE_HANDSHAKE) { }
    sendLinkStatus(LinkStatus::HandshakeReceived);

    while(!m_packetLayer.isHandshakeEnabled()) { }

    switch (m_packetLayer.getMode())
    {
        case PacketLayer::Mode::master:
            while(m_packetLayer.getTransmittedHandshake() != LINK_MASTER_HANDSHAKE) { }
            return;

        case PacketLayer::Mode::slave:
            while(m_packetLayer.getReceivedHandshake() != LINK_MASTER_HANDSHAKE) { }
            sendLinkStatus(LinkStatus::LinkConnected);
            return;
    }
}

