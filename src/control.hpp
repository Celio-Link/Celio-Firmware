#include "layers/usbLayer.hpp"
#include "layers/packetLayer.hpp"
#include "module/link.hpp"
#include "module/emu.hpp"
#include "linkStatus.hpp"
#include "callbacks/commands.hpp"
#include "payloads/pokemon.hpp"
#include "module/moduleInterface.hpp"

class Control
{
    enum class ControlCommand
    {
        SetMode = 0x00,
        Cancel = 0x01
    };

    enum class Mode 
    {
        tradeEmu = 0x00,
        onlineLink = 0x01
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

        switch (m_mode)
        {
            case Mode::tradeEmu:
            {
                party::partyInit();
                UsbLayer::getInstance().setReceiveDataHandler(party::usbReceivePkmFile, nullptr);

                EmuModule emuModule;
                m_currentModule = &emuModule;
                emuModule.execute();

                sendLinkStatus(LinkStatus::EmuTradeSessionFinished);
                break;
            }

            case Mode::onlineLink:
            {
                UsbLayer::getInstance().setReceiveDataHandler(usbLink_receiveHandler, nullptr);

                LinkModule linkModule;
                m_currentModule = &linkModule;
                linkModule.execute();

                sendLinkStatus(LinkStatus::LinkClosed);
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

    void receiveCommand(std::span<const uint8_t> data)
    {
        if (m_currentModule != nullptr && m_currentModule->canHandle(data[0]))
        {
            return m_currentModule->receiveCommand(data);
        } 
        
        if (!canHandle(data[0])) return;
        switch (static_cast<ControlCommand>(data[0]))
        {
            case ControlCommand::SetMode: return callSetMode(static_cast<Mode>(data[1]));
            case ControlCommand::Cancel: return callCancel();
            default: return;
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

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    static void receiveCommandHandler(std::span<const uint8_t> receivedData, void* userData)
    {
        Control* self = static_cast<Control*>(userData);
        self->receiveCommand(receivedData);
    }
};