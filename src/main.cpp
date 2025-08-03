#include <zephyr/kernel.h>

#include "control.hpp"

#include "./layers/packetLayer.hpp"
#include "./layers/usbLayer.hpp"
#include "./sections/tradeSetup.hpp"
#include "./sections/tradeConnection.hpp"
#include "./sections/tradeDisconnected.hpp"

#include "link_defines.h"

int main(void)
{
    PacketLayer g_packetLayer = PacketLayer();
    Control g_control = Control(g_packetLayer);
    while (true)
    {
        g_control.executeMode();
    }

    return 0;
}