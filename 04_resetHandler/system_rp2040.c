#include <stdint.h>

// Define constants related to clocks
#define XOSC            (12000000)  // Crystal Oscillator Frequency

// Define necessary register addresses
// RESETS
#define RESETS_BASE                 (0x4000c000)
#define RESETS_RESET                (*(volatile uint32_t *) (RESETS_BASE + 0x000))
#define RESETS_RESET_DONE           (*(volatile uint32_t *) (RESETS_BASE + 0x008))
// XOSC
#define XOSC_BASE                   (0x40024000)
#define XOSC_CTRL                   (*(volatile uint32_t *) (XOSC_BASE + 0x000))
#define XOSC_STATUS                 (*(volatile uint32_t *) (XOSC_BASE + 0x004))
// PLL_SYS
#define PLL_SYS_BASE                (0x40028000)
#define PLL_SYS_CS                  (*(volatile uint32_t *) (PLL_SYS_BASE + 0x000))
#define PLL_SYS_PWR                 (*(volatile uint32_t *) (PLL_SYS_BASE + 0x004))
#define PLL_SYS_FBDIV_INT           (*(volatile uint32_t *) (PLL_SYS_BASE + 0x008))
#define PLL_SYS_PRIM                (*(volatile uint32_t *) (PLL_SYS_BASE + 0x00c))
// Clocks
#define CLOCKS_BASE                 (0x40008000)
#define CLOCKS_REF_CTRL             (*(volatile uint32_t *) (CLOCKS_BASE + 0x030))
#define CLOCKS_REF_SELECTED         (*(volatile uint32_t *) (CLOCKS_BASE + 0x038))
#define CLOCKS_SYS_CTRL             (*(volatile uint32_t *) (CLOCKS_BASE + 0x03c))
#define CLOCKS_SYS_SELECTED         (*(volatile uint32_t *) (CLOCKS_BASE + 0x044))
// ROSC
#define ROSC_BASE                   (0x40060000)
#define ROSC_CTRL                   (*(volatile uint32_t *) (ROSC_BASE + 0x000))
// WATCHDOG
#define WATCHDOG_BASE               (0x40058000)
#define WATCHDOG_TICK               (*(volatile uint32_t *) (WATCHDOG_BASE + 0x02c))
// TIMER
#define TIMER_BASE                  (0x40054000)
#define TIMER_TIMEHR                (*(volatile uint32_t *) (TIMER_BASE + 0x008))
#define TIMER_TIMELR                (*(volatile uint32_t *) (TIMER_BASE + 0x00c))

void SystemInit()
{
    // Initialize XOSC
    XOSC_CTRL |= (0xfab << 12); // Enable XOSC
    while (!(XOSC_STATUS & (1 << 31))); // Wait for XOSC to stabilize

    // Initialize System PLL
    RESETS_RESET &= ~(1 << 12); // Bring System PLL out of reset state
    while (!(RESETS_RESET_DONE & (1 << 12))); // Wait for PLL peripheral to respond
    PLL_SYS_FBDIV_INT = 100; // Set feedback clock div = 100, thus VCO clock = 12MHz * 100 = 1.2GHz
    PLL_SYS_PWR &= ~((1 << 0) | (1 << 5)); // Turn on the main power and VCO
    while (!(PLL_SYS_CS & (1 << 31))); // Wait for PLL to lock
    PLL_SYS_PRIM = (6 << 16) | (2 << 12); // Set POSTDIV1 = 6 and POSTDIV2 = 2, thus 1.2GHz / 6 / 2 = 100MHz
    PLL_SYS_PWR &= ~(1 << 3); // Turn on the post dividers

    // Setup clock generators
    // Setup clk_ref
    CLOCKS_REF_CTRL |= (2 << 0); // Switch clk_ref glitchless mux to XOSC_CLKSRC for the best accuracy possible
    while (!(CLOCKS_REF_SELECTED & (1 << 2)));// Make sure that the switch happened
    // Setup clk_sys
    CLOCKS_SYS_CTRL |= (1 << 0); // Switch clk_sys glitchless mux to CLKSRC_CLK_SYS_AUX and the aux defaults to CLKSRC_PLL_SYS
    while (!(CLOCKS_SYS_SELECTED & (1 << 1)));// Make sure that the switch happened

    // Shut down ROSC
    ROSC_CTRL = (ROSC_CTRL & (~0x00fff000)) | (0xd1e << 12);

    // Enable 64-bit Timer
    WATCHDOG_TICK |= (12 << 0); // Set appropriate value for TICK, 1 us = 12 cycles / 12MHz
    RESETS_RESET &= ~(1 << 21); // Bring 64-bit Timer out of reset state
    while (!(RESETS_RESET_DONE & (1 << 21))); // Wait for TIMER peripheral to respond
}

uint64_t readTime()
{
    uint32_t timeLR = TIMER_TIMELR;
    uint32_t timeHR = TIMER_TIMEHR;
    return (((uint64_t)timeHR << 32) | timeLR);
}

void usSleep(uint64_t us)
{
    uint64_t timeOld = readTime(); // Get current timer value
    while ((readTime() - timeOld) < us); // Wait till desired time is passed
}