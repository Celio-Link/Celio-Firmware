#include <zephyr/kernel.h>
#include "TransiveStruct.hpp"

void moveCommandInit(std::span<const uint8_t> data);

uint16_t moveCommandTransive();

TransiveStruct moveCommand();