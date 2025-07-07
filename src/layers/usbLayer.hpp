
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>
#include <span>
#include <array>
#include <algorithm>

#pragma once


class UsbLayer
{
public:
    using UsbReceiveHandler = void(*)(std::span<const uint8_t>);

    static UsbLayer& getInstance() 
    {
        static UsbLayer instance;
        return instance;
    }

    UsbLayer(const UsbLayer&) = delete;
    UsbLayer& operator=(const UsbLayer&) = delete;
    UsbLayer(UsbLayer&&) = delete;
    UsbLayer& operator=(UsbLayer&&) = delete;

    void sendStatus(std::span<const uint8_t> data)
    {
        std::ranges::copy(data, m_sendData.begin());
        usb_transfer(129, m_sendData.data(), data.size(), USB_TRANS_WRITE, m_usbWriteCallback, this);
    }

    void sendData(std::span<const uint8_t> data)

    void setReceiveCommandHandler(const UsbReceiveHandler handler)
    {
        m_receiveHandler = handler;
    }

    void setReceiveDataHandler()
    {
        
    }

    static constexpr uint16_t endpointSize() { return m_endpointSize; }

private:
    static constexpr uint16_t m_endpointSize = 64;

    void receive(size_t size)
    {
        std::ranges::copy_n(m_endpointBuffer.begin(), size, m_receiveData.begin());
        usb_transfer(129, m_endpointBuffer.data(), m_endpointBuffer.size(), USB_TRANS_READ, m_usbReadCallback, this);
        if (m_receiveHandler != nullptr) m_receiveHandler(std::span(m_receiveData.begin(), size));
    }; 

    UsbReceiveHandler m_receiveHandler = nullptr;

    std::array<uint8_t, m_endpointSize> m_endpointBuffer = {};

    std::array<uint8_t, m_endpointSize> m_receiveData = {};
    std::array<uint8_t, m_endpointSize> m_sendData{};

    UsbLayer() 
    {
        usb_transfer(129, m_endpointBuffer.data(), m_endpointBuffer.size(), USB_TRANS_READ, m_usbReadCallback, this);
    }

    static void m_usbReadCallback(uint8_t ep, int size, void* userData)
    {
        UsbLayer* self = static_cast<UsbLayer*>(userData);
        self->receive(size);
    }

    static void m_usbWriteCallback(uint8_t ep, int size, void* userData)
    {
        UsbLayer* self = static_cast<UsbLayer*>(userData);
        //lift mutex
    }
};
