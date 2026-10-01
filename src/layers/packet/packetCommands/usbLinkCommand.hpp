#include <zephyr/kernel.h>
#include "TransiveStruct.hpp"

TransmitBehaviour usbLinkCommand(uint8_t otherPlayerCount);

void usbLink_receiveHandler(std::span<const uint8_t> data, void*);