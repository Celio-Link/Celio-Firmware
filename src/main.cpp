#include <zephyr/kernel.h>

#include "./layers/packetLayer.hpp"
#include "./sections/tradeSetup.hpp"
#include "./sections/tradeConnection.hpp"
#include "./sections/tradeDisconnected.hpp"

#include "link_defines.h"

#include "payloads/pokemon.hpp"

int main(void)
{
    PacketLayer g_packetLayer = PacketLayer();
    TradeSetup g_tradeSetup(g_packetLayer);
    TradeConnection g_tradeConnection(g_packetLayer);
    TradeDisconnect g_tradeDisconnect(g_packetLayer);

    while (true)
    {
        auto command = g_packetLayer.getCommand();

        switch(command[0])
        {
            
            case LINKCMD_SEND_LINK_TYPE:
                switch(command[1])
                {
                    case LINKTYPE_TRADE_SETUP:
                        g_tradeSetup.process();
                        break;
                    
                    case LINKTYPE_TRADE_CONNECTING:
                        g_tradeConnection.process();
                        break;

                    case LINKTYPE_TRADE_DISCONNECTED:
                        g_tradeDisconnect.process();
                        break;
                }
                break;
        }
    }

    return 0;
}