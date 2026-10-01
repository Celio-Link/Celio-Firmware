#include "usbLinkCommand.hpp"
#include "../../../link_defines.h"
#include "TransiveStruct.hpp"
#include "zephyr/kernel.h"
#include <cstdint>
#include <cstring>
#include <optional>
#include <algorithm>

static constexpr uint8_t packetSize = 17;
static constexpr uint8_t dataCountOffset = 1;

// Queues should be synced by zephyr internally, so no need for atomics or mutexes.
// Besides, due to current structurings, usbLink_receiveHandler and loadTransivePacket
// run in ISR context, maybe improve later
K_MSGQ_DEFINE(g_packetQueue0, 16, 200, 1);
K_MSGQ_DEFINE(g_packetQueue1, 16, 200, 1);
K_MSGQ_DEFINE(g_packetQueue2, 16, 200, 1);

struct SeatQueueStatus
{
    bool packetAvailable = false;
    uint16_t currentPacket[8] = {};
    k_msgq* queue;
};

std::array<SeatQueueStatus, 3> g_seatPacketQueues = {
    SeatQueueStatus
    {
        .packetAvailable = false,
        .currentPacket = {},
        .queue = &g_packetQueue0
    },
    SeatQueueStatus
    {
        .packetAvailable = false,
        .currentPacket = {},
        .queue = &g_packetQueue1
    },
    SeatQueueStatus
    {
        .packetAvailable = false,
        .currentPacket = {},
        .queue = &g_packetQueue2
    }
};

struct UsbLinkContext : TransmitContext
{
    uint8_t index = 0;
    uint8_t simulatedPartnereCount = 1;
    std::array<SeatQueueStatus, 3>* seatQueues = &g_seatPacketQueues;
};

void usbLink_receiveHandler(std::span<const uint8_t> data, void*)
{
    for(uint8_t i = 0; i < data[0]; i++ )
    {
        const uint8_t packetOffset = packetSize * i + dataCountOffset;
        uint8_t seatIndex = data[packetOffset];
        auto commandPacket = data.subspan(packetOffset + 1, 16);
        k_msgq_put(g_seatPacketQueues[seatIndex].queue, commandPacket.data(), K_NO_WAIT);
    }
}

static void loadNextPacket(TransmitContext* ctx)
{
    (void)ctx;
    for (size_t i = 0; i < sizeof(g_seatPacketQueues)/sizeof(SeatQueueStatus); i++)
    {
        SeatQueueStatus* seatQueue = &g_seatPacketQueues[i];
        seatQueue->packetAvailable = (k_msgq_get(seatQueue->queue, seatQueue->currentPacket, K_NO_WAIT) == 0);
    }
}

static std::array<uint16_t, 3> usbLinkTransive(TransmitContext* ctx)
{   
    auto* usbCtx = static_cast<UsbLinkContext*>(ctx);
    std::array<uint16_t, 3> ret = {};

    bool packedShifted = false;
    for (uint8_t i = 0; i < usbCtx->simulatedPartnereCount; i++)
    {
        SeatQueueStatus* seatQueue = &g_seatPacketQueues[i];
        if (!seatQueue->packetAvailable)
        {
            ret[i] = 0x00;
            continue;
        }
        
        ret[i] = seatQueue->currentPacket[usbCtx->index];
        packedShifted = true;

        // TODO Why does this happen? Only observed on Reconnect and only from slaves -> master and is concistent, so no random flip
        if (usbCtx->index == 0 && (ret[i] == 0xFF02 || ret[i] == 0xFF06 || ret[i] == 0xFF07))
        {
            ret[i] = 0x5FFF;
        } 
        
        if (usbCtx->index == 7)
        {
            seatQueue->packetAvailable = false;
        }
    }

    if (packedShifted) usbCtx->index++; 
    if (usbCtx->index == 8) usbCtx->index = 0;
    return ret;
}

TransmitBehaviour usbLinkCommand(uint8_t otherPlayerCount)
{
    std::unique_ptr<UsbLinkContext> p = std::make_unique<UsbLinkContext>();

    for (size_t i = 0; i < sizeof(p->seatQueues->size()); i++)
    {
        SeatQueueStatus* seatQueue = &p->seatQueues->at(i);
        k_msgq_purge(seatQueue->queue);
        memset(seatQueue->currentPacket, 0, 16);
        seatQueue->packetAvailable = false;
    }
    p->index = 0;
    p->simulatedPartnereCount = otherPlayerCount;

    static TransmitBehaviour transive
    {
        .context = std::move(p),
        .transmitCallback = usbLinkTransive,
        .transmitDoneCallback = [](TransmitContext* ctx) 
        {
            loadNextPacket(ctx);
            return CommandState::resume; 
        }
    };

    return std::move(transive);
}