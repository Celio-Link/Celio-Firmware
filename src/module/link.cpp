#include "link.hpp"
#include "../linkStatus.hpp"
#include "../callbacks/commands.hpp"

#include "syscalls/kernel.h"
#include "zephyr/kernel.h"
#include <zephyr/irq.h>


void LinkModule::execute()
{
    m_cancel = false;
    sendLinkStatus(LinkStatus::AwaitMode);
    k_sem_take(&m_waitForLinkModeCommand, K_FOREVER);
    bool keepAlive = true;

    while (keepAlive && !m_cancel)
    {
        {
            UsbSection section(m_packetLayerMode);
            m_currentSection = &section;
            keepAlive = section.process();
        }
        m_currentSection = nullptr; //kinda sketch but commands should only arrive when we have a current section

        if (keepAlive)
        {
            sendLinkStatus(LinkStatus::LinkReconnecting);
            k_sleep(K_MSEC(400));
        }
    }
}

void LinkModule::handleCall(Call& call)
{
    switch (static_cast<LinkModeCall>(call.fid))
    {
        case LinkModeCall::SetMode: return callSetMode(call);
        case LinkModeCall::StartHandshake: return callStartHandshake(call);
        case LinkModeCall::ConnectLink: return callConnectLink(call);
        case LinkModeCall::Data: return callReceiveData(call);
        default: break;
    }
}

void LinkModule::callSetMode(Call& call)
{
    switch(call.params()[0])
    {
        case 0x00: m_packetLayerMode = PacketLayer::Mode::master; break;
        case 0x01: m_packetLayerMode = PacketLayer::Mode::slave; break;
    }
    k_sem_give(&m_waitForLinkModeCommand);
    call.exitSuccess();
}

void LinkModule::callStartHandshake(Call& call)
{
    m_currentSection->startHandshake();
    call.exitSuccess();
}

void LinkModule::callConnectLink(Call& call)
{
    m_currentSection->connectLink();
    call.exitSuccess();
}

void LinkModule::callReceiveData(Call& call) 
{
    usbLink_receiveHandler(call.params());
    call.exitSuccess();
}

