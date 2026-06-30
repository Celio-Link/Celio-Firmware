#include <zephyr/kernel.h>

#include "../layers/transport/transportLayer.hpp"

#pragma once

struct IModule {

    virtual ~IModule() = default;

    virtual void cancel() = 0;

    virtual bool canHandle(uint8_t command) = 0;

    virtual void handleCall(Call& call) = 0;

};