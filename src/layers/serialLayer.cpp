#include "serialLayer.hpp"

#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>

SerialLayer& SerialLayer::getInstance()
{
    static SerialLayer instance;
    return instance;
}

SerialLayer::SerialLayer()
{
    ring_buf_init(&m_txRing, sizeof(m_txRingMem), m_txRingMem);
    k_mutex_init(&m_txMutex);
    initIfNeeded();
}

void SerialLayer::initIfNeeded()
{
    if (m_ready) return;

#if DT_NODE_EXISTS(DT_NODELABEL(cdc_acm_uart0))
    m_dev = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart0));
    if (m_dev == nullptr || !device_is_ready(m_dev)) {
        // Retry on next sendFrame() if the device wasn't bound yet at init.
        return;
    }

    uart_irq_callback_user_data_set(
        m_dev,
        [](const struct device* /*dev*/, void* ud) {
            static_cast<SerialLayer*>(ud)->onUartIrq();
        },
        this);

    uart_irq_rx_enable(m_dev);
    m_ready = true;
#else
    // CDC-ACM not configured in this build — SerialLayer stays inert.
    m_dev = nullptr;
#endif
}

void SerialLayer::onUartIrq()
{
    while (uart_irq_update(m_dev) && uart_irq_is_pending(m_dev))
    {
        if (uart_irq_rx_ready(m_dev))
        {
            uint8_t buf[64];
            int n = uart_fifo_read(m_dev, buf, sizeof(buf));
            for (int i = 0; i < n; i++) processIncomingByte(buf[i]);
        }

        if (uart_irq_tx_ready(m_dev))
        {
            uint8_t txBuf[64];
            uint32_t taken = ring_buf_get(&m_txRing, txBuf, sizeof(txBuf));
            if (taken == 0) {
                uart_irq_tx_disable(m_dev);
            } else {
                int written = uart_fifo_fill(m_dev, txBuf, taken);
                if (written < static_cast<int>(taken)) {
                    // Re-enqueue the tail; safe without locking — this IRQ is
                    // the sole consumer of the ring.
                    ring_buf_put(&m_txRing, txBuf + written, taken - written);
                }
            }
        }
    }
}

void SerialLayer::processIncomingByte(uint8_t b)
{
    switch (m_rxState)
    {
        case RxState::sync1:
            if (b == syncByte0) m_rxState = RxState::sync2;
            break;
        case RxState::sync2:
            if (b == syncByte1) m_rxState = RxState::channel;
            else if (b == syncByte0) m_rxState = RxState::sync2;  // stay
            else m_rxState = RxState::sync1;
            break;
        case RxState::channel:
            m_rxChannel = b;
            m_rxState = RxState::lenLo;
            break;
        case RxState::lenLo:
            m_rxLen = b;
            m_rxState = RxState::lenHi;
            break;
        case RxState::lenHi:
            m_rxLen |= static_cast<uint16_t>(b) << 8;
            if (m_rxLen > maxPayload) {
                m_rxState = RxState::sync1;  // bad/oversized frame, resync
                break;
            }
            m_rxPos = 0;
            if (m_rxLen == 0) {
                dispatchFrame();
                m_rxState = RxState::sync1;
            } else {
                m_rxState = RxState::payload;
            }
            break;
        case RxState::payload:
            m_rxBuf[m_rxPos++] = b;
            if (m_rxPos >= m_rxLen) {
                dispatchFrame();
                m_rxState = RxState::sync1;
            }
            break;
    }
}

void SerialLayer::dispatchFrame()
{
    auto payload = std::span<const uint8_t>(m_rxBuf.data(), m_rxLen);

    // Only claim active transport when the frame matches a real channel + a
    // registered handler. Bytes from OS probing (e.g. ModemManager) can
    // coincidentally form a parseable header on an unknown channel; those are
    // dropped without flipping routing.
    if (m_rxChannel == channelCommand && m_commandHandler.handler != nullptr) {
        Transport::setActive(Transport::Id::Serial);
        m_commandHandler.handler(payload, m_commandHandler.userData);
    } else if (m_rxChannel == channelData && m_dataHandler.handler != nullptr) {
        Transport::setActive(Transport::Id::Serial);
        m_dataHandler.handler(payload, m_dataHandler.userData);
    }
}

bool SerialLayer::sendFrame(uint8_t channel, std::span<const uint8_t> payload)
{
    if (payload.size() > maxPayload) return false;
    initIfNeeded();
    if (!m_ready) return false;

    const uint8_t header[5] = {
        syncByte0,
        syncByte1,
        channel,
        static_cast<uint8_t>(payload.size() & 0xFF),
        static_cast<uint8_t>((payload.size() >> 8) & 0xFF)
    };

    const uint32_t total = sizeof(header) + payload.size();

    k_mutex_lock(&m_txMutex, K_FOREVER);

    if (ring_buf_space_get(&m_txRing) < total) {
        k_mutex_unlock(&m_txMutex);
        return false;
    }

    ring_buf_put(&m_txRing, header, sizeof(header));
    if (!payload.empty()) {
        ring_buf_put(&m_txRing, payload.data(), payload.size());
    }

    k_mutex_unlock(&m_txMutex);

    uart_irq_tx_enable(m_dev);
    return true;
}

bool SerialLayer::sendStatus(std::span<const uint8_t, 2> data)
{
    return sendFrame(channelStatus, data);
}

bool SerialLayer::sendData(std::span<const uint8_t> data)
{
    return sendFrame(channelData, data);
}

void SerialLayer::setReceiveCommandHandler(Transport::ReceiveHandler handler, void* userData)
{
    m_commandHandler = { handler, userData };
}

void SerialLayer::setReceiveDataHandler(Transport::ReceiveHandler handler, void* userData)
{
    m_dataHandler = { handler, userData };
}

// APPLICATION level so the USB stack (brought up at POST_KERNEL 2) is ready
// before we bind the CDC-ACM UART.
static int serial_layer_init(void)
{
    SerialLayer::getInstance();
    return 0;
}

SYS_INIT(serial_layer_init, APPLICATION, 0);
