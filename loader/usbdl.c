//-----------------------------------------------------------------------------
// This is the Windows-side program that you run to download code into a
// device. It is capable of loading both the application (i.e., a piece of
// code starting at address 0x00002000 in flash) or the bootrom (i.e., a
// piece of code starting at address 0x00000000 in flash).
// 
// Since the device looks like an HID device, we do not need to provide our
// own kernel-mode driver. We just get a handle to our device--which we
// can find by looking at the PID/VID--and from there we can just use the
// usual I/O functions.
//
// Jonathan Westhues, July 2005, public release May 2006
//
// This version was modified by W. Scherr, admin *AT* pin4 *dot* at -
// for replay board. See also http://fpgaarcade.com/ and http://www.pin4.at/.
//-----------------------------------------------------------------------------

#if defined(WIN32)
#include <windows.h>
#include <setupapi.h>
#include "include/hidsdi.h"
#include "include/hidpi.h"
#include <stdint.h>
#elif defined(__APPLE__)
#include "usbdl_osx.h"
#elif defined(__linux__)
#include "usbdl_linux.h"
#elif defined (__CYGWIN__)
#error "use mingw32.exe"
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>


#include "../include/usb_cmd.h"

// This must obviously agree with the descriptors in the ARM-side code.
#define OUR_VID             0x9ac5
#define OUR_PID             0x4b8f

// Check your datasheets! This varies depending on the part.
#define FLASH_PAGE_SIZE     256

static HANDLE UsbHandle;
static DWORD VerifyTransfers = 0;

static void ShowError(void)
{
    char buf[1024];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
        buf, sizeof(buf), NULL);
    printf("ERROR: %s", buf);
}

static BOOL UsbConnect(void)
{
#if defined(WIN32)
    typedef void (__stdcall *GetGuidProc)(GUID *);
    typedef BOOLEAN (__stdcall *GetAttrProc)(HANDLE, HIDD_ATTRIBUTES *);
    typedef BOOLEAN (__stdcall *GetPreparsedProc)(HANDLE,
                                        PHIDP_PREPARSED_DATA *);
    typedef NTSTATUS (__stdcall *GetCapsProc)(PHIDP_PREPARSED_DATA, PHIDP_CAPS);
    GetGuidProc         getGuid;
    GetAttrProc         getAttr;
    GetPreparsedProc    getPreparsed;
    GetCapsProc         getCaps;

    // I don't think you can get hid.lib without paying for the DDK; but
    // if we link dynamically, then we don't need to.
    HMODULE h = LoadLibrary("hid.dll");
    getGuid      = (GetGuidProc)GetProcAddress(h, "HidD_GetHidGuid");
    getAttr      = (GetAttrProc)GetProcAddress(h, "HidD_GetAttributes");
    getPreparsed = (GetPreparsedProc)GetProcAddress(h, "HidD_GetPreparsedData");
    getCaps      = (GetCapsProc)GetProcAddress(h, "HidP_GetCaps");

    GUID hidGuid;
    getGuid(&hidGuid);

    HDEVINFO devInfo;
    devInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL,
        DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

    SP_DEVICE_INTERFACE_DATA devInfoData;
    devInfoData.cbSize = sizeof(devInfoData);

    int i;
    for(i = 0;; i++) {
        if(!SetupDiEnumDeviceInterfaces(devInfo, 0, &hidGuid, i, &devInfoData))
        {
            if(GetLastError() != ERROR_NO_MORE_ITEMS) {
//                printf("SetupDiEnumDeviceInterfaces failed\n");
            }
//            printf("done list\n");
            SetupDiDestroyDeviceInfoList(devInfo);
            return FALSE;
        }

//        printf("item %d:\n", i);
    
        DWORD sizeReqd = 0;
        if(!SetupDiGetDeviceInterfaceDetail(devInfo, &devInfoData,
            NULL, 0, &sizeReqd, NULL))
        {
            if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
//                printf("SetupDiGetDeviceInterfaceDetail (0) failed\n");
                continue;
            }
        }

        SP_DEVICE_INTERFACE_DETAIL_DATA *devInfoDetailData =
            (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(sizeReqd);
        devInfoDetailData->cbSize = sizeof(*devInfoDetailData);

        if(!SetupDiGetDeviceInterfaceDetail(devInfo, &devInfoData,
            devInfoDetailData, 87, NULL, NULL))
        {
//            printf("SetupDiGetDeviceInterfaceDetail (1) failed\n");
            continue;
        }

        char *path = devInfoDetailData->DevicePath;

        UsbHandle = CreateFile(path, /*GENERIC_READ |*/ GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, NULL);

        if(UsbHandle == INVALID_HANDLE_VALUE) {
            ShowError();
//            printf("CreateFile failed: for '%s'\n", path);
            continue;
        }

        HIDD_ATTRIBUTES attr;
        attr.Size = sizeof(attr);
        if(!getAttr(UsbHandle, &attr)) {
            ShowError();
//            printf("HidD_GetAttributes failed\n");
            continue;
        }

//        printf("VID: %04x PID %04x\n", attr.VendorID, attr.ProductID);

        if(attr.VendorID != OUR_VID || attr.ProductID != OUR_PID) {
            CloseHandle(UsbHandle);
//            printf("    nope, not us\n");
            continue;
        }

//        printf ("got it!\n");
        CloseHandle(UsbHandle);

        UsbHandle = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, NULL);

        if(UsbHandle == INVALID_HANDLE_VALUE) {
            ShowError();
//            printf("Error, couldn't open our own handle as desired.\n");
            return FALSE;
        }

        PHIDP_PREPARSED_DATA pp;
        getPreparsed(UsbHandle, &pp);
        HIDP_CAPS caps;

        if(getCaps(pp, &caps) != HIDP_STATUS_SUCCESS) {
//            printf("getcaps failed\n");
            return FALSE;
        }

//        printf("input/out report %d/%d\n", caps.InputReportByteLength,
//            caps.OutputReportByteLength);


        return TRUE;
    }
    return FALSE;

