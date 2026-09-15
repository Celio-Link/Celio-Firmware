#include "multiMode.h"
#include "./../linkLayer.h"
#include "./../cableDetection/cableDetection.h"

#include <zephyr/kernel.h>

#include <zephyr/drivers//misc/pio_rpi_pico/pio_rpi_pico.h>
#include <zephyr/drivers/pinctrl.h>

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

/* SET-instruction pin values.
 *   bit 0 = SC  (GP0)       bit 1 = SI  (GP1, always input)
 *   bit 2 = SO  (GP2)       bit 3/4 = SD (GP3 or GP4)       */
#define PIO_SC          1   /* bit 0 */
#define PIO_SO          4   /* bit 2 */
#define PIO_SD_GBA      8   /* bit 3 — GBA cable, SD on GP3, set_count=4 */
#define PIO_SD_GBC      16  /* bit 4 — GBC cable, SD on GP4, set_count=5 */

/* --- GBA cable programs (SD on GP3) --- */

RPI_PICO_PIO_DEFINE_PROGRAM(pio_master_gba, 0, 26,
    (0xe080 | PIO_SC | PIO_SO | PIO_SD_GBA), //  0: set    pindirs, SC|SO|SD=out
    (0xe000 | PIO_SC | PIO_SO | PIO_SD_GBA), //  1: set    pins, SC|SO|SD=HIGH
    0x0082, 0xc001, 0xe03e, 0x0245, 0x80a0, 0xa047, 0xe02f, 0x80a0,
    (0xe000 | PIO_SO | PIO_SD_GBA),          // 10: set    pins, SO|SD=HIGH
    0xef04, 0x6e01, 0x004c,
    (0xef00 | PIO_SO | PIO_SD_GBA),          // 14: set    pins, SO|SD=HIGH   [15]
    (0xe000 | PIO_SD_GBA),                   // 15: set    pins, SD=HIGH
    0xe085, 0xe03f, 0x1f53, 0x0035, 0x00d2,
    0xf62f, 0x4e01, 0x0056, 0x9020,
    (0xfe00 | PIO_SO | PIO_SD_GBA),          // 25: set    pins, SO|SD=HIGH   [30]
    0xc000);

RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_gba, 0, 18,
    (0xe000 | PIO_SD_GBA), 0xe084, 0xe02f, 0x2020,
    0xd701, 0x4e01, 0x0045, 0x8020,
    (0xe000 | PIO_SD_GBA),
    (0xe080 | PIO_SO | PIO_SD_GBA),
    0xbf42, 0xe02f, 0x80a0, 0xf000, 0x6e01, 0x004e,
    (0xf000 | PIO_SD_GBA),
    0xc000, 0xbf42);

/* --- GBC cable programs (SD on GP4) --- */

RPI_PICO_PIO_DEFINE_PROGRAM(pio_master_gbc, 0, 26,
    (0xe080 | PIO_SC | PIO_SO | PIO_SD_GBC), //  0: set    pindirs, SC|SO|SD=out
    (0xe000 | PIO_SC | PIO_SO | PIO_SD_GBC), //  1: set    pins, SC|SO|SD=HIGH
    0x0082, 0xc001, 0xe03e, 0x0245, 0x80a0, 0xa047, 0xe02f, 0x80a0,
    (0xe000 | PIO_SO | PIO_SD_GBC),          // 10: set    pins, SO|SD=HIGH
    0xef04, 0x6e01, 0x004c,
    (0xef00 | PIO_SO | PIO_SD_GBC),          // 14: set    pins, SO|SD=HIGH   [15]
    (0xe000 | PIO_SD_GBC),                   // 15: set    pins, SD=HIGH
    0xe085, 0xe03f, 0x1f53, 0x0035, 0x00d2,
    0xf62f, 0x4e01, 0x0056, 0x9020,
    (0xfe00 | PIO_SO | PIO_SD_GBC),          // 25: set    pins, SO|SD=HIGH   [30]
    0xc000);

RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_gbc, 0, 18,
    (0xe000 | PIO_SD_GBC), 0xe084, 0xe02f, 0x2020,
    0xd701, 0x4e01, 0x0045, 0x8020,
    (0xe000 | PIO_SD_GBC),
    (0xe080 | PIO_SO | PIO_SD_GBC),
    0xbf42, 0xe02f, 0x80a0, 0xf000, 0x6e01, 0x004e,
    (0xf000 | PIO_SD_GBC),
    0xc000, 0xbf42);

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

static void configureMaster();

static void configureSlave();

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Interface
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void multiMode_selectMode(enum MultiMode mode)
{
    switch(mode)
    {

    case MASTER: configureMaster(); break;
    case SLAVE: configureSlave(); break;
    case DISABLED: link_disable(); break;
      break;
    }
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

static void configureMaster()
{
    link_disable();

    enum CableType cableType = cableDetection_getDetectedCableType();
    link_configureCableType(cableType);
    switch (cableType)
    {
        case GBC:
            link_configureProgram(
                RPI_PICO_PIO_GET_PROGRAM(pio_master_gbc), 
                RPI_PICO_PIO_GET_WRAP(pio_master_gbc), 
                RPI_PICO_PIO_GET_WRAP_TARGET(pio_master_gbc)
            );

        case GBA:
            link_configureProgram(
                RPI_PICO_PIO_GET_PROGRAM(pio_master_gba), 
                RPI_PICO_PIO_GET_WRAP(pio_master_gba), 
                RPI_PICO_PIO_GET_WRAP_TARGET(pio_master_gba)
            );
    }
    
	link_enable();
}

static void configureSlave(void)
{
    link_disable();

    enum CableType cableType = cableDetection_getDetectedCableType();
    link_configureCableType(cableType);

    switch (cableType)
    {
        case GBC:
            link_configureProgram(
                RPI_PICO_PIO_GET_PROGRAM(pio_slave_gbc), 
                RPI_PICO_PIO_GET_WRAP(pio_slave_gbc), 
                RPI_PICO_PIO_GET_WRAP_TARGET(pio_slave_gbc)
            );

        case GBA:
            link_configureProgram(
                RPI_PICO_PIO_GET_PROGRAM(pio_slave_gba), 
                RPI_PICO_PIO_GET_WRAP(pio_slave_gba), 
                RPI_PICO_PIO_GET_WRAP_TARGET(pio_slave_gba)
            );
    }
    
	link_enable();

    link_setPioPinDirs(link_getPin(SC), GPIO_IN);
    link_setPioPinDirs(link_getPin(SI), GPIO_IN);
    link_setPioPinDirs(link_getPin(SO), GPIO_OUT);
}