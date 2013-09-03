# should work with recent Yartago setups, including utilities
#

CC	  = arm-none-eabi-gcc
AS	  = arm-none-eabi-as
LD	  = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy

OBJDIR  = obj

INCLUDE = -I../include

INCLUDES = ../include/*.h

CFLAGS  = -O2 -c $(INCLUDE) -Wall

OBJ =   $(OBJDIR)/start.o

all: ../app_image.s19

$(OBJDIR)/osimage.s19: $(OBJCOMMON)
	mkdir  -p $(OBJDIR)
	@echo obj/osimage.s19
	@$(CC)  -o $(OBJ) $(CFLAGS) -mthumb -mthumb-interwork start.c
	@$(LD) -g -Map=../app_image.map -Tldscript -o $(OBJDIR)/osimage.elf $(OBJ)
	@$(OBJCOPY) -Osrec --srec-forceS3 $(OBJDIR)/osimage.elf $(OBJDIR)/osimage.s19

../app_image.s19: $(OBJDIR)/osimage.s19
	echo osimage.s19
	@cat $(OBJDIR)/osimage.s19 > ../app_image.s19
	@rm -rf $(OBJDIR)

clean:
	@rm -rf $(OBJDIR)
	@rm -f ../app_image.s19
