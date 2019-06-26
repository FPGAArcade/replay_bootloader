
## This is a simple bootloader for AT91SAM7256 processor located on the fpgaarcade.com replay board. 

-------------------------------------------------------------------------
This version was modified by W. Scherr, admin *AT* pin4 *dot* at -
for replay board. See also http://fpgaarcade.com/ and http://www.pin4.at/.

-------------------------------------------------------------------------

Atmel provides a bootloader called SAM-BA but as far as I can tell it
is not what I want. SAM-BA takes full control of the processor: there
is no provision for it to jump to a separate `application' after a few
seconds. It would be possible to use their bootloader to download any kind
of program to the device, but in doing so that program must wipe SAM-BA
out (or else SAM-BA would never let it execute). This seems like it would
be rather inconvenient for development; you would have to fiddle with
the test pins to reload SAM-BA every time you wanted to download new code.

So the new bootloader works as follows:

At power-up, it is activated by pressing the menu button on the replay
board. It waits about 5 seconds to see if the downloader is trying to 
connect via USB. Should be sufficient for Windows to set up its driver.

Afterwards the bootloader gives up control and jumps to your program. 
If the downloader is trying to connect, then the bootloader receives 
the new program over USB and writes it into flash.

Its build upon this code: http://cq.cx/at91sam7sXXX.pl (and especially
the code was downloaded from here: http://cq.cx/dl/at91sam7sXXX.zip)

Credits for the original software goes to:   
	Jonathan Westhues, May 2006 (updated Jun 2007)   
	user jwesthues, at host cq.cx

I keep the license as is, also for this derivative work:

    THIS SOFTWARE IS IN THE PUBLIC DOMAIN. It may therefore be used in any 
    application, commercial or non-commercial, with or without attribution 
    to the author. NO WARRANTY IS EXPRESSED OR IMPLIED. 


### Step-by-step instructions:

0) Prerequisites:

    Compiling the code requires open-source/freeware tools only:
     * All of the ARM-side code is tested with Yagarto (arm-none-eabi, see http://www.yagarto.de/ ).
     * The downloader, which runs under Windows, is tested with Mingw (install
 Mingw including the MSYS development package, see here:
 http://www.mingw.org/wiki/Getting_Started, I used mingw-get-inst-20120426).

1) Building the bootrom:

        cd bootloader/bootrom
        make all

    The PID/VID are random numbers. If you intend to use this in a product,
then you should at least choose different random numbers in the code.

2) Flash the created "bootrom.bin" file using bossa   
(see here: http://www.shumatech.com/web/products/bossa), sam-ba/jTag should work, too.  
Do not set the lock bit!

3) Build the downloader:

        cd ../loader
        make all

4) The downloader recognizes the following command line options:

        usbdl load  app.s19         -- load the application from app.s19
        usbdl bootrom bootrom.s19   -- load the bootrom from bootrom.s19

    It is possible to use the bootrom to load a new bootrom, even when the
existing bootrom is running from flash. Of course, if something goes
wrong while doing this then you will have to reload the bootrom
over JTAG or SAM-BA! Don't use samba/bossa too often, it has impact
on the lifetime of the NVM/flags (sets/resets the flash lock all the time).
    
5) Set up your application. An simple example is provided:

        cd ../app
        make all
        ../usbdl.exe load myapp.s19

    Then do a power cycle on the board while pressing the button.

    The memory map of the ARM's flash must be as follows:
    
        0x00000000                  -- bootrom
        0x00002000                  -- start of application

    The bootrom does the equivalent of a C function call to the start of
your application; the stack will already be set up.