#else
    return UsbConnect3(OUR_VID, OUR_PID, &UsbHandle);
#endif
}

//-----------------------------------------------------------------------------
// Try to receive a command over USB. If we do, the copy it into *c and return
// TRUE; otherwise return FALSE.
//-----------------------------------------------------------------------------
static BOOL ReceiveCommandPoll(UsbCommand *c)
{
    static BOOL ReadInProgress = FALSE;
    static OVERLAPPED Ov;
    static BYTE Buf[65];
    static DWORD HaveRead;

    if(!ReadInProgress) {
        memset(&Ov, 0, sizeof(Ov));
        if(ReadFile(UsbHandle, Buf, 65, &HaveRead, &Ov))
        {
            memcpy(c, Buf+1, 64);
            return TRUE;
        }

        if(GetLastError() != ERROR_IO_PENDING) {
            ShowError();
            exit(-1);
        }
        ReadInProgress = TRUE;
    }

    if(HasOverlappedIoCompleted(&Ov)) {
        ReadInProgress = FALSE;

        if(!GetOverlappedResult(UsbHandle, &Ov, &HaveRead, FALSE)) {
            ShowError();
            exit(-1);
        }

        memcpy(c, Buf+1, 64);

        return TRUE;
    } else {
        return FALSE;
    }
}

//-----------------------------------------------------------------------------
// Block until we receive a command, and then return it in *c. Actually
// we are just spinning on ReceiveCommandPoll, but try not to chew up too
// much CPU while doing so.
//-----------------------------------------------------------------------------
static void ReceiveCommand(UsbCommand *c)
{
    while(!ReceiveCommandPoll(c)) {
        Sleep(0);
    }
}

