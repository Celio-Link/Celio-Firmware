#import "transportLayer.hpp"
#include <cstdint>

#pragma once

enum class EventTypes : uint16_t
{
    DataEvent,
    LinkStatusEvent,
    StatusEvent
};

struct Event
{
    EventTypes type;
    uint8_t data[64];
};

class EventDispatcher
{
public:
    explicit EventDispatcher(TransportControl& transport)
        : m_transport(transport)
    {}

    void publish(Event& e);

private:
    TransportControl& m_transport;
};

inline void EventDispatcher::publish(Event& e)
{
    Call outgoingCall(static_cast<uint16_t>(e.type), 0x00, std::span(e.data, 64));
    m_transport.sendCall(outgoingCall);
}