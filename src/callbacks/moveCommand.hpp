#include <zephyr/kernel.h>
#include "TransiveStruct.hpp"

#pragma once 

void moveCommandInit(uint16_t data);

uint16_t moveCommandTransive();

TransiveStruct moveCommand();