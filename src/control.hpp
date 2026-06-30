#include "layers/transport/eventDispatcher.hpp"
#include "layers/transport/transportLayer.hpp"
#include "module/link.hpp"
#include "module/emu.hpp"
#include "module/gb.hpp"
#include "module/advanceWars.hpp"
#include "linkStatus.hpp"
#include "callbacks/commands.hpp"
#include "payloads/pokemon.hpp"
#include "module/moduleInterface.hpp"
#include "persist.hpp"
#include "hardwareControl.hpp"
#include "firmware_version.hpp"
#include <cstdint>

class Control
{
    enum class ControlCommand : uint16_t
    {
        SetMode = 0x00,
        Cancel = 0x01,
        EnterGBPrinter = 0x03,
        GetFirmwareInfo = 0x0F
    };

    enum class Mode
    {
        gbaTradeEmu = 0x00,
        gbaLink = 0x01,
        gbLink = 0x02,
        gbPrinter = 0x03,
        advanceWars = 0x04
    };

    static constexpr uint8_t callSetModeId = 0x01;

public:
    Control(EventDispatcher& eventDispatcher) : m_eventDispatcher(eventDispatcher)
    {
        k_sem_init(&m_waitForModeSemaphore, 0, 1);
    }

    void executeMode()
    {
        k_sem_take(&m_waitForModeSemaphore, K_FOREVER);
        // Capture the requested mode; callSetMode always sets m_mode before
        // giving the semaphore (including when preempting a running mode), so we
        // must not clobber it below. The variant travels with the mode.
        const Mode mode = m_mode;
        const uint8_t awVariant = m_awVariant;
        sendLinkStatus(LinkStatus::DeviceReady);

        switch (mode)
        {
            case Mode::gbaTradeEmu:
            {
                applyLedForSlot(LED_SLOT_GBA);
                link_detectCableType();

                party::partyInit();
                //Transport::registerDataHandler(party::usbReceivePkmFile, nullptr);

                EmuModule emuModule;
                m_currentModule = &emuModule;
                emuModule.execute();

                sendLinkStatus(LinkStatus::EmuTradeSessionFinished);
                break;
            }

            case Mode::gbaLink:
            {
                applyLedForSlot(LED_SLOT_GBA);
                link_detectCableType();

                //Transport::registerDataHandler(usbLink_receiveHandler, nullptr);

                LinkModule linkModule;
                m_currentModule = &linkModule;
                linkModule.execute();

                //sendLinkStatus(LinkStatus::LinkClosed);
                break;
            }

            case Mode::gbLink:
            {
                GBModule gbModule;
                m_currentModule = &gbModule;
                gbModule.execute();  // Sets blue LED internally

                //sendLinkStatus(LinkStatus::GBSessionFinished);
                break;
            }

            case Mode::gbPrinter:
            {
                GBModule gbModule;
                m_currentModule = &gbModule;
                gbModule.executePrinterMode();  // Sets purple LED internally

                //sendLinkStatus(LinkStatus::GBSessionFinished);
                break;
            }

            case Mode::advanceWars:
            {
                applyLedForSlot(LED_SLOT_ADVANCE_WARS);
                link_detectCableType();

                //Transport::registerDataHandler(awProto_receiveHandler, nullptr);

                AdvanceWarsModule advanceWarsModule(
                    awVariant == 2 ? awproto::GameVariant::aw2
                                   : awproto::GameVariant::aw1);
                m_currentModule = &advanceWarsModule;
                advanceWarsModule.execute();

                //sendLinkStatus(LinkStatus::LinkClosed);
                break;
            }
        }

        m_currentModule = nullptr;
        applyLedForSlot(LED_SLOT_IDLE); // green = connected, no active mode
    }

    void acceptCall(Call& call)
    {
        if (m_hardwareControl.canHandle(call.fid)) return m_hardwareControl.handleCall(call);

        if (m_currentModule != nullptr && m_currentModule->canHandle(call.fid))
        {
            return m_currentModule->handleCall(call);
        } 
        
        if (!canHandle(call.fid)) return;

        switch (static_cast<ControlCommand>(call.fid))
        {
            case ControlCommand::SetMode: callSetMode(call);
            case ControlCommand::Cancel: return callCancel(call);
            case ControlCommand::GetFirmwareInfo: return callGetFirmwareInfo(call);
            default: return;
        }
    }

private:
    struct k_sem m_waitForModeSemaphore;
    Mode m_mode;
    uint8_t m_awVariant = 0;
    EventDispatcher& m_eventDispatcher;
    HardwareControl m_hardwareControl;

    IModule* m_currentModule = nullptr;

    bool canHandle(uint8_t command) { return (command & 0xF0) == 0x00; }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
    // CALLS
    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void callSetMode(Call& call)
    {
        m_mode = call.params()[0];
        if (call.params().size() >= 2) 
        {
            m_awVariant = call.params()[1];
        }
        // If a mode is already running, cancel it so executeMode unblocks and
        // switches to the new mode (otherwise SetMode would queue behind a mode
        // that never exits). Same cancel path the web "cancel" command uses.
        if (m_currentModule != nullptr)
        {
            m_currentModule->cancel();
        }
        k_sem_give(&m_waitForModeSemaphore);
        call.exitSuccess();
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void callCancel(Call& call)
    {
        if (m_currentModule != nullptr) m_currentModule->cancel();
        call.exitSuccess();
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void callGetFirmwareInfo(Call& call)
    {
        const uint8_t info[] = {
            fw::versionMajor,
            fw::versionMinor,
            fw::versionPatch,
            // Byte 4: WebUSB landing-page enabled (1) / disabled (0). Older web
            // apps simply ignore the extra byte.
            static_cast<uint8_t>(landingPageEnabled() ? 1 : 0)
        };
        std::ranges::copy(std::span(info), call.returnParams().begin());
        call.exitSuccess();
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
};