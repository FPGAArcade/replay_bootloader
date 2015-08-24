//-----------------------------------------------------------------------------
// The bootrom proper; the piece of code that runs from RAM and can program
// flash. If we are loaded over JTAG, then we go directly into RAM; otherwise
// we are copied from some location in flash into RAM by the routines in
// fromflash.c. In any case, we start up the USB driver and either load
// new code (if the downloader connects) or give up waiting and jump to
// the application.
//
// Jonathan Westhues, split Aug 14 2005, public release May 26 2006
//
// This version was modified by W. Scherr, admin *AT* pin4 *dot* at -
// for replay board. See also http://fpgaarcade.com/ and http://www.pin4.at/.
//-----------------------------------------------------------------------------
#include <bootrom.h>

#define AT91C_CKGR_MOSCEN     ((unsigned int) 0x1 <<  0) // (CKGR) Main Oscillator Enable
#define AT91C_CKGR_OSCBYPASS  ((unsigned int) 0x1 <<  1) // (CKGR) Main Oscillator Bypass
#define AT91C_CKGR_OSCOUNT    ((unsigned int) 0xFF <<  8) // (CKGR) Main Oscillator Start-up Time

#define AT91C_PMC_MOSCS       ((unsigned int) 0x1 <<  0) // (PMC) MOSC Status/Enable/Disable/Mask
#define AT91C_PMC_LOCK        ((unsigned int) 0x1 <<  2) // (PMC) PLL Status/Enable/Disable/Mask
#define AT91C_PMC_MCKRDY      ((unsigned int) 0x1 <<  3) // (PMC) MCK_RDY Status/Enable/Disable/Mask

#define AT91C_CKGR_USBDIV_1   ((unsigned int) 0x1 << 28) // (CKGR) Divider output is PLL clock output divided by 2
#define AT91C_CKGR_MUL        ((unsigned int) 0x7FF << 16) // (CKGR) PLL Multiplier
#define AT91C_CKGR_DIV        ((unsigned int) 0xFF <<  0) // (CKGR) Divider Selected
#define AT91C_CKGR_MUL        ((unsigned int) 0x7FF << 16) // (CKGR) PLL Multiplier

#define         AT91C_PMC_PRES_CLK_2                ((unsigned int) 0x1 <<  2) // (PMC) Selected clock divided by 2
#define         AT91C_PMC_CSS_PLL_CLK              ((unsigned int) 0x3) // (PMC) Clock from PLL is selected

static void ConfigClocks(void)
{
	PMC_MAIN_OSCILLATOR = ( AT91C_CKGR_OSCOUNT & (0x40 <<8)) | AT91C_CKGR_MOSCEN;
	// Wait Main Oscillator stabilization
	while(!(PMC_INTERRUPT_STATUS & AT91C_PMC_MOSCS));

	// Init PMC Step 2.
	PMC_PLL = AT91C_CKGR_USBDIV_1           |
							   (16 << 8)                     |
							   (AT91C_CKGR_MUL & (72 << 16)) |
							   (AT91C_CKGR_DIV & 14);

	// Wait for PLL stabilization
	while( !(PMC_INTERRUPT_STATUS & AT91C_PMC_LOCK) );
	// Wait until the master clock is established for the case we already
	// turn on the PLL
	while( !(PMC_INTERRUPT_STATUS & AT91C_PMC_MCKRDY) );

	 // Init PMC Step 3.
	PMC_MASTER_CLK = AT91C_PMC_PRES_CLK_2;
	// Wait until the master clock is established
	while( !(PMC_INTERRUPT_STATUS & AT91C_PMC_MCKRDY) );

	PMC_MASTER_CLK |= AT91C_PMC_CSS_PLL_CLK;
	// Wait until the master clock is established
	while( !(PMC_INTERRUPT_STATUS & AT91C_PMC_MCKRDY) );
}

static void Fatal(void)
{
	for(;;);
}

