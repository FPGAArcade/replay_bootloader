#pragma once

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <CoreFoundation/CoreFoundation.h>

typedef unsigned char BYTE;
typedef unsigned int DWORD;

typedef int BOOL;
typedef void* HANDLE;

#define FORMAT_MESSAGE_FROM_SYSTEM 10
#define ERROR_IO_PENDING 997

typedef struct _OVERLAPPED { } OVERLAPPED, *LPOVERLAPPED;

static volatile int32_t bytes_available = 0;
static uint8_t buffer[65];

static void ProcessEvents(void) {
	SInt32 res;
	do {
		res = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, FALSE);
	} while(res != kCFRunLoopRunFinished && res != kCFRunLoopRunTimedOut);
}

static void ReportCallback(void *context, IOReturn result, void *sender, IOHIDReportType report_type, uint32_t report_id, uint8_t *report, CFIndex report_length) {
	if (bytes_available)
	{
		printf("unexpected packet!\n");
		exit(-1);
	}

	buffer[0] = report_id;
	size_t bytes_to_copy = report_length > sizeof(buffer)-1 ? sizeof(buffer)-1 : report_length;
	memcpy(buffer+1, report, bytes_to_copy);

	bytes_available = bytes_to_copy+1;
}

static BOOL UsbConnect3(uint32_t vid, uint32_t pid, HANDLE* UsbHandle)
{
	CFIndex num_devices;
	IOReturn res;
	static uint8_t report_buf[1024];

	IOHIDManagerRef hid_mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!hid_mgr) {
		printf("unable to create!\n");
		goto error;
	}

	IOHIDManagerScheduleWithRunLoop(hid_mgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	{
		CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFNumberRef vidNumberRef = CFNumberCreate(NULL, kCFNumberIntType, &vid );
		CFNumberRef pidNumberRef = CFNumberCreate(NULL, kCFNumberIntType, &pid );
		CFDictionarySetValue(dict, CFSTR( kIOHIDVendorIDKey ), vidNumberRef);
		CFDictionarySetValue(dict, CFSTR( kIOHIDProductIDKey ), pidNumberRef);
		CFRelease(vidNumberRef);
		CFRelease(pidNumberRef);
		IOHIDManagerSetDeviceMatching(hid_mgr, dict);
	}
	res = IOHIDManagerOpen(hid_mgr, kIOHIDOptionsTypeSeizeDevice);
	if (res != kIOReturnSuccess) {
		printf("unable to open!\n");
		goto error;
	}

	CFSetRef device_set = IOHIDManagerCopyDevices(hid_mgr);

	if (!device_set) {
//		printf("no device found!\n");
		goto error;
	}

	num_devices = CFSetGetCount(device_set);
	if (num_devices < 1 || num_devices > 1)
	{
		printf("incorrect number of devices!\n");
		goto error;
	}
	IOHIDDeviceRef device;
	CFSetGetValues(device_set, (const void **) &device);

	IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDDeviceRegisterInputReportCallback(device, report_buf, sizeof(report_buf), &ReportCallback, 0);

	*UsbHandle = device;
	return TRUE;

error:
	IOHIDManagerClose(hid_mgr, kIOHIDOptionsTypeNone);
	CFRelease(hid_mgr);
	return FALSE;
}

void Sleep(DWORD dwMilliseconds)
{
	usleep(dwMilliseconds * 1000);
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped )
{
	while (!bytes_available)
		ProcessEvents();

	size_t bytes_to_copy = nNumberOfBytesToRead < bytes_available ? nNumberOfBytesToRead : bytes_available;
	memcpy(lpBuffer, buffer, bytes_to_copy);

	if (lpNumberOfBytesRead)
		*lpNumberOfBytesRead = bytes_to_copy;

	bytes_available = 0;

	return TRUE;
}

BOOL WriteFile(HANDLE hFile, const LPVOID lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	const uint8_t *data = lpBuffer;
	CFIndex length = nNumberOfBytesToWrite;
	IOReturn tIOReturn = IOHIDDeviceSetReport(hFile, kIOHIDReportTypeOutput, data[0], data+1, length-1);
	if (lpNumberOfBytesWritten)
		*lpNumberOfBytesWritten = length;
 	fflush(stdout);
	return tIOReturn == kIOReturnSuccess;
}

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, DWORD* lpNumberOfBytesTransferred, BOOL bWait) {
	ProcessEvents();
	return TRUE;
}

BOOL HasOverlappedIoCompleted(LPOVERLAPPED lpOverlapped) {
	ProcessEvents();
	return TRUE;
}

DWORD FormatMessage(DWORD dwFlags, const LPVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, char* lpBuffer, DWORD nSize, va_list *Arguments) {
	if( !strerror_r(dwMessageId, lpBuffer, nSize))
		return strlen(lpBuffer);
	return 0;
}

DWORD GetLastError(void) {
	return ERROR_IO_PENDING;
}
