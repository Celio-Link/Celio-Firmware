#include <zephyr/kernel.h>

#include "./layers/packetLayer.hpp"
#include "./sections/enterTraderoom.hpp"

#include "link_defines.h"

int main(void)
{
    PacketLayer g_packetLayer = PacketLayer();
    EnterTradeRoom g_enterTraderoom(g_packetLayer);
    
    while (true)
    {
        auto command = g_packetLayer.getCommand();

        switch(command[0])
        {
            case LINKCMD_READY_CLOSE_LINK:
                g_packetLayer.reset();
                break;

            case LINKCMD_SEND_LINK_TYPE:
                switch(command[1])
                {
                    case LINKTYPE_TRADE_SETUP:
                        g_enterTraderoom.process();
                        break;
                }
                break;
        }
    }

    return 0;
}