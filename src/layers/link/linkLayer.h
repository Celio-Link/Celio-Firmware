#include <zephyr/kernel.h>

#include "hardware/pio.h"

#include "./cableDetection/cableDetection.h"

#pragma once

struct NextTransmit
{
    uint16_t value;
    uint32_t timingUs;
};

enum LinkPin
{
    SC,
    SI,
    SO,
    SD,
    SD_GBA,
    SD_GBC
};

enum LinkPinDir
{
    PIN_DIR_IN,
    PIN_DOR_OUT
};

typedef void (*ReceiveHandler)(uint16_t rx, void* userData);
typedef struct NextTransmit (*TransmitHandler)(void* userData);
typedef void (*TransiveDoneHandler)(uint16_t rx, uint16_t tx, void* userData);

void link_setTransmitCallback(TransmitHandler cb, void* userData);

void link_setReceiveCallback(ReceiveHandler cb, void* userData);

void link_setTransiveDoneCallback(TransiveDoneHandler cb, void* userData);

void link_startTransive();

void link_disable();

void link_enable();

void link_setPioPinDirs(uint32_t pin, enum LinkPinDir direction);

uint32_t link_getPin(enum LinkPin pin);

void link_configureProgram(const pio_program_t* prgramm, uint32_t warp, uint32_t wrapTarget);

void link_configureCableType(enum CableType type);

//link_getReceivedWordCount
uint32_t link_receivedWordCount();

uint8_t link_readPartnerPins();