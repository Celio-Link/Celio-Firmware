#pragma once

#include <cstdint>
#include <span>
#include <array>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "../transport.hpp"

// CDC-ACM transport. Mirrors UsbLayer's handler shapes so the rest of the
// firmware sees the same per-packet API regardless of which transport
// delivered the message.
//
// Frame format on the wire (CDC-ACM is a byte stream, so we need our own
// framing to preserve logical packet boundaries):
//
//     | 0x47 0x42 | channel:1 | len:2 LE | payload[len] |
//        sync 'GB'    0=cmd,1=data,2=status
class SerialLayer
{
public:
    static constexpr size_t maxPayload = 64;

    static constexpr uint8_t syncByte0 = 0x47; // 'G'
    static constexpr uint8_t syncByte1 = 0x42; // 'B'

    static constexpr uint8_t channelCommand = 0x00;
    static constexpr uint8_t channelData = 0x01;
    static constexpr uint8_t channelStatus = 0x02;

    static SerialLayer& getInstance();

    SerialLayer(const SerialLayer&) = delete;
    SerialLayer& operator=(const SerialLayer&) = delete;
    SerialLayer(SerialLayer&&) = delete;
    SerialLayer& operator=(SerialLayer&&) = delete;

    bool sendStatus(std::span<const uint8_t, 2> data);
    bool sendData(std::span<const uint8_t> data);

    void setReceiveCommandHandler(Transport::ReceiveHandler handler, void* userData);
    void setReceiveDataHandler(Transport::ReceiveHandler handler, void* userData);

    // Called from the UART IRQ trampoline. Public so the C-callable wrapper
    // can reach it; not part of the public API.
    void onUartIrq();

private:
    SerialLayer();

    void initIfNeeded();
    bool sendFrame(uint8_t channel, std::span<const uint8_t> payload);
    void processIncomingByte(uint8_t b);
    void dispatchFrame();

    struct ReceiveDelegate
    {
        Transport::ReceiveHandler handler = nullptr;
        void* userData = nullptr;
    };

    enum class RxState : uint8_t { sync1, sync2, channel, lenLo, lenHi, payload };

    const struct device* m_dev = nullptr;
    bool m_ready = false;

    ReceiveDelegate m_commandHandler;
    ReceiveDelegate m_dataHandler;

    RxState m_rxState = RxState::sync1;
    uint8_t m_rxChannel = 0;
    uint16_t m_rxLen = 0;
    uint16_t m_rxPos = 0;
    std::array<uint8_t, maxPayload> m_rxBuf{};

    static constexpr size_t txRingSize = 512;
    uint8_t m_txRingMem[txRingSize] = {};
    struct ring_buf m_txRing;
    struct k_mutex m_txMutex;
};
