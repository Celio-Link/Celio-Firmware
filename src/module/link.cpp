#include "link.hpp"
#include "../linkStatus.hpp"
#include "../callbacks/commands.hpp"

void LinkModule::execute()
{
    establishConncection();
    m_packetLayer.setTransiveHandler(usbLinkCommand());
    while (true)
    {
        usbLink_loadTransivePacket();

        auto command = m_packetLayer.getCommand();

        if (command[0] != LINKCMD_EMPTY) UsbLayer::getInstance().sendData(std::span(reinterpret_cast<const uint8_t*>(command.data()), 16));

        if (command[0] == LINKCMD_READY_CLOSE_LINK)
        {
            m_packetLayer.setTransiveHandler(readyCloseLinkCommand());
            break;
        }
    }
}

void LinkModule::receiveCommand(std::span<const uint8_t> command)
{
    switch (static_cast<LinkModeCommand>(command[0]))
    {
        case LinkModeCommand::SetModeMaster: return m_packetLayer.setMode(PacketLayer::Mode::master);
        case LinkModeCommand::SetModeSlave: return m_packetLayer.setMode(PacketLayer::Mode::slave);
        case LinkModeCommand::StartHandshake: return m_packetLayer.enableHandshake();
        default: break;
    }
}

void LinkModule::establishConncection()
{
    sendLinkStatus(LinkStatus::HandshakeWaiting);
    while(!m_packetLayer.hasSendHandShake())
    {}
    sendLinkStatus(LinkStatus::HandshakeReceived);
}
