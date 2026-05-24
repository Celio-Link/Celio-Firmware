#include "transport.hpp"

#include "usbLayer.hpp"
#include "serialLayer.hpp"

namespace Transport
{
    namespace
    {
        Id g_active = Id::Usb;
    }

    void setActive(Id id) { g_active = id; }
    Id active() { return g_active; }

    void registerCommandHandler(ReceiveHandler handler, void* userData)
    {
        UsbLayer::getInstance().setReceiveCommandHandler(handler, userData);
        SerialLayer::getInstance().setReceiveCommandHandler(handler, userData);
    }

    void registerDataHandler(ReceiveHandler handler, void* userData)
    {
        UsbLayer::getInstance().setReceiveDataHandler(handler, userData);
        SerialLayer::getInstance().setReceiveDataHandler(handler, userData);
    }

    bool sendStatus(std::span<const uint8_t, 2> data)
    {
        if (g_active == Id::Serial) return SerialLayer::getInstance().sendStatus(data);
        return UsbLayer::getInstance().sendStatus(data);
    }

    bool sendData(std::span<const uint8_t> data)
    {
        if (g_active == Id::Serial) return SerialLayer::getInstance().sendData(data);
        return UsbLayer::getInstance().sendData(data);
    }
}
