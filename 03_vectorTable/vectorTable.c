#include <stdint.h>
#include <stdbool.h>

// Define necessary register addresses
#define RESETS_RESET                                    *(volatile uint32_t *) (0x4000c000)
#define IO_BANK0_GPIO25_CTRL                            *(volatile uint32_t *) (0x400140cc)
#define SIO_GPIO_OE_SET                                 *(volatile uint32_t *) (0xd0000024)
#define SIO_GPIO_OUT_XOR                                *(volatile uint32_t *) (0xd000001c)

// Define the initial stack pointer, the value will be provided by the linker
extern void _sstack();

// Declare main function
extern int main(void);

void _reset()
{
    main(); // Jump to main function
    while(true); // Inf loop if we ever come back here
}

void _defaultHandler()
{
    RESETS_RESET &= ~(1 << 5); // Bring IO_BANK0 out of reset state
    IO_BANK0_GPIO25_CTRL = 5; // Set GPIO 25 function to SIO
    SIO_GPIO_OE_SET |= 1 << 25; // Set output enable for GPIO 25 in SIO

    while (true)
    {
        for (uint32_t i = 0; i < 10000; ++i); // Wait for some time
        SIO_GPIO_OUT_XOR |= 1 << 25; // Flip output for GPIO 25
    }
}

__attribute__((section(".vector"))) void (*const vector[])(void) = 
{
    _sstack,            // Stack pointer
    _reset,             // Reset Handler
    _defaultHandler,    // NMI
    _defaultHandler,    // HardFault
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // SVCall
    _defaultHandler,    // Reserved
    _defaultHandler,    // Reserved
    _defaultHandler,    // PendSV
    _defaultHandler,    // SysTick
    _defaultHandler,    // ExternalInterrupt[0]     = TIMER_IRQ_0
    _defaultHandler,    // ExternalInterrupt[1]     = TIMER_IRQ_1
    _defaultHandler,    // ExternalInterrupt[2]     = TIMER_IRQ_2
    _defaultHandler,    // ExternalInterrupt[3]     = TIMER_IRQ_3
    _defaultHandler,    // ExternalInterrupt[4]     = PWM_IRQ_WRAP
    _defaultHandler,    // ExternalInterrupt[5]     = USBCTRL_IRQ
    _defaultHandler,    // ExternalInterrupt[6]     = XIP_IRQ
    _defaultHandler,    // ExternalInterrupt[7]     = PIO0_IRQ_0
    _defaultHandler,    // ExternalInterrupt[8]     = PIO0_IRQ_1
    _defaultHandler,    // ExternalInterrupt[9]     = PIO1_IRQ_0
    _defaultHandler,    // ExternalInterrupt[10]    = PIO1_IRQ_1
    _defaultHandler,    // ExternalInterrupt[11]    = DMA_IRQ_0
    _defaultHandler,    // ExternalInterrupt[12]    = DMA_IRQ_1
    _defaultHandler,    // ExternalInterrupt[13]    = IO_IRQ_BANK0
    _defaultHandler,    // ExternalInterrupt[14]    = IO_IRQ_QSPI
    _defaultHandler,    // ExternalInterrupt[15]    = SIO_IRQ_PROC0
    _defaultHandler,    // ExternalInterrupt[16]    = SIO_IRQ_PROC1
    _defaultHandler,    // ExternalInterrupt[17]    = CLOCKS_IRQ
    _defaultHandler,    // ExternalInterrupt[18]    = SPI0_IRQ
    _defaultHandler,    // ExternalInterrupt[19]    = SPI1_IRQ
    _defaultHandler,    // ExternalInterrupt[20]    = UART0_IRQ
    _defaultHandler,    // ExternalInterrupt[21]    = UART1_IRQ
    _defaultHandler,    // ExternalInterrupt[22]    = ADC_IRQ_FIFO
    _defaultHandler,    // ExternalInterrupt[23]    = I2C0_IRQ
    _defaultHandler,    // ExternalInterrupt[24]    = I2C1_IRQ
    _defaultHandler,    // ExternalInterrupt[25]    = RTC_IRQ
    _defaultHandler,    // ExternalInterrupt[26]    = Reserved
    _defaultHandler,    // ExternalInterrupt[27]    = Reserved
    _defaultHandler,    // ExternalInterrupt[28]    = Reserved
    _defaultHandler,    // ExternalInterrupt[29]    = Reserved
    _defaultHandler,    // ExternalInterrupt[30]    = Reserved
    _defaultHandler,    // ExternalInterrupt[31]    = Reserved
};