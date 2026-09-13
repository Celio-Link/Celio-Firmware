#include <zephyr/kernel.h>

#pragma once

enum CableType
{
    GBA,
    GBC
};

enum CableSelection
{
    CABLE_AUTO = 0,
    CABLE_FORCE_GBA = 1,
    CABLE_FORCE_GBC = 2
};

void cableDetection_flipSdPinPath(void);

void cableDetection_detectCableType(void);

enum CableType cableDetection_getDetectedCableType(void);

void cableDetection_setCableOverride(uint8_t mode);