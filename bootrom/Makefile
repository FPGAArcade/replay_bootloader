# should work with recent Yartago setups, including utilities
#

CC	  = arm-none-eabi-gcc
AS	  = arm-none-eabi-as
LD	  = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
OBJDIR  = obj

INCLUDE = -I.

INCLUDES = at91sam7sXXX.h

CFLAGS  = -g -c $(INCLUDE) -Wall

OBJJTAG = $(OBJDIR)/bootrom.o $(OBJDIR)/ram-reset.o $(OBJDIR)/usb.o

OBJFLASH = $(OBJDIR)/flash-reset.o $(OBJDIR)/fromflash.o

all: setup ../bootrom.bin

setup:
	mkdir -p $(OBJDIR)

$(OBJDIR)/bootrom.bin: $(OBJFLASH)
	@echo obj/bootrom.bin
	@$(LD) -g -Map=../bootrom.map -Tldscript-flash --oformat elf32-littlearm -o $(OBJDIR)/bootrom.elf $(OBJFLASH)
	@$(OBJCOPY) -Obinary --pad-to 512 $(OBJDIR)/bootrom.elf $(OBJDIR)/bootrom.bin
	@$(OBJDUMP) -d -S $(OBJDIR)/bootrom.elf > ../bootrom.lst

#may not work with yartago only so we use copy below
#@cat $(OBJDIR)/bootrom.bin $(OBJDIR)/bootrom-forjtag.bin > ../bootrom.bin

../bootrom.bin: $(OBJDIR)/bootrom.bin $(OBJDIR)/bootrom-forjtag.bin
	@echo bootrom.bin
	@echo "@copy /b $(OBJDIR)\bootrom.bin + /b $(OBJDIR)\bootrom-forjtag.bin /b ..\bootrom.bin" > merge.bat
	@.\merge.bat
	@rm -rf $(OBJDIR) 

$(OBJDIR)/bootrom-forjtag.bin: $(OBJJTAG)
	@echo obj/bootrom-forjtag.bin
	@$(LD) -g -Map=../bootrom-forjtag.map -Tldscript-ram-jtag --oformat elf32-littlearm -o $(OBJDIR)/bootrom-forjtag.elf $(OBJJTAG)
	@$(OBJCOPY) -Obinary $(OBJDIR)/bootrom-forjtag.elf $(OBJDIR)/bootrom-forjtag.bin
	@$(OBJDUMP) -d -S $(OBJDIR)/bootrom-forjtag.elf > ../bootrom-forjtag.lst

forjtag.bin: $(OBJDIR)/bootrom-forjtag.bin
	@echo forjtag.bin
	@cp $(OBJDIR)/bootrom-forjtag.bin  forjtag.bin

$(OBJDIR)/bootrom.o: bootrom.c $(INCLUDES)
	@echo $(@B).c
	@$(CC) $(CFLAGS) -mthumb -mthumb-interwork bootrom.c -o $(OBJDIR)/bootrom.o

$(OBJDIR)/fromflash.o: fromflash.c $(INCLUDES)
	@echo $(@B).c
	@$(CC) $(CFLAGS) -mthumb -mthumb-interwork fromflash.c -o $(OBJDIR)/fromflash.o

$(OBJDIR)/usb.o: usb.c $(INCLUDES)
	@echo $(@B).c
	@$(CC) $(CFLAGS) -mthumb -mthumb-interwork usb.c -o $(OBJDIR)/usb.o

$(OBJDIR)/ram-reset.o: ram-reset.s
	@echo $(@B).s
	@$(CC) $(CFLAGS) -mthumb-interwork -o $(OBJDIR)/ram-reset.o ram-reset.s

$(OBJDIR)/flash-reset.o: flash-reset.s
	@echo $(@B).s
	@$(CC) $(CFLAGS) -mthumb-interwork -o $(OBJDIR)/flash-reset.o flash-reset.s

clean:
	@echo Cleanup
	@rm -rf $(OBJDIR)
	@rm -f *.bin ../*.map
