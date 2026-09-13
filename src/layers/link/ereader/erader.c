#include "ereader.h"
#include "../linkLayer.h"
#include "../cableDetection/cableDetection.h"

#include <zephyr/kernel.h>

#include "hardware/gpio.h"

#include <zephyr/drivers//misc/pio_rpi_pico/pio_rpi_pico.h>
#include <zephyr/drivers/pinctrl.h>

/* SET-instruction pin values.
 *   bit 0 = SC  (GP0)       bit 1 = SI  (GP1, always input)
 *   bit 2 = SO  (GP2)       bit 3/4 = SD (GP3 or GP4)       */
#define PIO_SC          1   /* bit 0 */
#define PIO_SO          4   /* bit 2 */
#define PIO_SD_GBA      8   /* bit 3 — GBA cable, SD on GP3, set_count=4 */
#define PIO_SD_GBC      16  /* bit 4 — GBC cable, SD on GP4, set_count=5 */

/* --- GBA cable programs (SD on GP3) --- */

/* e-reader child: SO stays on GPIO LOW — no PIO_SO in set ops. */
RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_ereader_gba, 0, 18,
    (0xe000 | PIO_SD_GBA), 0xe080, 0xe02f, 0x2020,
    0xd701, 0x4e01, 0x0045, 0x8020,
    (0xe000 | PIO_SD_GBA),
    (0xe080 | PIO_SD_GBA),
    0xbf42, 0xe02f, 0x80a0, 0xf000, 0x6e01, 0x004e,
    (0xf000 | PIO_SD_GBA),
    0xc000, 0xbf42);

RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_ereader_gbc, 0, 18,
    (0xe000 | PIO_SD_GBC), 0xe080, 0xe02f, 0x2020,
    0xd701, 0x4e01, 0x0045, 0x8020,
    (0xe000 | PIO_SD_GBC),
    (0xe080 | PIO_SD_GBC),
    0xbf42, 0xe02f, 0x80a0, 0xf000, 0x6e01, 0x004e,
    (0xf000 | PIO_SD_GBC),
    0xc000, 0xbf42);

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

static void configureEreaderSlavePio(void);

static void releaseInactiveSdPin(void);

static void assertBothSdPartnerGpio(void);

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

/* Undo the SIO overrides the e-reader flows leave behind (SD pins driven high
 * for partner presence, SO forced low as e-reader child) so later modes start
 * from cable-idle pin state; the PIO paths only reclaim the pins they use. */
void ereader_releasePartnerPins(void)
{
    uint32_t SO_pin = link_getPin(SO);
    uint32_t sd_gba = link_getPin(SD_GBA);
    uint32_t sd_gbc = link_getPin(SD_GBC);
    gpio_init(SO_pin); gpio_set_dir(SO_pin, GPIO_IN); gpio_disable_pulls(SO_pin);
    gpio_init(sd_gba); gpio_set_dir(sd_gba, GPIO_IN); gpio_disable_pulls(sd_gba);
    gpio_init(sd_gbc); gpio_set_dir(sd_gbc, GPIO_IN); gpio_disable_pulls(sd_gbc);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void ereader_configurePartnerPresence(void)
{
    link_disable();
    uint32_t SO_pin = link_getPin(SO);
    uint32_t SC_pin = link_getPin(SC);
    uint32_t SI_pin = link_getPin(SI);
    gpio_init(SC_pin); gpio_pull_up(SC_pin); gpio_set_dir(SC_pin, GPIO_IN);
    gpio_init(SI_pin); gpio_pull_up(SI_pin); gpio_set_dir(SI_pin, GPIO_IN);
    gpio_init(SO_pin); gpio_set_dir(SO_pin, GPIO_OUT); gpio_put(SO_pin, 0);
    assertBothSdPartnerGpio();
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void ereader_configureEreaderSlave(void)
{
    link_disable();

    uint32_t SC_pin = link_getPin(SC);
    uint32_t SI_pin = link_getPin(SI);
    uint32_t SO_pin = link_getPin(SO);

    gpio_init(SC_pin); gpio_pull_up(SC_pin); gpio_set_dir(SC_pin, GPIO_IN);
    gpio_init(SI_pin); gpio_pull_up(SI_pin); gpio_set_dir(SI_pin, GPIO_IN);
    gpio_init(SO_pin); gpio_set_dir(SO_pin, GPIO_OUT); gpio_put(SO_pin, 0);
    releaseInactiveSdPin();

    configureEreaderSlavePio();
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

static void configureEreaderSlavePio(void)
{
    link_disable();

    switch (cableDetection_getDetectedCableType())
    {
        case GBC:
            link_configureProgram(
                RPI_PICO_PIO_GET_PROGRAM(pio_slave_ereader_gbc), 
                RPI_PICO_PIO_GET_WRAP(pio_slave_ereader_gbc), 
                RPI_PICO_PIO_GET_WRAP_TARGET(pio_slave_ereader_gbc)
            );

        case GBA:
            link_configureProgram(
                RPI_PICO_PIO_GET_PROGRAM(pio_slave_ereader_gba), 
                RPI_PICO_PIO_GET_WRAP(pio_slave_ereader_gba), 
                RPI_PICO_PIO_GET_WRAP_TARGET(pio_slave_ereader_gba)
            );
    }

    gpio_init(link_getPin(SO));
    gpio_set_dir(link_getPin(SO), GPIO_OUT);
    gpio_put(link_getPin(SO), 0);

    link_setPioPinDirs(link_getPin(SC), GPIO_IN);
    link_setPioPinDirs(link_getPin(SI), GPIO_IN);

    link_enable();
}

static void assertBothSdPartnerGpio(void)
{
    uint32_t sd_gba = link_getPin(SD_GBA);
    uint32_t sd_gbc = link_getPin(SD_GBC);
    gpio_init(sd_gba); gpio_set_dir(sd_gba, GPIO_OUT); gpio_put(sd_gba, 1);
    gpio_init(sd_gbc); gpio_set_dir(sd_gbc, GPIO_OUT); gpio_put(sd_gbc, 1);
}



static void releaseInactiveSdPin(void)
{
    const bool gbc = cableDetection_getDetectedCableType();

    const uint32_t inactive = gbc ? link_getPin(SD_GBC) : link_getPin(SD_GBA);
        
    gpio_init(inactive);
    gpio_set_dir(inactive, GPIO_IN);
    gpio_disable_pulls(inactive);
}