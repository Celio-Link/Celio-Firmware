#include "emu.hpp"

void EmuModule::execute()
{
    NextSection nextSection = NextSection::setup;
    while (true)
    {
        switch (nextSection)
        {
            case NextSection::setup:
            {
                m_packetLayer.setMode(PacketLayer::Mode::master);
                connect();
                TradeSetup tradeSetup(m_packetLayer, LINKTYPE_TRADE_SETUP);
                nextSection = tradeSetup.process(); // -> exit / -> connection
                break;
            }
            case NextSection::connection:
            {
                m_packetLayer.setMode(PacketLayer::Mode::master);
                connect();
                TradeConnection tradeConnection(m_packetLayer);
                nextSection = tradeConnection.process(); // -> tradeLounge? / -> disconnect
                break;
            }
            case NextSection::disconnect:
            {
                m_packetLayer.setMode(PacketLayer::Mode::master);
                connect();
                TradeDisconnect tradeDisconnection(m_packetLayer);
                nextSection = tradeDisconnection.process(); // -> connection
                break;
            }
            case NextSection::lounge:
            {
                m_packetLayer.setMode(PacketLayer::Mode::master);
                connect();
                TradeLounge tradeLounge(m_packetLayer);
                nextSection = tradeLounge.process(); // -> exit / -> connection
                break;
            }
            case NextSection::exit: return;
            NVIC_EnableIRQ(USB_IRQn);
        }
    }
}

void EmuModule::connect()
{
    while(m_packetLayer.getReceivedHandshake() != LINK_SLAVE_HANDSHAKE) {}
    m_packetLayer.setMode(PacketLayer::Mode::master);
    m_packetLayer.enableHandshake();
    k_sleep(K_MSEC(500));
    m_packetLayer.connect();
}