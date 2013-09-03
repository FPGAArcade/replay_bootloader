# libs must be AFTER the source arguments in gcc, otherwise link wil fail!!!
#
# should work with recent MinGW setups
#

LIBS   = -lkernel32 -luser32 -ladvapi32 -luuid -lsetupapi

all: ../usbdl.exe

../usbdl.exe: usbdl.c ../include/usb_cmd.h
	gcc -o ../usbdl.exe usbdl.c $(LIBS)

clean:
	rm usbdl.exe
