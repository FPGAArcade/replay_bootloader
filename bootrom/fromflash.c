#include <bootrom.h>

extern void CallRam(void);

static void ConfigClocks(void)
{
    volatile int i;

    // we are using a 18.432 MHz crystal as the basis for everything
    PMC_SYS_CLK_ENABLE = PMC_SYS_CLK_PROCESSOR_CLK | PMC_SYS_CLK_UDP_CLK;

    PMC_PERIPHERAL_CLK_ENABLE =
        (1<<PERIPH_PIOA) |
        (1<<PERIPH_ADC) |
        (1<<PERIPH_SPI) |
        (1<<PERIPH_SSC) |
        (1<<PERIPH_PWMC) |
        (1<<PERIPH_UDP);

    PMC_MAIN_OSCILLATOR = PMC_MAIN_OSCILLATOR_ENABLE |
        PMC_MAIN_OSCILLATOR_STARTUP_DELAY(0x40);

    // TODO: THIS DEPENDS ON THE CRYSTAL FREQUENCY THAT YOU CHOOSE. Make
    // the ARM run at some reasonable speed, and make the USB peripheral
    // run at exactly 48 MHz.
	
    // minimum PLL clock frequency is 80 MHz in range 00 (96 here so okay)
    PMC_PLL = PMC_PLL_DIVISOR(14) | PMC_PLL_COUNT_BEFORE_LOCK(28) |
        PMC_PLL_FREQUENCY_RANGE(0) | PMC_PLL_MULTIPLIER(73) |
        PMC_PLL_USB_DIVISOR(1);

    // let the PLL spin up, plenty of time
    for(i = 0; i < 100; i++)
        ;

    PMC_MASTER_CLK = PMC_CLK_SELECTION_SLOW_CLOCK | PMC_CLK_PRESCALE_DIV_2;

    for(i = 0; i < 100; i++)
        ;

    PMC_MASTER_CLK = PMC_CLK_SELECTION_PLL_CLOCK | PMC_CLK_PRESCALE_DIV_2;

    for(i = 0; i < 100; i++)
        ;
}


void CMain(void)
{
    int i;

    volatile DWORD *src = (volatile DWORD *)0x200;
    volatile DWORD *dest = (volatile DWORD *)0x00200000;

	// Configure the flash that we are running out of (soon).
	MC_FLASH_MODE = MC_FLASH_MODE_FLASH_WAIT_STATES(1) |
		MC_FLASH_MODE_MASTER_CLK_IN_MHZ(48);

    ConfigClocks();

    for(i = 0; i < 1024; i++) {
        *dest++ = *src++;
        WDT_HIT();
    }

    CallRam();
}
