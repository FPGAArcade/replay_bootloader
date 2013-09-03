
#ifndef __BOOTROM_H
#define __BOOTROM_H

#include <at91sam7sXXX.h>
#include <myhw.h>
#include "../include/usb_cmd.h"

#define USB_D_PLUS_PULLUP_ON() { \
        PIO_OUTPUT_DATA_CLEAR = (1<<GPIO_USB_PU); \
        PIO_OUTPUT_ENABLE = (1<<GPIO_USB_PU); \
    }
#define USB_D_PLUS_PULLUP_OFF() { \
        PIO_OUTPUT_DATA_SET = (1<<GPIO_USB_PU); \
        PIO_OUTPUT_ENABLE = (1<<GPIO_USB_PU); \
    }

#define LED_ON()            PIO_OUTPUT_DATA_CLEAR = (1<<GPIO_LED)
#define LED_OFF()           PIO_OUTPUT_DATA_SET = (1<<GPIO_LED)

// These are functions that the USB driver provides.
void UsbStart(void);
BOOL UsbPoll(void);
void UsbSendPacket(BYTE *packet, int len);

// These are functions that the USB driver calls, that the code that uses
// it provides.
void UsbPacketReceived(BYTE *data, int len);

#endif

