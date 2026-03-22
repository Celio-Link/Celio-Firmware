
#include "rawRelay.hpp"
#include "../linkStatus.hpp"

void RawRelayModule::execute()
{
    m_cancel = false;

    // Step 1: Emit AwaitMode — server will assign master/slave
    sendLinkStatus(LinkStatus::AwaitMode);
    k_sem_take(&m_waitForLinkModeCommand, K_FOREVER);
    if (m_cancel) return;

    // Step 2: Mode stored in receiveCommand(), emit HandshakeReceived
    // Server waits for both clients to report HandshakeReceived, then sends StartHandshake
    sendLinkStatus(LinkStatus::HandshakeReceived);
    k_sem_take(&m_waitForStart, K_FOREVER);
    if (m_cancel) return;

    // Step 3: Create section first (registers linkLayer callbacks),
    // THEN enable PIO so it doesn't clock before callbacks exist
    {
        RawRelaySection section;
        m_currentSection = &section;
        link_changeMode(m_linkMode);
        sendLinkStatus(LinkStatus::LinkConnected);
        section.process();
    }
    m_currentSection = nullptr;
    link_changeMode(DISABLED);
}

void RawRelayModule::receiveCommand(std::span<const uint8_t> command)
{
    switch (static_cast<LinkModeCommand>(command[0]))
    {
        case LinkModeCommand::SetModeMaster:
            m_linkMode = MASTER;
            k_sem_give(&m_waitForLinkModeCommand);
            break;

        case LinkModeCommand::SetModeSlave:
            m_linkMode = SLAVE;
            k_sem_give(&m_waitForLinkModeCommand);
            break;

        case LinkModeCommand::StartHandshake:
            k_sem_give(&m_waitForStart);
            break;

        case LinkModeCommand::ConnectLink:
            // Ignored in passthrough mode — relay is already running
            break;

        default: break;
    }
}
