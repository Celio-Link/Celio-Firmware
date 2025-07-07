
#include "usbLinkBridge.hpp"

void TradeConnection::process()
{
    while (true)
    {
        auto command = m_packetLayer.getCommand();

        if (command[0] = LINKCMD_READY_CLOSE_LINK)
        {
            m_packetLayer.setTransiveHandler(readyCloseLinkCommand());
            return;
        }
                
    }
}