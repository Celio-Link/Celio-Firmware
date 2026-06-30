#include "layers/transport/transportLayer.hpp"
#include "module/link.hpp"
#include "module/emu.hpp"
#include "module/advanceWars.hpp"
#include "callbacks/commands.hpp"
#include "hardware.hpp"
#include "persist.hpp"
#include <cstdint>

#pragma once

// Hardware commands (0x40-0x4F) are device-level, handled regardless of active module

    enum class HardwareCommand : uint8_t
    {
        SetVoltage = 0x40,
        SetLEDColor = 0x42,
        RebootBootloader = 0x43,
        SetWebUsbLanding = 0x44,
        GetLedConfig = 0x45,
        SetModeLedColor = 0x46,
        ResetLedColors = 0x47,
        Reboot = 0x48,
    };

class HardwareControl 
{
public:
    bool canHandle(uint8_t command) { return (command & 0xF0) == 0x40; }
    
    void handleCall(Call& call)
    {
        switch (static_cast<HardwareCommand>(call.fid))
        {
            case HardwareCommand::SetVoltage: return callSetVoltage(call);
            case HardwareCommand::SetLEDColor: return callSetLedColor(call);
            case HardwareCommand::RebootBootloader: return callReboot(call);
            case HardwareCommand::GetLedConfig: return callGetLedConfig(call);
            case HardwareCommand::SetModeLedColor: return callSetModeLedColor(call);
            case HardwareCommand::SetWebUsbLanding: return callSetWebUsbLanding(call);
            case HardwareCommand::ResetLedColors: return callResetColors(call);
            default: break;
        }
    }

private:
    void callGetLedConfig(Call& call)
    {
        // [0x45, count, r0,g0,b0, ... r4,g4,b4]
        uint8_t info[2 + LED_SLOT_COUNT * 3];
        info[0] = static_cast<uint8_t>(HardwareCommand::GetLedConfig);
        info[1] = LED_SLOT_COUNT;
        for (uint8_t slot = 0; slot < LED_SLOT_COUNT; slot++) {
            getLedColor(slot, &info[2 + slot * 3]);
        }
    }

    void callSetVoltage(Call& call)
    {
        switch(call.params()[0])
        {
            case 0x00: Hardware::getInstance().setVoltage3V3(); break;
            case 0x01: Hardware::getInstance().setVoltage5V(); break;
        };
    }

    void callSetLedColor(Call& call)
    {
        if (call.params().size() >= 4) 
        {
            Hardware::getInstance().setLED(
                call.params()[0], 
                call.params()[1], 
                call.params()[2], 
                call.params()[3] != 0
            );
        }
        break;
    }

    void callReboot(Call& call)
    {
        switch(call.params()[0])
        {
            case 0x00: Hardware::getInstance().rebootToBootloader(); break;
            case 0x01: Hardware::getInstance().reboot(); break;
        }
    }

    void callSetModeLedColor(Call& call)
    {
        if (call.params().size() >= 4) 
        {
            Hardware::getInstance().setLedColor(
                call.params()[0], 
                call.params()[1], 
                call.params()[2], 
                call.params()[3] != 0
            );
        }
        break;
    }

    void callSetWebUsbLanding(Call& call)
    {
        if (call.params().size() >= 1) 
        {
            setLandingPageEnabled(data[0] != 0);
        }
    }

    void callResetColors(Call& call)
    {
        resetLedColors();
    }
};