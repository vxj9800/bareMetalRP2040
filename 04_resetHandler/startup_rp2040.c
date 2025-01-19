#include <stdint.h>
#include <stdbool.h>

// Type of vector table entry
typedef void (*vectFunc) (void);

// Declare the initial stack pointer, the value will be provided by the linker
extern uint32_t __stack, _sdata, _edata, _sdataf;

// Declare _start function from libgloss
extern void _start(void);

// Declare interrupt functions defined in this file
__attribute__((noreturn)) void defaultHandler();
__attribute__((noreturn)) void resetHandler();

// Declare all other core interrupt functions as weak and alias of the defaultHandler
void nmiHandler         () __attribute__((weak, alias("defaultHandler")));
void hardFaultHandler   () __attribute__((weak, alias("defaultHandler")));
void svCallHandler      () __attribute__((weak, alias("defaultHandler")));
void pendSvHandler      () __attribute__((weak, alias("defaultHandler")));
void sysTickHandler     () __attribute__((weak, alias("defaultHandler")));

// Declare all other external interrupt functions as weak and alias of the defaultHandler
void timerIrq0          () __attribute__((weak, alias("defaultHandler")));
void timerIrq1          () __attribute__((weak, alias("defaultHandler")));
void timerIrq2          () __attribute__((weak, alias("defaultHandler")));
void timerIrq3          () __attribute__((weak, alias("defaultHandler")));
void pwmIrqWrap         () __attribute__((weak, alias("defaultHandler")));
void usbctrlIrq         () __attribute__((weak, alias("defaultHandler")));
void xipIrq             () __attribute__((weak, alias("defaultHandler")));
void pio0Irq0           () __attribute__((weak, alias("defaultHandler")));
void pio0Irq1           () __attribute__((weak, alias("defaultHandler")));
void pio1Irq0           () __attribute__((weak, alias("defaultHandler")));
void pio1Irq1           () __attribute__((weak, alias("defaultHandler")));
void dmaIrq0            () __attribute__((weak, alias("defaultHandler")));
void dmaIrq1            () __attribute__((weak, alias("defaultHandler")));
void ioIrqBank0         () __attribute__((weak, alias("defaultHandler")));
void ioIrqQspi          () __attribute__((weak, alias("defaultHandler")));
void sioIrqProc0        () __attribute__((weak, alias("defaultHandler")));
void sioIrqProc1        () __attribute__((weak, alias("defaultHandler")));
void clocksIrq          () __attribute__((weak, alias("defaultHandler")));
void spi0Irq            () __attribute__((weak, alias("defaultHandler")));
void spi1Irq            () __attribute__((weak, alias("defaultHandler")));
void uart0Irq           () __attribute__((weak, alias("defaultHandler")));
void uart1Irq           () __attribute__((weak, alias("defaultHandler")));
void adcIrqFifo         () __attribute__((weak, alias("defaultHandler")));
void i2c0Irq            () __attribute__((weak, alias("defaultHandler")));
void i2c1Irq            () __attribute__((weak, alias("defaultHandler")));
void rtcIrq             () __attribute__((weak, alias("defaultHandler")));

// Declare SystemInit function
extern void SystemInit(void);

// Declare usSleep function
extern void usSleep(uint64_t us);

// Declare main function
extern int main(void);

