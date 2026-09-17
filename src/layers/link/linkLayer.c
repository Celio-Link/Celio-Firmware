#include "linkLayer.h"
#include "./cableDetection/cableDetection.h"

#include <zephyr/kernel.h>

#include "hardware/pio.h"
#include "hardware/gpio.h"

#include <zephyr/drivers//misc/pio_rpi_pico/pio_rpi_pico.h>
#include <zephyr/drivers/pinctrl.h>

#define TX_RX_DONE_IRQ 0
#define TX_VALUE_IRQ 1

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

PINCTRL_DT_DEFINE(DT_NODELABEL(pio_link));

struct LinkPio
{
    PIO device;
    size_t id;
};

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void* g_receiveUserData = NULL;
static ReceiveHandler g_receiveCallback = NULL;

void* g_transmitUserData = NULL;
static TransmitHandler g_transmitCallback = NULL;

void* g_transiveDoneUserdata = NULL;
static TransiveDoneHandler g_transiveDoneCallback = NULL;

static struct LinkPio g_pio = { .device = NULL, .id = 0};

static pio_sm_config g_config = {};

static enum CableType g_configuredCable;

static uint32_t g_wordCount = 0;

uint16_t g_lastTxValue = 0x00;

#pragma push_macro("pio0")
#undef pio0
uint32_t SC_pin = DT_RPI_PICO_PIO_PIN_BY_NAME(DT_CHILD(DT_NODELABEL(pio0), piolink), default, 0, link_pins, 0);
uint32_t SI_pin = DT_RPI_PICO_PIO_PIN_BY_NAME(DT_CHILD(DT_NODELABEL(pio0), piolink), default, 0, link_pins, 1);
uint32_t SO_pin = DT_RPI_PICO_PIO_PIN_BY_NAME(DT_CHILD(DT_NODELABEL(pio0), piolink), default, 0, link_pins, 2);
uint32_t SD_GBA_pin = DT_RPI_PICO_PIO_PIN_BY_NAME(DT_CHILD(DT_NODELABEL(pio0), piolink), default, 0, link_pins, 3);
uint32_t SD_GBC_pin = DT_RPI_PICO_PIO_PIN_BY_NAME(DT_CHILD(DT_NODELABEL(pio0), piolink), default, 0, link_pins, 4);
#pragma pop_macro("pio0")

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

static void configureGBA();

static void configureGBC();

void assignGpioToPio();

static uint16_t reverse16Bit(uint16_t x);

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

static void pioIsr_tx(const void* arg)
{
    (void)arg;
    struct NextTransmit txValue = 
    {
        0xDEAD, 
        50000
    };

    if (g_transmitCallback) txValue = g_transmitCallback(g_transmitUserData);
    pio_sm_put(g_pio.device, g_pio.id, txValue.timingUs);
    pio_sm_put(g_pio.device, g_pio.id, txValue.value);
    g_lastTxValue = txValue.value;
    pio_interrupt_clear(g_pio.device, TX_VALUE_IRQ);
    return;
}

static void pioIsr_done(const void* arg)
{
    (void)arg;
    g_wordCount++;
    uint16_t rxData = pio_sm_get(g_pio.device, g_pio.id);
    rxData = reverse16Bit(rxData);
    if (g_receiveCallback) g_receiveCallback(rxData, g_receiveUserData);
    if (g_transiveDoneCallback) g_transiveDoneCallback(rxData, g_lastTxValue, g_transiveDoneUserdata);
    pio_interrupt_clear(g_pio.device, TX_RX_DONE_IRQ);
}


