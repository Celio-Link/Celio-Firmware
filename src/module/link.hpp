#include <zephyr/kernel.h>

#include "../sections//usbSection.hpp"
#include "moduleInterface.hpp"
#include "syscalls/kernel.h"

class LinkModule : public IModule
{
private:
    enum class LinkModeCall : uint8_t
    {
        SetMode = 0x10,
        StartHandshake = 0x12,
        ConnectLink = 0x13,
        Data = 0x14
    };

public:
    LinkModule()
    {
        k_sem_init(&m_waitForLinkModeCommand, 0, 1);
    }

    void execute();

    void cancel() override
    { 
        m_cancel = true;
        k_sem_give(&m_waitForLinkModeCommand);
        if (m_currentSection) m_currentSection->cancel();
    }

    bool canHandle(uint8_t fid) override { return (fid & 0xF0) == 0x10; }

    void handleCall(Call& call) override
    {
        switch (static_cast<LinkModeCall>(call.fid))
        {
            case LinkModeCall::SetMode: return callSetMode(call);
            case LinkModeCall::StartHandshake: return callStartHandshake(call);
            case LinkModeCall::ConnectLink: return callConnectLink(call);
            case LinkModeCall::Data: return callReceiveData(call);
            default: break;
        }
    }

private:

    bool m_cancel = false;
    struct k_sem m_waitForLinkModeCommand;

    UsbSection* m_currentSection = nullptr;
    PacketLayer::Mode m_packetLayerMode = PacketLayer::Mode::slave;

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
    // CALLS
    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void callSetMode(Call& call);
    void callReceiveData(Call& call);
    void callStartHandshake(Call& call);
    void callConnectLink(Call& call);
};