// Vector table: 48 Exceptions = 16 Arm + 32 External
const vectFunc vector[48] __attribute__((section(".vector"))) = 
{
    (vectFunc)(&__stack),   // Stack pointer
    resetHandler,           // Reset Handler
    nmiHandler,             // NMI
    hardFaultHandler,       // HardFault
    0,                      // Reserved
    0,                      // Reserved
    0,                      // Reserved
    0,                      // Reserved
    0,                      // Reserved
    0,                      // Reserved
    0,                      // Reserved
    svCallHandler,          // SVCall
    0,                      // Reserved
    0,                      // Reserved
    pendSvHandler,          // PendSV
    sysTickHandler,         // SysTick
    timerIrq0,              // ExternalInterrupt[0]     = TIMER_IRQ_0
    timerIrq1,              // ExternalInterrupt[1]     = TIMER_IRQ_1
    timerIrq2,              // ExternalInterrupt[2]     = TIMER_IRQ_2
    timerIrq3,              // ExternalInterrupt[3]     = TIMER_IRQ_3
    pwmIrqWrap,             // ExternalInterrupt[4]     = PWM_IRQ_WRAP
    usbctrlIrq,             // ExternalInterrupt[5]     = USBCTRL_IRQ
    xipIrq,                 // ExternalInterrupt[6]     = XIP_IRQ
    pio0Irq0,               // ExternalInterrupt[7]     = PIO0_IRQ_0
    pio0Irq1,               // ExternalInterrupt[8]     = PIO0_IRQ_1
    pio1Irq0,               // ExternalInterrupt[9]     = PIO1_IRQ_0
    pio1Irq1,               // ExternalInterrupt[10]    = PIO1_IRQ_1
    dmaIrq0,                // ExternalInterrupt[11]    = DMA_IRQ_0
    dmaIrq1,                // ExternalInterrupt[12]    = DMA_IRQ_1
    ioIrqBank0,             // ExternalInterrupt[13]    = IO_IRQ_BANK0
    ioIrqQspi,              // ExternalInterrupt[14]    = IO_IRQ_QSPI
    sioIrqProc0,            // ExternalInterrupt[15]    = SIO_IRQ_PROC0
    sioIrqProc1,            // ExternalInterrupt[16]    = SIO_IRQ_PROC1
    clocksIrq,              // ExternalInterrupt[17]    = CLOCKS_IRQ
    spi0Irq,                // ExternalInterrupt[18]    = SPI0_IRQ
    spi1Irq,                // ExternalInterrupt[19]    = SPI1_IRQ
    uart0Irq,               // ExternalInterrupt[20]    = UART0_IRQ
    uart1Irq,               // ExternalInterrupt[21]    = UART1_IRQ
    adcIrqFifo,             // ExternalInterrupt[22]    = ADC_IRQ_FIFO
    i2c0Irq,                // ExternalInterrupt[23]    = I2C0_IRQ
    i2c1Irq,                // ExternalInterrupt[24]    = I2C1_IRQ
    rtcIrq,                 // ExternalInterrupt[25]    = RTC_IRQ
    0,                      // ExternalInterrupt[26]    = Reserved
    0,                      // ExternalInterrupt[27]    = Reserved
    0,                      // ExternalInterrupt[28]    = Reserved
    0,                      // ExternalInterrupt[29]    = Reserved
    0,                      // ExternalInterrupt[30]    = Reserved
    0,                      // ExternalInterrupt[31]    = Reserved
};

void resetHandler()
{
    // Copy .data section from FLASH to SRAM
    uint32_t *initValsPtr = &_sdataf;
    for (uint32_t *dataPtr = &_sdata; dataPtr < &_edata; ++dataPtr)
        *dataPtr = *initValsPtr++;
    
    // Initialize the system
    SystemInit();

    _start(); // Call C Runtime Startup, it will jump to main function
    while(true); // Inf loop if we ever come back here
}

// Define necessary register addresses
#define RESETS_RESET                                    *(volatile uint32_t *) (0x4000c000)
#define RESETS_RESET_DONE                               *(volatile uint32_t *) (0x4000c008)
#define IO_BANK0_GPIO25_CTRL                            *(volatile uint32_t *) (0x400140cc)
#define SIO_GPIO_OE_SET                                 *(volatile uint32_t *) (0xd0000024)
#define SIO_GPIO_OUT_XOR                                *(volatile uint32_t *) (0xd000001c)

void defaultHandler()
{
    RESETS_RESET &= ~(1 << 5); // Bring IO_BANK0 out of reset state
    while (!(RESETS_RESET_DONE & (1 << 5))); // Wait for peripheral to respond
    IO_BANK0_GPIO25_CTRL = 5; // Set GPIO 25 function to SIO
    SIO_GPIO_OE_SET |= 1 << 25; // Set output enable for GPIO 25 in SIO

    while (true)
    {
        usSleep(50000); // Wait for 0.05sec
        SIO_GPIO_OUT_XOR |= 1 << 25; // Flip output for GPIO 25
    }
}