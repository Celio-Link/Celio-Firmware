#pragma once

#include <span>
#include <zephyr/kernel.h>

#include "moduleInterface.hpp"

class GBModule : public IModule
{
public:
    GBModule()
    {
        k_sem_init(&m_dataSemaphore, 0, 1);
    }

    void execute();
    void cancel() override;
    bool canHandle(uint8_t command) override;
    void receiveCommand(std::span<const uint8_t> command) override;

private:
    enum class GBCommand : uint8_t
    {
        SetTimingConfig = 0x30,
        EnterPrinterMode = 0x31,
        ExitPrinterMode = 0x32,
    };

    enum class SubMode { normal, printer };

    bool m_cancel = false;
    SubMode m_subMode = SubMode::normal;

    // Timing config (defaults match reconfigurable firmware)
    uint8_t m_bytesPerTransfer = 1;
    uint32_t m_usBetweenTransfer = 1000;

    struct k_sem m_dataSemaphore;

    // --- Normal SPI passthrough ---
    void normalModeLoop();

    // --- Printer mode (GPIO bit-bang SPI slave) ---
    void printerModeLoop();

    // --- USB data handler ---
    static void usbDataHandler(std::span<const uint8_t> data, void* userData);
};