//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Interface
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_setTransmitCallback(TransmitHandler cb, void* userData) 
{
    g_transmitUserData = userData;
    g_transmitCallback = cb;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_setReceiveCallback(ReceiveHandler cb, void* userData) 
{
    g_receiveUserData = userData;
    g_receiveCallback = cb;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_setTransiveDoneCallback(TransiveDoneHandler cb, void* user_data)
{
    g_transiveDoneUserdata = user_data;
    g_transiveDoneCallback = cb;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_startTransive() {}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_disable()
{
    g_wordCount = 0;
    pio_sm_set_enabled(g_pio.device, g_pio.id, false);
    pio_clear_instruction_memory(g_pio.device);
    pio_sm_restart(g_pio.device, g_pio.id);
    pio_sm_clear_fifos(g_pio.device, g_pio.id);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_enable()
{
    pio_sm_set_enabled(g_pio.device, g_pio.id, true);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_configureProgram(const pio_program_t* prgramm, uint32_t warp, uint32_t wrapTarget)
{
    g_wordCount = 0;
    uint32_t offset = pio_add_program(g_pio.device, prgramm);
    sm_config_set_wrap(&g_config, offset + wrapTarget, offset + warp);
    assignGpioToPio();
    pio_sm_init(g_pio.device, g_pio.id, -1, &g_config);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

uint32_t link_getPin(enum LinkPin pin)
{
    switch(pin)
    {
        case SC: return SC_pin;
        case SI: return SI_pin;
        case SO: return SO_pin;
        case SD: return g_configuredCable == GBA ? SD_GBA_pin : SD_GBC_pin;
        case SD_GBA: return SD_GBA_pin;
        case SD_GBC: return SD_GBC_pin;
      break;
    }
    return 0;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

uint32_t link_receivedWordCount() { return g_wordCount; }

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_setPioPinDirs(uint32_t pin, enum LinkPinDir direction)
{
    bool isOut = (direction == PIN_DOR_OUT);
    pio_sm_set_consecutive_pindirs(g_pio.device, g_pio.id, pin, 1, isOut);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void link_configureCableType(enum CableType type)
{
    switch(type)
    {
        case GBA: configureGBA(); break;
        case GBC: configureGBC(); break;
    }
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

uint8_t link_readPartnerPins()
{
    const uint32_t in = sio_hw->gpio_in;
    uint8_t mask = 0;
    if (!(in & (1u << 0))) mask |= 0x01;
    if (in & (1u << 1)) mask |= 0x02;
    if (in & (1u << 2)) mask |= 0x04;
    if (in & (1u << 3)) mask |= 0x08;
    if (in & (1u << 4)) mask |= 0x10;
    return mask;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void assignGpioToPio()
{
    pio_gpio_init(g_pio.device, SC_pin);
    pio_gpio_init(g_pio.device, SI_pin);
    pio_gpio_init(g_pio.device, SO_pin);
    pio_gpio_init(g_pio.device, SD_GBA_pin);
    pio_gpio_init(g_pio.device, SD_GBC_pin);
}

uint16_t reverse16Bit(uint16_t x)
{
	x = ((x & 0x5555) << 1) | ((x & 0xAAAA) >> 1);
	x = ((x & 0x3333) << 2) | ((x & 0xCCCC) >> 2);
	x = ((x & 0x0F0F) << 4) | ((x & 0xF0F0) >> 4);
	return (x << 8) | (x >> 8);
}

static void configureGBA()
{
    g_configuredCable = GBA;
    sm_config_set_set_pins(&g_config, SC_pin, 4);
    sm_config_set_out_pins(&g_config, SD_GBA_pin, 1);
    sm_config_set_in_pins(&g_config, SD_GBA_pin);
    sm_config_set_jmp_pin(&g_config, SD_GBA_pin);

    pio_sm_init(g_pio.device, g_pio.id, -1, &g_config);
}

static void configureGBC()
{
    g_configuredCable = GBC;
    sm_config_set_set_pins(&g_config, SC_pin, 5);
    sm_config_set_out_pins(&g_config, SD_GBC_pin, 1);
    sm_config_set_in_pins(&g_config, SD_GBC_pin);
    sm_config_set_jmp_pin(&g_config, SD_GBC_pin);

    pio_sm_init(g_pio.device, g_pio.id, -1, &g_config);
}

static int init()
{
    const struct device* dev = DEVICE_DT_GET(DT_PROP(DT_NODELABEL(pio_link), pio));

    const struct pinctrl_dev_config* config = PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(pio_link));
    g_pio.device = pio_rpi_pico_get_pio(dev);

    pio_rpi_pico_allocate_sm(dev, &g_pio.id);

    IRQ_CONNECT(PIO0_IRQ_0 , 0, pioIsr_done, NULL, 0);
    IRQ_CONNECT(PIO0_IRQ_1 , 0, pioIsr_tx, NULL, 0);

    irq_enable(PIO0_IRQ_0);
    irq_enable(PIO0_IRQ_1);

    pio_set_irq0_source_enabled(g_pio.device, pis_interrupt0, true);
    pio_set_irq1_source_enabled(g_pio.device, pis_interrupt1, true);

    int ret = pinctrl_apply_state(config, PINCTRL_STATE_DEFAULT);

    link_disable();

    assignGpioToPio();
    
    configureGBC();

    gpio_pull_up(link_getPin(SD));

    g_config = pio_get_default_sm_config();

    sm_config_set_out_shift(&g_config, true, false, 0);
    sm_config_set_in_shift(&g_config, false, false, 0);

    sm_config_set_clkdiv(&g_config, 67.816f); // ~540 ns per inst, 16 inst equal baud 115200

    pio_sm_init(g_pio.device, g_pio.id, -1, &g_config);

    return ret;
}

SYS_INIT(init, APPLICATION, 1);
