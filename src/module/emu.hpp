#include <zephyr/kernel.h>
#include "../layers/packetLayer.hpp"

class EmuModule
{
public:
    EmuModule() {}
    
    void execute() {}

    void receiveCommand(std::span<const uint8_t> command) {}

    bool canHandle(uint8_t command) { return (command & 0xF0) == 0x20; }
};