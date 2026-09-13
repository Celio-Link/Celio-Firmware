#include "cableDetection.h"
#include "./../linkLayer.h"

#include <zephyr/kernel.h>

#include "hardware/gpio.h"

#include <zephyr/drivers//misc/pio_rpi_pico/pio_rpi_pico.h>
#include <zephyr/drivers/pinctrl.h>


/* Detect cable type by reading GP1 (SI pin).
 * GBA cable: GP1 hardwired to GND in cable — reads LOW, always
 * GBC cable: GP1 connected to GBA SO or floating — pull-up → reads HIGH
 *
 * A GBA that is already in MULTI link mode drives SO LOW, so on a GBC cable
 * an instantaneous sample can misread as a GBA cable. Sample over a window:
 * a hardwired ground can never read high, so any high sample proves GBC.
 * (A steadily driven-low SO still misreads; the host's cable-flip fallback
 * covers that case.) */
#define CABLE_DETECT_SETTLE_ITERS   1000    /* ~10us */
#define CABLE_DETECT_SAMPLE_ITERS   200000  /* ~2ms */

static uint8_t g_cableOverride = CABLE_AUTO;

static bool g_gbcCableConnected = false;

static bool g_enableWatchdog = false;

static volatile uint8_t g_watchdog_flips_left = 0;

bool useGbcCable(void);

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//
// Interface
//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

/* Force the SD pin path opposite to the currently effective one (wrong-pin
 * recovery). Cleared back to auto by the next SetCableOverride. */
void cableDetection_flipSdPinPath(void)
{
    g_cableOverride = useGbcCable() ? CABLE_FORCE_GBA : CABLE_FORCE_GBC;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

/* Sample even when forced — clients may return the session to auto later. */
void cableDetection_detectCableType(void)
{
    const uint32_t si_pin = link_getPin(SI);

    gpio_set_function(si_pin, GPIO_FUNC_SIO);
    gpio_set_dir(si_pin, GPIO_IN);
    gpio_pull_up(si_pin);
    for (volatile int i = 0; i < CABLE_DETECT_SETTLE_ITERS; i++);  /* settle ~10us */
    g_gbcCableConnected = false;
    for (volatile int i = 0; i < CABLE_DETECT_SAMPLE_ITERS; i++)  /* sample ~2ms */
    {
        if (gpio_get(si_pin)) { g_gbcCableConnected = true; break; }
    }
    gpio_set_function(si_pin, GPIO_FUNC_PIO0);
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

enum CableType cableDetection_getDetectedCableType(void)
{
    return useGbcCable() ? GBC : GBA;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

void cableDetection_setCableOverride(uint8_t mode)
{
    if (mode > CABLE_FORCE_GBC) mode = CABLE_AUTO;
    g_cableOverride = mode;
}

//-////////////////////////////////////////////////////////////////////////////////////////////////////////-//

bool useGbcCable(void)
{
    if (g_cableOverride == CABLE_FORCE_GBA) return false;
    if (g_cableOverride == CABLE_FORCE_GBC) return true;
    return g_gbcCableConnected;
}


bool tryWrongPinRecovery(void)
{
    const unsigned int key = irq_lock();
    bool recovered = false;

    // only in slave mode for pkmn, 
    if (g_slave_kind == CLASSIC && link_receivedWordCount() == 0 && g_watchdog_flips_left > 0)
    {
        g_watchdog_flips_left--;
        cableDetection_flipSdPinPath();
        //link_configureSlave(); //?
        recovered = true;
    }
    irq_unlock(key);
    return recovered;
}

/* Wrong-pin watchdog for classic slave modes (trade emu, GBA link relay, AW
 * slave). Cable detection is a heuristic and can pick the wrong SD pin (GBA
 * already in link mode at detect time, or cable plugged in afterwards). The
 * partner clocking SC while zero words arrive is proof of the wrong pin — SC
 * is the same pin for both cable types — so after sustained evidence the
 * watchdog flips the SD path and reconfigures, once per armed session.
 * Requiring both SC levels (a real toggle) rejects a stuck line, and any
 * received word disarms the watchdog until the next configure. */
static void wrongPinWatchdogThread(void* a, void* b, void* c)
{
    (void)a; (void)b; (void)c;
    bool scSeenLow = false;
    bool scSeenHigh = false;
    int ticks = 0;
    int clockingIntervals = 0;

    for (;;)
    {
        if (!g_enableWatchdog || g_watchdog_flips_left == 0)
        {
            scSeenLow = scSeenHigh = false;
            ticks = 0;
            clockingIntervals = 0;
            k_sleep(K_MSEC(100));
            continue;
        }

        k_sleep(K_MSEC(1));
        if (link_readPartnerPins() & 0x01) scSeenLow = true;
        else scSeenHigh = true;

        if (++ticks < 550) continue;
        ticks = 0;

        const bool scToggled = scSeenLow && scSeenHigh;
        scSeenLow = scSeenHigh = false;

        if (link_receivedWordCount() != 0)
        {
            clockingIntervals = 0;
            continue;
        }
        if (scToggled) clockingIntervals++;

        if (clockingIntervals >= 4)
        {
            clockingIntervals = 0;
            tryWrongPinRecovery();
        }
    }
}

K_THREAD_DEFINE(wrongPinWatchdog_tid, 768, wrongPinWatchdogThread,
                NULL, NULL, NULL, 12, 0, 0);