//-----------------------------------------------------------------------------
// Send a command; if wantAck is true, then try to receive a command right
// after, and verify that it is an ACK (our higher-level ACK, not the USB
// ACK).
//-----------------------------------------------------------------------------
static void SendCommand(UsbCommand *c, BOOL wantAck)
{
    BYTE buf[65];
    buf[0] = 0;
    memcpy(buf+1, c, 64);

    DWORD written;
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    WriteFile(UsbHandle, buf, 65, &written, &ov);
    if(GetLastError() != ERROR_IO_PENDING) {
        ShowError();
        exit(-1);
    }
    
    while(!HasOverlappedIoCompleted(&ov)) {
        Sleep(0);
    }

    if(!GetOverlappedResult(UsbHandle, &ov, &written, FALSE)) {
        ShowError();
        exit(-1);
    }

    if(wantAck) {
        UsbCommand ack;
        ReceiveCommand(&ack);
        memcpy(c, &ack, sizeof(ack));
        if(ack.cmd != CMD_ACK) {
            printf("bad ACK\n");
            exit(-1);
        }
    }
}


// The address at which we expect to see the next byte in the S records file;
// we are assuming that the S records are in order.
static DWORD ExpectedAddr;

// The bytes that are currently queued to send; we wait until we get a full
// page's worth.
static BYTE QueuedToSend[256];

// This is TRUE if all of the bytes currently in QueuedToSend have already
// been written to the device, else FALSE.
static BOOL AllWritten;

