//-----------------------------------------------------------------------------
// Definitions for all the types of commands that may be sent over USB; our
// own protocol.
// Jonathan Westhues, Mar 2006
//-----------------------------------------------------------------------------

#ifndef __USB_CMD_H
#define __USB_CMD_H

typedef struct {
    DWORD       cmd;
    DWORD       ext1;
    DWORD       ext2;
    DWORD       ext3;
    union {
        BYTE        asBytes[48];
        DWORD       asDwords[12];
    } d;
} UsbCommand;

// For the bootloader
#define CMD_DEVICE_INFO                         0x0000
#define CMD_SETUP_WRITE                         0x0001
#define CMD_FINISH_WRITE                        0x0003
#define CMD_HARDWARE_RESET                      0x0004
#define CMD_ACK                                 0x00ff

#endif
