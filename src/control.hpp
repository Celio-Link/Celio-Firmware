#include "layers/usbLayer.hpp"
#include "layers/packetLayer.hpp"
#include "module/link.hpp"
#include "module/emu.hpp"
#include "module/gb.hpp"
#include "linkStatus.hpp"
#include "callbacks/commands.hpp"
#include "payloads/pokemon.hpp"
#include "module/moduleInterface.hpp"
#include "hardware.hpp"

class Control
{
    enum class ControlCommand
    {
        SetMode = 0x00,
        Cancel = 0x01,
        GetFirmwareInfo = 0x0F
    };

    // Firmware version: 2.0.0
    static constexpr uint8_t FW_VERSION_MAJOR = 2;
    static constexpr uint8_t FW_VERSION_MINOR = 0;
    static constexpr uint8_t FW_VERSION_PATCH = 0;

    enum class Mode 
    {
        gbaTradeEmu = 0x00,
        gbaLink = 0x01,
        gbLink = 0x02
    };

    static constexpr uint8_t callSetModeId = 0x01;

public:
    Control()
    {
        UsbLayer::getInstance().setReceiveCommandHandler(receiveCommandHandler, this);

        k_sem_init(&m_waitForModeSemaphore, 0, 1);
    }

    void executeMode()
    {
        k_sem_take(&m_waitForModeSemaphore, K_FOREVER);
        sendLinkStatus(LinkStatus::DeviceReady);

        // LED blue = mode active
        Hardware::getInstance().setLED(0, 0, 5, true);

        switch (m_mode)
        {
            case Mode::gbaTradeEmu:
            {
                party::partyInit();
                UsbLayer::getInstance().setReceiveDataHandler(party::usbReceivePkmFile, nullptr);

                EmuModule emuModule;
                m_currentModule = &emuModule;
                emuModule.execute();

                sendLinkStatus(LinkStatus::EmuTradeSessionFinished);
                break;
            }

            case Mode::gbaLink:
            {
                UsbLayer::getInstance().setReceiveDataHandler(usbLink_receiveHandler, nullptr);

                LinkModule linkModule;
                m_currentModule = &linkModule;
                linkModule.execute();

                sendLinkStatus(LinkStatus::LinkClosed);
                break;
            }

            case Mode::gbLink:
            {
                GBModule gbModule;
                m_currentModule = &gbModule;
                gbModule.execute();  // GB module manages its own LED colors

                sendLinkStatus(LinkStatus::GBSessionFinished);
                break;
            }
        }

        m_mode = {};
        m_currentModule = nullptr;
    }

private:
    struct k_sem m_waitForModeSemaphore;
    Mode m_mode;

    IModule* m_currentModule = nullptr;

    bool canHandle(uint8_t command) { return (command & 0xF0) == 0x00; }

    // Hardware commands (0x40-0x4F) are device-level, handled regardless of active module
    bool isHardwareCommand(uint8_t command) { return (command & 0xF0) == 0x40; }

    enum class HardwareCommand : uint8_t
    {
        SetVoltage3V3 = 0x40,
        SetVoltage5V = 0x41,
        SetLEDColor = 0x42,
    };

    void receiveCommand(std::span<const uint8_t> data)
    {
        // Handle hardware commands first (device-level, always available)
        if (isHardwareCommand(data[0]))
        {
            return handleHardwareCommand(data);
        }

        if (m_currentModule != nullptr && m_currentModule->canHandle(data[0]))
        {
            return m_currentModule->receiveCommand(data);
        } 
        
        if (!canHandle(data[0])) return;
        switch (static_cast<ControlCommand>(data[0]))
        {
            case ControlCommand::SetMode: return callSetMode(static_cast<Mode>(data[1]));
            case ControlCommand::Cancel: return callCancel();
            case ControlCommand::GetFirmwareInfo: return callGetFirmwareInfo();
            default: return;
        }
    }

    void handleHardwareCommand(std::span<const uint8_t> data)
    {
        switch (static_cast<HardwareCommand>(data[0]))
        {
            case HardwareCommand::SetVoltage3V3:
                Hardware::getInstance().setVoltage3V3();
                break;
            case HardwareCommand::SetVoltage5V:
                Hardware::getInstance().setVoltage5V();
                break;
            case HardwareCommand::SetLEDColor:
                if (data.size() >= 5) {
                    Hardware::getInstance().setLED(data[1], data[2], data[3], data[4] != 0);
                }
                break;
            default: break;
        }
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
    // CALLS
    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void callSetMode(Mode mode)
    {
        k_sem_give(&m_waitForModeSemaphore);
        m_mode = mode;
    }

    void callCancel()
    {
        if (m_currentModule != nullptr) m_currentModule->cancel();
    }

    void callGetFirmwareInfo()
    {
        const uint8_t info[] = {
            0x0F, // Echo back the command ID so web app knows this is a firmware info response
            FW_VERSION_MAJOR,
            FW_VERSION_MINOR,
            FW_VERSION_PATCH
        };
        UsbLayer::getInstance().sendData(
            std::span<const uint8_t>(info, sizeof(info)));
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    static void receiveCommandHandler(std::span<const uint8_t> receivedData, void* userData)
    {
        Control* self = static_cast<Control*>(userData);
        self->receiveCommand(receivedData);
    }
};