void UsbPacketReceived(BYTE *packet, int len)
{
	int i;
	UsbCommand *c = (UsbCommand *)packet;
	volatile DWORD *p;

	if(len != sizeof(*c)) {
		Fatal();
	}

	switch(c->cmd) {
		case CMD_DEVICE_INFO:
			break;

		case CMD_SETUP_WRITE:
			p = (volatile DWORD *)0;
			for(i = 0; i < 12; i++) {
				p[i+c->ext1] = c->d.asDwords[i];
			}
			break;

		case CMD_FINISH_WRITE:
			p = (volatile DWORD *)0;
			for(i = 0; i < 4; i++) {
				p[i+60] = c->d.asDwords[i];
			}

			MC_FLASH_COMMAND = MC_FLASH_COMMAND_KEY |
				MC_FLASH_COMMAND_PAGEN(c->ext1/FLASH_PAGE_SIZE_BYTES) |
				FCMD_WRITE_PAGE;
			while(!(MC_FLASH_STATUS & MC_FLASH_STATUS_READY))
				;
			break;

		case CMD_HARDWARE_RESET:
			break;

		default:
			Fatal();
			break;
	}

	c->cmd = CMD_ACK;
	UsbSendPacket(packet, len);
}

void Bootrom(void)
{

	int i = 0;

	//------------
	// First set up all the I/O pins; GPIOs configured directly, other ones
	// just need to be assigned to the appropriate peripheral.

	// Kill all the pullups, especially the one on USB D+; leave them for
	// the unused pins, though.
	PIO_NO_PULL_UP_ENABLE =     (1 << GPIO_LED);

	PIO_OUTPUT_ENABLE =         (1 << GPIO_USB_PU)          |
								(1 << GPIO_LED);

	PIO_ENABLE =                (1 << GPIO_KEY)             |
								(1 << GPIO_USB_PU)          |
								(1 << GPIO_LED);

	PIO_GLITCH_ENABLE =         (1 << GPIO_KEY);

	LED_ON();

	// stack setup?
	USB_D_PLUS_PULLUP_OFF();

	for(i = 0; i < 10000; i++) LED_OFF(); // delay a bit, before testing the key

	if (PIO_PIN_DATA_STATUS&(1<<GPIO_KEY)) goto run_flash;

	// disable watchdog
	WDT_MODE = WDT_MODE_DISABLE;

	LED_ON();

	// Configure the flash that we are running out of.
	MC_FLASH_MODE = MC_FLASH_MODE_FLASH_WAIT_STATES(1) |
		MC_FLASH_MODE_MASTER_CLK_IN_MHZ(48);

	// Careful, a lot of peripherals can't be configured until the PLL clock
	// comes up; you write to the registers but it doesn't stick.
	ConfigClocks();
	UsbStart();

	// Borrow a PWM unit for my real-time clock
	PWM_ENABLE = PWM_CHANNEL(0);
	// 48 MHz / 1024 gives 46.875 kHz
	PWM_CH_MODE(0) = PWM_CH_MODE_PRESCALER(10);
	PWM_CH_DUTY_CYCLE(0) = 0;
	PWM_CH_PERIOD(0) = 0xffff;

	WORD start = (SWORD)PWM_CH_COUNTER(0);

	i=0;
	for(;;) {

		WORD now = (SWORD)PWM_CH_COUNTER(0);

		if(UsbPoll()) {
			// It did something; reset the clock that would jump us to the
			// applications.
			start = now;
		}

		WDT_HIT();
		if((SWORD)(now - start) > 30000) {
			i=i+1;
			if (i&1) LED_OFF();
			else LED_ON();

			// you may increase the number below if the enumeration process
			// in Windows is longer and the downloader does not work...)
			if (i>10) { //after ~5sec or no keypress
run_flash:
				USB_D_PLUS_PULLUP_OFF();
				LED_OFF();

				// This is a function call to 0x00102001, the application reset
				// vector, which is equal to (0x81 << 13)+1.
				//
				// I would have thought that I could write this as a cast to
				// a function pointer and a call, but I can't seem to get that
				// to work. I also can't figure out how to make the assembler
				// load pc relative a constant in flash, thus the ugly way
				// to specify the address.
				asm("mov r3, #129\n");
				asm("lsl r3, r3, #13\n");
				//asm("mov r4, #1\n");  // we don't need this!
				//asm("orr r3, r4\n");
				asm("bx r3\n");
			} else {
				start = now;
			}
		}
	}
}