static uint32_t feed_crc32(uint32_t crc, void* memory, unsigned int length)
{
    unsigned char* data = (unsigned char*)memory;
    crc = ~crc;

    if (data == 0 && length == 0xffffffff) {
        crc = length;
        length = 0;
    }

    for (uint32_t i = 0; i < length; ++i) {
        uint32_t byte = *data++;
        crc = crc ^ byte;

        for (int j = 7; j >= 0; j--) {    // Do eight times.
            uint32_t mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }

    return ~crc;
}

static uint32_t crc32(void* memory, unsigned int length)
{
    uint32_t crc;
    crc = feed_crc32(crc, 0, 0xffffffff);
    return feed_crc32(crc, memory, length);
}

//-----------------------------------------------------------------------------
// Called when we have accumulated a full page's worth of bytes in
// QueuedToSend; we copy them over to the device, and then tell the device
// to write them to flash.
//-----------------------------------------------------------------------------
static void FlushPrevious(void)
{
    UsbCommand c;
    memset(&c, 0, sizeof(c));

    //printf("expected = %08x flush\n", ExpectedAddr);

#if FLASH_PAGE_SIZE != 256
#error Fix download format for different page size!
#endif
    int i;
    for(i = 0; i < 240; i += 48) {
        c.cmd = CMD_SETUP_WRITE;
        memcpy(c.d.asBytes, QueuedToSend+i, 48);
        c.ext1 = (i/4);
        SendCommand(&c, TRUE);
        if (VerifyTransfers) {
            unsigned int crc = crc32(QueuedToSend+i, 48);
            if (crc != c.ext1)
                printf("\nUSB packet CRC32 mismatch on CMD_SETUP_WRITE!\n");
        }
    }

    c.cmd = CMD_FINISH_WRITE;
    c.ext1 = (ExpectedAddr-1) & (~255);
    //printf("    c.ext1 = %08x\n", c.ext1);
    printf(".");
    memcpy(c.d.asBytes, QueuedToSend+240, 16);
    SendCommand(&c, TRUE);
    if (VerifyTransfers) {
        unsigned int crc = crc32(QueuedToSend+240, 16);
        if (crc != c.ext1)
            printf("\nUSB packet CRC32 mismatch on CMD_FINISH_WRITE!\n");
    }

    AllWritten = TRUE;
}

//-----------------------------------------------------------------------------
// Called for each byte in the S records; make sure that it is at the
// address (where) where we expected it, and then tack it on to the page
// to be written that we are working on. If we have a full page of data
// ready to be written, call FlushPrevious() to write it and thus clear
// out our buffer for the next page.
//-----------------------------------------------------------------------------
static void GotByte(DWORD where, BYTE which)
{
    AllWritten = FALSE;

    if(where != ExpectedAddr) {
        printf("bad: got at %08x, expected at %08x\n", where, ExpectedAddr);
        exit(-1);
    }
    QueuedToSend[where & (FLASH_PAGE_SIZE-1)] = which;
    ExpectedAddr++;

    if((where & (FLASH_PAGE_SIZE-1)) == (FLASH_PAGE_SIZE-1)) {    
        // we have completed a full page
        FlushPrevious();
    }
}

//-----------------------------------------------------------------------------
// The integer value of a hex digit 0-9a-fA-F.
//-----------------------------------------------------------------------------
static int HexVal(int c)
{
    c = tolower(c);
    if(c >= '0' && c <= '9') {
        return c - '0';
    } else if(c >= 'a' && c <= 'f') {
        return (c - 'a') + 10;
    } else {
        printf("bad hex digit '%c'\n", c);
        exit(-1);
    }
}

//-----------------------------------------------------------------------------
// The integer value of the first two characters of a string, interepreted
// as an 8-bit hex quantity.
//-----------------------------------------------------------------------------
static BYTE HexByte(char *s)
{
    return (HexVal(s[0]) << 4) | HexVal(s[1]);
}

//-----------------------------------------------------------------------------
// Read S records from a file, and write them to the device. We verify that
// the file starts at the correct address, which is why we need to know
// whether the file being loaded is a bootrom or an application.
//-----------------------------------------------------------------------------
static void LoadFlashFromSRecords(char *file)
{
    uint32_t filesize = 0;
    uint32_t file_crc32 = 0;

    ExpectedAddr = 0x102000L;

    FILE *f = fopen(file, "r");
    if(!f) {
        printf("couldn't open file\n");
        exit(-1);
    }

    printf("Now uploading to: 0x%08x\n", ExpectedAddr);
    fflush(0);

    char line[512];
    file_crc32 = feed_crc32(file_crc32, 0, 0xffffffff);
    while(fgets(line, sizeof(line), f)) {
        if(memcmp(line, "S3", 2)==0) {
            char *s = line + 2;
            int len = HexByte(s) - 5;
            s += 2;

            char addrStr[9];
            memcpy(addrStr, s, 8);
            addrStr[8] = '\0';
            DWORD addr;
            sscanf(addrStr, "%x", &addr);
            s += 8;

            int i;
            for(i = 0; i < len; i++) {
                uint8_t v = 0xff;
                while((addr+i) > ExpectedAddr) {
                    GotByte(ExpectedAddr, 0xff);
                    file_crc32 = feed_crc32(file_crc32, &v, sizeof(v));
                    filesize++;
                }
                v = HexByte(s);
                GotByte(addr+i, v);
                file_crc32 = feed_crc32(file_crc32, &v, sizeof(v));
                filesize++;
                s += 2;
            }
        }
    }

    if(!AllWritten) FlushPrevious();

    fclose(f);
    printf("\nflashing done. size = %d bytes ; CRC32 = %08x\n", filesize, file_crc32);
    fflush(0);

    if (VerifyTransfers) {
        printf("Verifying firmware...\n");

        UsbCommand c;
        memset(&c, 0xfe, sizeof(c));
        c.cmd = CMD_CRC32_MEMORY;
        c.ext1 = 0x102000L;
        c.ext2 = filesize;
        SendCommand(&c, TRUE);

        if (file_crc32 != c.ext1) {
            printf("Firmware verification FAILED!\n");
            exit(-1);
        }

        printf("Firmware verified OK!\n");
    }

}

//-----------------------------------------------------------------------------
// Read binary data from a file, and write them to the device. 
//-----------------------------------------------------------------------------
static void LoadBootloaderFromBin(char *file, BOOL force)
{
    DWORD addr = 0;
    uint32_t filesize = 0;
    uint32_t file_crc32 = 0;

    ExpectedAddr = 0;

    FILE *f = fopen(file, "rb");
    if(!f) {
        printf("couldn't open file\n");
        exit(-1);
    }

    {
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);

        printf("Bootloader is %i bytes; ", filesize);

        void* mem = malloc(filesize);
        fread(mem, 1, filesize, f);
        fseek(f, 0, SEEK_SET);
        file_crc32 = crc32(mem, filesize);
        free(mem);

        printf("CRC32 is %08x\n", file_crc32);

        if (!force) {
            UsbCommand c;
            memset(&c, 0xfe, sizeof(c));
            c.cmd = CMD_CRC32_MEMORY;
            c.ext1 = 0x0;
            c.ext2 = filesize;
            SendCommand(&c, TRUE);

            printf("Existing bootloader CRC32 is %08x\n", c.ext1);

            if (file_crc32 == c.ext1) {
                printf("Existing bootloader is up to date - skipping\n");
                fclose(f);
                return;
            }
        } else {
            printf("Ignoring existing bootloader - forcing update\n");
        }

    }

    printf("Fixing bootloader...\n");
    fflush(0);

    while(!feof(f)) {
      BYTE c=fgetc(f);
      //printf("%04x %02x\n",addr,c);
      GotByte(addr, c);
      ++addr;
    }

    if(!AllWritten) FlushPrevious();

    fclose(f);
    printf("\nflashing done.\n");
    fflush(0);

    if (VerifyTransfers) {
        printf("Verifying bootloader...\n");

        UsbCommand c;
        memset(&c, 0xfe, sizeof(c));
        c.cmd = CMD_CRC32_MEMORY;
        c.ext1 = 0x0;
        c.ext2 = filesize;
        SendCommand(&c, TRUE);

        if (file_crc32 != c.ext1) {
            printf("Bootloader verification FAILED!\n");
            exit(-1);
        }

        printf("Bootloader verified OK!\n");
    }
}

