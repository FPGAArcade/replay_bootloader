//-----------------------------------------------------------------------------
// Just vector to AppMain(). This is in its own file so that I can place it
// with the linker script.
// Jonathan Westhues, Mar 2006
//
// This version was modified by W. Scherr, admin *AT* pin4 *dot* at -
// for replay board. See also http://fpgaarcade.com/ and http://www.pin4.at/.
//-----------------------------------------------------------------------------

#include "../bootrom/at91sam7sXXX.h"
#include "../bootrom/myhw.h"

#define LED_ON()            PIO_OUTPUT_DATA_CLEAR = (1<<GPIO_LED)
#define LED_OFF()           PIO_OUTPUT_DATA_SET = (1<<GPIO_LED)

void Vector(void)
{	int i;
    // Of course, for real you would jump to your start code here.
    for(;;) {
		for(i = 0; i < 1000000; i++) LED_ON();
		for(i = 0; i < 1000000; i++) LED_OFF();
        WDT_HIT();
	}
}
