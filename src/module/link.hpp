#include <zephyr/kernel.h>
#include "../layers/packetLayer.hpp"

class LinkModule
{
    enum class LinkModeCommand : uint8_t
    {
        SetModeMaster = 0x10,
        SetModeSlave = 0x11,
        StartHandshake = 0x12
    };

public:
    LinkModule(PacketLayer& packetLayer) : m_packetLayer(packetLayer)
    {
        m_packetLayer.disableHandshake();
    }

    void execute();

    bool canHandle(uint8_t command) { return (command & 0xF0) == 0x10; }

    void receiveCommand(std::span<const uint8_t> command);

private:
    void establishConncection();

    PacketLayer& m_packetLayer;

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
    // CALLS
    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
};

