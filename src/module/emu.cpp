#include "emu.hpp"
#include "../sections/sections.hpp"

void EmuModule::execute()
{
    NextSection nextSection = NextSection::setup;
    while (!m_cancel)
    {
        switch (nextSection)
        {
            case NextSection::setup:
            {
                TradeSetup tradeSetup(LINKTYPE_TRADE_SETUP);
                m_currentSection = &tradeSetup;
                nextSection = tradeSetup.process(); // -> exit / -> connection / -> cancel
                break;
            }
            case NextSection::connection:
            {
                TradeConnection tradeConnection;
                m_currentSection = &tradeConnection;
                nextSection = tradeConnection.process(); // -> tradeLounge? / -> disconnect / -> cancel
                break;
            }
            case NextSection::disconnect:
            {
                TradeDisconnect tradeDisconnection;
                m_currentSection = &tradeDisconnection;
                nextSection = tradeDisconnection.process(); // -> connection / -> cancel
                break;
            }
            case NextSection::lounge:
            {
                TradeLounge tradeLounge;
                m_currentSection = &tradeLounge;
                nextSection = tradeLounge.process(); // -> exit / -> connection / -> cancel
                break;
            }
            case NextSection::cancel: [[fallthrough]];
            case NextSection::exit:
            {
                m_cancel = false;
                return;
            }
        }
        m_currentSection = nullptr;
    }
}