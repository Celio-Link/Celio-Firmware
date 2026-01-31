#include <span>
#include <zephyr/kernel.h>

#include "moduleInterface.hpp"
#include "../sections/section.hpp"


class EmuModule : public IModule
{
public:
    EmuModule() {}
    
    void execute();

    void receiveCommand(std::span<const uint8_t> command) override {}

    bool canHandle(uint8_t command) override { return (command & 0xF0) == 0x20; }

    void cancel() override
    { 
        if (m_currentSection != nullptr) m_currentSection->cancel();
        m_cancel = true;
    }

private:
    bool m_cancel = false;
    Section* m_currentSection = nullptr;
};