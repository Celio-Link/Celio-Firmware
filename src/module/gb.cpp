#include "gb.hpp"
#include "../linkStatus.hpp"
#include "../hardware.hpp"
#include "../layers/usbLayer.hpp"

extern "C"
{
    #include "../layers/gbLinkLayer.h"
    #include "../layers/linkLayer.h"
    #include "hardware/gpio.h"
    #include "hardware/timer.h"
}

// --- Pin Definitions ---
#define PIN_SCK  0
#define PIN_SIN  1
#define PIN_SOUT 2
#define PIN_SD   3

// Max bytes per single USB transfer
static constexpr uint8_t MAX_TRANSFER_BYTES = 0x40;

// --- Shared state for USB data reception ---
static uint8_t g_rxBuf[MAX_TRANSFER_BYTES * 2];
static uint32_t g_rxCount = 0;
static bool g_dataReady = false;

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// USB Data Handler
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void GBModule::usbDataHandler(std::span<const uint8_t> data, void* userData)
{
    // Copy received data into shared buffer
    uint32_t count = data.size();
    if (count > sizeof(g_rxBuf)) count = sizeof(g_rxBuf);
    
    for (uint32_t i = 0; i < count; i++) {
        g_rxBuf[i] = data[i];
    }
    g_rxCount = count;
    g_dataReady = true;
    
    // Signal the main loop that data is available
    GBModule* self = static_cast<GBModule*>(userData);
    k_sem_give(&self->m_dataSemaphore);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Module Interface
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

bool GBModule::canHandle(uint8_t command)
{
    return (command & 0xF0) == 0x30;
}

void GBModule::receiveCommand(std::span<const uint8_t> command)
{
    switch (static_cast<GBCommand>(command[0]))
    {
        case GBCommand::SetTimingConfig:
            if (command.size() >= 5) {
                m_usBetweenTransfer = (command[1] << 0) | (command[2] << 8) | (command[3] << 16);
                m_bytesPerTransfer = command[4];
                if (m_bytesPerTransfer > MAX_TRANSFER_BYTES)
                    m_bytesPerTransfer = MAX_TRANSFER_BYTES;
            }
            break;

        case GBCommand::EnterPrinterMode:
            m_subMode = SubMode::printer;
            // Signal the normal mode loop to exit so we can switch
            k_sem_give(&m_dataSemaphore);
            break;

        case GBCommand::ExitPrinterMode:
            m_subMode = SubMode::normal;
            break;

        default:
            break;
    }
}

void GBModule::cancel()
{
    m_cancel = true;
    m_subMode = SubMode::normal;
    k_sem_give(&m_dataSemaphore);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Execute — Main Entry Point
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void GBModule::execute()
{
    m_cancel = false;

    // Disable GBA PIO link before initializing GB SPI
    link_changeMode(DISABLED);

    // Initialize GB 8-bit SPI PIO on pio0
    gb_link_init();

    // Set LED to blue (active/connected)
    Hardware::getInstance().setLED(0, 0, 5, true);

    // Set USB data handler
    UsbLayer::getInstance().setReceiveDataHandler(usbDataHandler, this);

    sendLinkStatus(LinkStatus::GBModeActive);

    while (!m_cancel)
    {
        switch (m_subMode)
        {
            case SubMode::normal:
                normalModeLoop();
                break;

            case SubMode::printer:
                sendLinkStatus(LinkStatus::GBPrinterModeActive);
                Hardware::getInstance().setLED(5, 0, 5, true); // Purple for printer
                
                // Disable PIO SPI for GPIO bit-bang printer mode
                gb_link_deinit();
                
                printerModeLoop();
                
                // Re-enable PIO SPI
                gb_link_init();
                
                Hardware::getInstance().setLED(0, 0, 5, true); // Blue (active)
                m_subMode = SubMode::normal;
                sendLinkStatus(LinkStatus::GBModeActive);
                break;
        }
    }

    // Cleanup: deinit GB PIO so GBA can reclaim pio0
    gb_link_deinit();

    // Clear USB data handler
    UsbLayer::getInstance().setReceiveDataHandler(nullptr, nullptr);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Normal SPI Passthrough Mode
// Ported from handle_input_data() in gb-link-firmware-reconfigurable/main.c
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void GBModule::normalModeLoop()
{
    while (!m_cancel && m_subMode == SubMode::normal)
    {
        // Wait for USB data
        k_sem_take(&m_dataSemaphore, K_FOREVER);
        
        if (m_cancel || m_subMode != SubMode::normal) break;
        if (!g_dataReady) continue;
        g_dataReady = false;

        uint32_t count = g_rxCount;
        if (count == 0) continue;

        // Zero-fill remainder of buffer (matching reconfigurable firmware behavior)
        for (uint32_t i = count; i < (MAX_TRANSFER_BYTES * 2); i++) {
            g_rxBuf[i] = 0;
        }

        // Perform SPI transfer: send data to Game Boy, receive response
        uint8_t txBuf[MAX_TRANSFER_BYTES * 2];
        uint8_t rxBuf[MAX_TRANSFER_BYTES * 2];
        
        for (uint32_t i = 0; i < count; i++) {
            txBuf[i] = g_rxBuf[i];
        }

        // Transfer in chunks matching bytes_per_transfer setting
        // Match old firmware: delay AFTER EVERY chunk including the last
        uint32_t totalProcessed = 0;
        while (totalProcessed < count) {
            uint32_t transferable = m_bytesPerTransfer;
            if (count - totalProcessed < transferable) {
                transferable = count - totalProcessed;
            }
            
            gb_link_transfer(txBuf + totalProcessed, rxBuf + totalProcessed, 
                           transferable, 0);
            totalProcessed += transferable;

            // Delay after EVERY chunk (matches old firmware exactly)
            if (m_usBetweenTransfer > 0) {
                busy_wait_us(m_usBetweenTransfer);
            }
        }

        // Send response back via USB
        UsbLayer::getInstance().sendData(
            std::span<const uint8_t>(rxBuf, totalProcessed));
    }
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Printer Mode — GPIO bit-bang SPI slave with protocol handling
// Ported from printer_mode_loop() in gb-link-firmware-reconfigurable/main.c
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

// Printer protocol states
enum PrinterState {
    GB_WAIT_FOR_SYNC_1,
    GB_WAIT_FOR_SYNC_2,
    GB_COMMAND,
    GB_COMPRESSION_INDICATOR,
    GB_LEN_LOWER,
    GB_LEN_HIGHER,
    GB_DATA,
    GB_CHECKSUM_1,
    GB_CHECKSUM_2,
    GB_SEND_DEVICE_ID,
    GB_SEND_STATUS
};

void GBModule::printerModeLoop()
{
    // Reconfigure pins for GPIO bit-bang mode (SPI slave)
    gpio_init(PIN_SCK);
    gpio_init(PIN_SIN);
    gpio_init(PIN_SOUT);

    gpio_set_dir(PIN_SCK, GPIO_IN);
    gpio_set_dir(PIN_SIN, GPIO_IN);
    gpio_set_dir(PIN_SOUT, GPIO_OUT);
    gpio_put(PIN_SOUT, 0);

    // Bit-level variables
    uint8_t received_data = 0;
    uint8_t received_bits = 0;
    uint8_t send_data = 0x00;
    bool bit_synced = false;

    // Protocol state machine
    PrinterState state = GB_WAIT_FOR_SYNC_1;
    uint8_t command = 0;
    uint16_t length = 0;
    uint16_t data_count = 0;
    uint8_t printer_status = 0x00;  // 0x00 = OK, ready

    // Timeout for disconnection detection
    uint32_t idle_count = 0;
    const uint32_t IDLE_TIMEOUT = 10000000;
    const uint32_t PRINT_ABORT_TIMEOUT = 2000000;

    // Send start marker to web app
    uint8_t start_marker = 0xFF;
    UsbLayer::getInstance().sendData(
        std::span<const uint8_t>(&start_marker, 1));

    while (m_subMode == SubMode::printer && !m_cancel)
    {
        // Wait for clock to go low (with timeout)
        idle_count = 0;
        uint32_t timeout = (state != GB_WAIT_FOR_SYNC_1 && state != GB_WAIT_FOR_SYNC_2)
                           ? PRINT_ABORT_TIMEOUT : IDLE_TIMEOUT;

        while (gpio_get(PIN_SCK)) {
            idle_count++;
            if (idle_count > timeout) {
                if (m_cancel || m_subMode != SubMode::printer) {
                    goto exit_printer;
                }

                // If we timed out mid-packet, the print was aborted
                if (state != GB_WAIT_FOR_SYNC_1 && state != GB_WAIT_FOR_SYNC_2) {
                    // Reset state machine
                    state = GB_WAIT_FOR_SYNC_1;
                    bit_synced = false;
                    received_data = 0;
                    received_bits = 0;
                    send_data = 0x00;

                    // Send "ABORTPRINT" to browser
                    const char* abort_marker = "ABORTPRINT";
                    UsbLayer::getInstance().sendData(
                        std::span<const uint8_t>(
                            reinterpret_cast<const uint8_t*>(abort_marker), 10));
                    break;
                }

                idle_count = 0;
            }
        }

        // Clock is LOW — output our bit (LSB first)
        gpio_put(PIN_SOUT, send_data & 0x1);
        send_data = send_data >> 1;

        // Wait for clock to go high
        while (!gpio_get(PIN_SCK)) {}

        // Clock is HIGH — sample input bit (MSB first)
        received_data = (received_data << 1) | (gpio_get(PIN_SIN) & 0x1);

        // Bit sync detection — look for 0x88 pattern
        if (!bit_synced) {
            if (received_data != 0x88) {
                continue;
            } else {
                received_bits = 8;
                bit_synced = true;
            }
        } else {
            received_bits++;
        }

        // Check if we have a complete byte
        if (received_bits != 8) {
            continue;
        }

        // We have a complete byte — process it
        uint8_t byte = received_data;
        received_data = 0;
        received_bits = 0;

        // Protocol state machine
        switch (state) {
            case GB_WAIT_FOR_SYNC_1:
                if (byte == 0x88) {
                    state = GB_WAIT_FOR_SYNC_2;
                }
                send_data = 0x00;
                break;

            case GB_WAIT_FOR_SYNC_2:
                if (byte == 0x33) {
                    state = GB_COMMAND;
                } else {
                    state = GB_WAIT_FOR_SYNC_1;
                    bit_synced = false;
                }
                send_data = 0x00;
                break;

            case GB_COMMAND:
                command = byte;
                state = GB_COMPRESSION_INDICATOR;
                send_data = 0x00;
                UsbLayer::getInstance().sendData(
                    std::span<const uint8_t>(&byte, 1));
                break;

            case GB_COMPRESSION_INDICATOR:
                state = GB_LEN_LOWER;
                send_data = 0x00;
                UsbLayer::getInstance().sendData(
                    std::span<const uint8_t>(&byte, 1));
                break;

            case GB_LEN_LOWER:
                length = byte;
                state = GB_LEN_HIGHER;
                send_data = 0x00;
                break;

            case GB_LEN_HIGHER:
            {
                length |= ((uint16_t)byte << 8);
                data_count = 0;

                if (length > 0) {
                    state = GB_DATA;
                } else {
                    state = GB_CHECKSUM_1;
                }
                send_data = 0x00;

                uint8_t len_bytes[2] = {
                    static_cast<uint8_t>(length & 0xFF),
                    static_cast<uint8_t>((length >> 8) & 0xFF)
                };
                UsbLayer::getInstance().sendData(
                    std::span<const uint8_t>(len_bytes, 2));
                break;
            }

            case GB_DATA:
                data_count++;
                UsbLayer::getInstance().sendData(
                    std::span<const uint8_t>(&byte, 1));
                if (data_count >= length) {
                    state = GB_CHECKSUM_1;
                }
                send_data = 0x00;
                break;

            case GB_CHECKSUM_1:
                state = GB_CHECKSUM_2;
                send_data = 0x00;
                break;

            case GB_CHECKSUM_2:
                state = GB_SEND_DEVICE_ID;
                send_data = 0x81;  // Printer device ID
                break;

            case GB_SEND_DEVICE_ID:
                state = GB_SEND_STATUS;
                send_data = printer_status;  // 0x00 = OK
                break;

            case GB_SEND_STATUS:
                state = GB_WAIT_FOR_SYNC_1;
                bit_synced = false;
                send_data = 0x00;

                if (command == 0x02) {  // PRINT command
                    uint8_t print_marker = 0xFE;
                    UsbLayer::getInstance().sendData(
                        std::span<const uint8_t>(&print_marker, 1));
                }
                break;
        }
    }

exit_printer:
    // Re-initialize GPIO for PIO SPI mode will happen via gb_link_init() in execute()
    return;
}