int main(int argc, char **argv)
{
    int i = 0;
    uint32_t bootloader_version = 0x0;
    uint32_t bootloader_size = 0x0;
    uint32_t firmware_size = 0x0;

    if(argc < 2) {
        printf("Usage: %s load    <application>.s19\n", argv[0]);
        return -1;
    }
    if(strcmp(argv[1], "info") && argc != 3) {
        printf("Need filename.\n");
        return -1;
    }

    if( strcmp(argv[1], "full")==0 ||
        strcmp(argv[1], "load")==0 || 
        strcmp(argv[1], "info")==0 ) {

        for(;;) {
            if(UsbConnect()) {
                break;
            }
            if(i == 0) {
                printf("No device connected, polling for it now...\n");
                fflush(0);
            }
            if(i > 50000) {
                printf("...could not connect to USB device; exiting.\n");
                return -1;
            }
            i++;
            Sleep(5);
        }

        printf("Device connected - quering version...\n");

        UsbCommand c;
        memset(&c, 0xfe, sizeof(c));
        c.cmd = CMD_DEVICE_INFO;
        SendCommand(&c, TRUE);
        if (c.ext1 != 0xfefefefe) {
            bootloader_version = c.ext1;
            bootloader_size = c.ext2;
            firmware_size = c.ext3;
            VerifyTransfers = 1;
        }

        printf("Bootloader version : %08x\n", bootloader_version);

        if (strcmp(argv[1], "info")==0) {

            if (bootloader_version == 0) {
                printf("Command not supported - bootloader too old\n");
                return 0;
            }

            if (!bootloader_size) {
                printf("Unknown bootloader size - too old?\n");
            } else {

                bootloader_size += 0x200; // add size of first stage bootrom
                printf("Bootloader size : %d bytes\n", bootloader_size);

                memset(&c, 0xfe, sizeof(c));
                c.cmd = CMD_CRC32_MEMORY;
                c.ext1 = 0x0;
                c.ext2 = bootloader_size;
                SendCommand(&c, TRUE);
                printf("Bootloader CRC32: %08x\n", c.ext1);
            }

            if (!firmware_size) {
                printf("Unknown firmware size - too old\n");
            } else {

                printf("Firmware size : %d bytes\n", firmware_size);

                memset(&c, 0xfe, sizeof(c));
                c.cmd = CMD_CRC32_MEMORY;
                c.ext1 = 0x102000;
                c.ext2 = firmware_size;
                SendCommand(&c, TRUE);
                printf("Firmware CRC32: %08x\n", c.ext1);
            }

            return 0;
        }

        if(strcmp(argv[1], "full")==0) {
            LoadBootloaderFromBin("bootrom.bin", bootloader_version == 0x0);
        }

        LoadFlashFromSRecords(argv[2]);

    } else {
        printf("Command '%s' not recognized.\n", argv[1]);
        return -1;
    }

    return 0;
}
