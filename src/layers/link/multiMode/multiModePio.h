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

RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_gba, 0, 19,
    0xe008, //  0: set    pins, 8                    
    0xe084, //  1: set    pindirs, 4                 
    0xe02f, //  2: set    x, 15                      
    0x2020, //  3: wait   0 pin, 0                   
    0xd701, //  4: irq    nowait 1               [23]
    0x4e01, //  5: in     pins, 1                [14]
    0x0045, //  6: jmp    x--, 5                     
    0x8020, //  7: push   block                      
    0xe008, //  8: set    pins, 8                    
    0xe08c, //  9: set    pindirs, 12                
    0xbe42, // 10: nop                           [30]
    0xe02f, // 11: set    x, 15                      
    0x80a0, // 12: pull   block                      
    0x80a0, // 13: pull   block                      
    0xf000, // 14: set    pins, 0                [16]
    0x6e01, // 15: out    pins, 1                [14]
    0x004f, // 16: jmp    x--, 15                    
    0xf008, // 17: set    pins, 8                [16]
    0xc000, // 18: irq    nowait 0                   
    0xbf42, // 19: nop                           [31]
);

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

RPI_PICO_PIO_DEFINE_PROGRAM(pio_slave_gbc, 0, 19,
    (0xe000 | PIO_SD_GBC), 
    0xe084, 
    0xe02f, 
    0x2020,
    0xd701, 
    0x4e01, 
    0x0045, 
    0x8020,
    (0xe000 | PIO_SD_GBC),
    (0xe080 | PIO_SO | PIO_SD_GBC),
    0xbf42, 
    0xe02f, 
    0x80a0,
    0x80a0,
    0xf000, 
    0x6e01, 
    0x004e,
    (0xf000 | PIO_SD_GBC),
    0xc000, 
    0xbf42);