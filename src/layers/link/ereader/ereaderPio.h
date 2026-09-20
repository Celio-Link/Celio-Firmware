#include <zephyr/kernel.h>
#include <zephyr/drivers//misc/pio_rpi_pico/pio_rpi_pico.h>

#pragma once

/* SET-instruction pin values.
 *   bit 0 = SC  (GP0)       bit 1 = SI  (GP1, always input)
 *   bit 2 = SO  (GP2)       bit 3/4 = SD (GP3 or GP4)       */
#define PIO_SC          1   /* bit 0 */
#define PIO_SO          4   /* bit 2 */
#define PIO_SD_GBA      8   /* bit 3 — GBA cable, SD on GP3, set_count=4 */
#define PIO_SD_GBC      16  /* bit 4 — GBC cable, SD on GP4, set_count=5 */

/* --- GBA cable programs (SD on GP3) --- */

/* e-reader child: SO stays on GPIO LOW — no PIO_SO in set ops. */
RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_ereader_gba, 0, 19,
    (0xe000 | PIO_SD_GBA), 0xe080, 0xe02f, 0x2020,
    0xd701, 0x4e01, 0x0045, 0x8020,
    (0xe000 | PIO_SD_GBA),
    (0xe080 | PIO_SD_GBA),
    0xbf42, 0xe02f, 0x80a0, 0x80a0, 0xf000, 0x6e01, 0x004e,
    (0xf000 | PIO_SD_GBA),
    0xc000, 0xbf42);

RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_ereader_gbc, 0, 19,
    (0xe000 | PIO_SD_GBC), 0xe080, 0xe02f, 0x2020,
    0xd701, 0x4e01, 0x0045, 0x8020,
    (0xe000 | PIO_SD_GBC),
    (0xe080 | PIO_SD_GBC),
    0xbf42, 0xe02f, 0x80a0, 0x80a0, 0xf000, 0x6e01, 0x004e,
    (0xf000 | PIO_SD_GBC),
    0xc000, 0xbf42);

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//