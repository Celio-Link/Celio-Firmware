#include "span"
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <zephyr/kernel.h>

class Transport;

class Call
{
public:
    explicit Call(uint16_t fid, uint8_t flags, std::span<uint8_t> data) {}
    bool exitSuccess();
    bool exitError();

    void fillPayload(std::span<const uint8_t> data)
    {
        std::ranges::copy(data, m_payload.subspan(m_currentPayloadSize).begin());
        m_currentPayloadSize += data.size();
    }

    bool payloadCompletlyReceived() { return m_currentPayloadSize == m_payload.size(); }

private:
    std::span<uint8_t> m_payload;
    uint32_t m_currentPayloadSize = 0;
    Transport* m_transport;
};

class Transport
{
public:
    Call& receiveCall();
    bool returnCall(Call& call);

private:
    void receiveData(std::span<const uint8_t> data) 
    {
        if (m_idle)
        {
            uint16_t fid = 0x00;
            uint32_t payloadSize = 0x00;
            uint8_t flags = 0x00;
            std::span<uint8_t> payload(m_payloadBuffer.data(), payloadSize);
            data = data.subspan(6);
            Call newCall(fid, flags, std::span(m_payloadBuffer.data(), payloadSize));
        }
        m_currentCall.fillPayload(data);
        if (m_currentCall.payloadCompletlyReceived()) k_sem_give(&m_waitForCall);
    }

    bool m_idle = true;
    Call m_currentCall;
    struct k_sem m_waitForCall;
    std::array<uint8_t, 2048> m_payloadBuffer = {};

    static void receiveHandler(std::span<const uint8_t> receivedData, void* userData)
    {
        Transport* self = static_cast<Transport*>(userData);
        self->receiveData(receivedData);
    }
};

inline Call& Transport::receiveCall()
{
    k_sem_take(&m_waitForCall, K_FOREVER);
    return m_currentCall;
}

inline bool Transport::returnCall(Call& call)
{

}