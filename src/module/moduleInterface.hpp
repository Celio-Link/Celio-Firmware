#include <zephyr/kernel.h>
#include <span>

#pragma once

struct IModule {

    virtual ~IModule() = default;

    virtual void cancel() = 0;

    virtual bool canHandle(uint8_t command) = 0;

    virtual void receiveCommand(std::span<const uint8_t> command) = 0;

};