# libs must be AFTER the source arguments in gcc, otherwise link wil fail!!!
#
# should work with recent MinGW setups
#

ifeq ($(OS),Windows_NT)
	APP_NAME = usbdl.exe
else
	APP_NAME = usbdl_$(shell uname -s).elf
endif

LIBS   = -lkernel32 -luser32 -ladvapi32 -luuid -lsetupapi

all: ../$(APP_NAME)

../usbdl.exe: usbdl.c ../include/usb_cmd.h
	gcc -o ../usbdl.exe usbdl.c $(LIBS)

../%_Darwin.elf: usbdl.c usbdl_osx.h ../include/usb_cmd.h
	gcc -o $@ usbdl.c -framework IOKit -framework CoreFoundation

clean:
	rm ../$(APP_NAME)
