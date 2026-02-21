#include "pokemon.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>


//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

namespace party 
{
    const static std::array<uint8_t, 100> g_fillerPkmnArray = 
    {
        0x57, 0xe5, 0x88, 0x2a, 0x39, 0x30, 0x31, 0xd4, 0xc0, 0xc3, 
        0xc6, 0xc6, 0xbf, 0xcc, 0xff, 0x00, 0x00, 0x00, 0x02, 0x02, 
        0xc8, 0xdd, 0xe0, 0xe7, 0xff, 0xff, 0x00, 0x00, 0xfd, 0xd1, 
        0x00, 0x00, 0x7e, 0x14, 0xa0, 0x9c, 0xfc, 0x25, 0x99, 0xc7, 
        0x6e, 0xd5, 0xbd, 0xfe, 0x6e, 0xd5, 0xb9, 0xfe, 0x6e, 0xd5, 
        0xb9, 0xfe, 0x6e, 0xd5, 0xb9, 0xfe, 0x83, 0xd5, 0xb9, 0xfe, 
        0x6e, 0xd5, 0xb9, 0xfe, 0x76, 0xd5, 0xb9, 0xfe, 0xa7, 0xd5, 
        0xfd, 0xfe, 0x67, 0xe8, 0xb9, 0xfe, 0x6d, 0x93, 0xb9, 0xfe, 
        0x00, 0x00, 0x00, 0x00, 0x19, 0xFF, 0x3f, 0x00, 0x3f, 0x00, 
        0x2e, 0x00, 0x24, 0x00, 0x1d, 0x00, 0x2d, 0x00, 0x20, 0x00
    };

    static size_t g_receivedBytes = 0;
    static std::array<uint8_t, 600> g_party = {};

    static size_t g_partnerReceivedBytes = 0;
    static std::array<uint8_t, 600> g_partnerParty = {};

    static constexpr size_t chunkSize = 200;
    static constexpr size_t numberOfSlots = 6;

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    std::span<const uint8_t> getParty() { return g_party; }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    std::span<uint8_t> slot(uint8_t index) 
    { 
        return std::span(g_party).subspan(index * 100, 100); 
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void clearPartySlot(uint8_t index)
    {
        std::ranges::copy(g_fillerPkmnArray, slot(index).begin());
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    bool isPartySlotEmpty(uint8_t index)
    {
        return memcmp(g_fillerPkmnArray.data(), slot(index).data(), 100) == 0;
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    int partyInit()
    {
        g_receivedBytes = 0;
        for (size_t i = 0; i < numberOfSlots; i++)
        {
            clearPartySlot(i);
        }
        return 0;
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void partnerPartyInit()
    {
        g_partnerReceivedBytes = 0;
        std::fill(g_partnerParty.begin(), g_partnerParty.end(), 0x00);
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void partnerPartyConstruct(std::span<const uint8_t> data)
    {
        size_t nextThreshold = ((g_partnerReceivedBytes / chunkSize) + 1) * chunkSize;

        if (g_partnerReceivedBytes + data.size() > nextThreshold) 
        {
            data = data.first(nextThreshold - g_partnerReceivedBytes);
        }
        std::ranges::copy(data, std::span(g_partnerParty).subspan(g_partnerReceivedBytes).begin());
        g_partnerReceivedBytes += data.size();
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void tradePkmnAtIndex(uint8_t index)
    {
        std::ranges::copy(std::span(g_partnerParty).subspan(index * 100, 100), slot(index).begin());
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

    void usbReceivePkmFile(std::span<const uint8_t> data, void*)
    {
        static std::array<uint8_t, 0x64> encryptedPkm;
        std::ranges::copy(data, std::span(encryptedPkm).subspan(g_receivedBytes).begin());
        g_receivedBytes += data.size();
        if (g_receivedBytes >= 0x64)
        {
            for (size_t i = 0; i < numberOfSlots; i++)
            {
                if (isPartySlotEmpty(i))
                {
                    std::ranges::copy(encryptedPkm, slot(i).begin());
                    break;
                }
            }
            g_receivedBytes = 0;
        }
    }

    //-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
}