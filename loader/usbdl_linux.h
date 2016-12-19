#pragma once

#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libusb.h>
#include <pthread.h>

typedef unsigned char BYTE;
typedef signed int DWORD;
typedef void* LPVOID;

typedef int BOOL;
typedef void* HANDLE;

#define FORMAT_MESSAGE_FROM_SYSTEM 10
#define ERROR_IO_PENDING 997

#define FALSE 0
#define TRUE (!(FALSE))


typedef struct _OVERLAPPED { } OVERLAPPED, *LPOVERLAPPED;

static volatile int32_t bytes_transferred = 0;
static uint8_t buffer[64];

static void transfer_callback(struct libusb_transfer *transfer)
{
	if (transfer->status == LIBUSB_TRANSFER_CANCELLED)
		return;

	if (transfer->status == LIBUSB_TRANSFER_TIMED_OUT ||
		transfer->status == LIBUSB_TRANSFER_STALL)
	{
		printf("r");
		libusb_submit_transfer(transfer);
		return;
	}

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		return;

	if (bytes_transferred)
	{
		printf("unexpected callback!\n");
		exit(-1);
	}

	bytes_transferred = transfer->actual_length;
}

static int submit_transfer(libusb_context* context, struct libusb_transfer* transfer)
{
	bytes_transferred = 0;
	int res = libusb_submit_transfer(transfer);
	if (res)
	{
		fprintf(stderr, "libusb_submit_transfer failed\n");
		return res;
	}

	while (!bytes_transferred)
	{
		res = libusb_handle_events(context);
		if (res)
		{
			fprintf(stderr, "libusb_handle_events failed\n");
			return res;
		}
	}
	return 0;
}

static libusb_context *usb_context = NULL;
static struct libusb_transfer* read_transfer = NULL;
static struct libusb_transfer* write_transfer = NULL;

static BOOL UsbConnect3(uint32_t vid, uint32_t pid, HANDLE* UsbHandle)
{
	*UsbHandle = NULL;

	if (usb_context == NULL)
	{
		if (libusb_init(&usb_context))
		{
			fprintf(stderr, "libusb_init failed\n");
			return FALSE;
		}
	}

//	libusb_set_debug(usb_context, /*LIBUSB_LOG_LEVEL_WARNING */LIBUSB_LOG_LEVEL_DEBUG);

	libusb_device** devs;
	ssize_t num_devs = libusb_get_device_list(usb_context, &devs);
	if (num_devs < 0)
	{
		fprintf(stderr, "libusb_get_device_list failed\n");
		goto error;
	}

	for (int i = 0; i < num_devs; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		if (libusb_get_device_descriptor(dev, &desc))
		{
			fprintf(stderr, "libusb_get_device_descriptor failed\n");
			goto error;
		}

		if (desc.idVendor != vid || desc.idProduct != pid)
			continue;

		printf("found %04x %04x\n", desc.idVendor, desc.idProduct);

		struct libusb_config_descriptor *conf_desc = NULL;
		if (libusb_get_active_config_descriptor(dev, &conf_desc))
			libusb_get_config_descriptor(dev, 0, &conf_desc);

		if (!conf_desc)
			continue;

		int interface_num = -1;
		int input_endpoint = 0;
		int output_endpoint = 0;
		libusb_device_handle *handle = NULL;

		for (int j = 0; j < conf_desc->bNumInterfaces; j++)
		{
			const struct libusb_interface *intf = &conf_desc->interface[j];
			for (int k = 0; k < intf->num_altsetting; k++)
			{
				const struct libusb_interface_descriptor *intf_desc = &intf->altsetting[k];
				if (intf_desc->bInterfaceClass == LIBUSB_CLASS_HID)
				{
					interface_num = intf_desc->bInterfaceNumber;
					for (int l = 0; l < intf_desc->bNumEndpoints; ++l)
					{
						const struct libusb_endpoint_descriptor* ep = &intf_desc->endpoint[l];
						if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) != LIBUSB_TRANSFER_TYPE_INTERRUPT)
							continue;

						if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
							input_endpoint = input_endpoint ? input_endpoint : ep->bEndpointAddress;
						if ((ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT)
							output_endpoint = output_endpoint ? output_endpoint : ep->bEndpointAddress;
					}
					goto interface_found;
				}
			}
		}

interface_found:
		printf("interface : %i\n", interface_num);
		printf("input_endpoint : %02x\n", input_endpoint);
		printf("output_endpoint : %02x\n", output_endpoint);

		static const char* speed_names[] = { "unknown", "low", "full", "high", "super" };
		uint32_t speed = libusb_get_device_speed(dev);
		printf("speed : %s\n", speed_names[speed > LIBUSB_SPEED_SUPER ? 0 : speed]);

		if (interface_num >= 0)
		{
			int res = libusb_open(dev, &handle);
			if (res == LIBUSB_ERROR_ACCESS)
			{
				fprintf(stderr, "\nlibusb_open failed - are you running as 'root'?\n");
				fprintf(stderr, "try 'sudo' or run\n");
				fprintf(stderr, "\t$ echo 'SUBSYSTEM==\"usb\", ATTR{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", MODE=\"0666\", GROUP=\"plugdev\"'", desc.idVendor, desc.idProduct);
				fprintf(stderr, " | sudo tee -a /etc/udev/rules.d/75-usbdl.rules\n");
				fprintf(stderr, "\t$ sudo service udev restart\n");
				fprintf(stderr, "and try again\n");
				exit(-1);
			}
			if (res)
				fprintf(stderr, "libusb_open failed\n");
		}

		if (handle && libusb_kernel_driver_active(handle, interface_num) == 1)
		{
			if (libusb_detach_kernel_driver(handle, interface_num))
			{
				libusb_close(handle);
				handle = NULL;
				fprintf(stderr, "libusb_detach_kernel_driver failed\n");
			}
		}

		if (handle && libusb_claim_interface(handle, interface_num))
		{
			libusb_close(handle);
			handle = NULL;
			fprintf(stderr, "libusb_claim_interface failed\n");
		}

		if (handle)
		{
			int timeout = 100;
			read_transfer = libusb_alloc_transfer(0);
			libusb_fill_interrupt_transfer(	read_transfer,
											handle,
											input_endpoint,
											buffer,
											sizeof(buffer),
											transfer_callback,
											0,
											timeout);

			write_transfer = libusb_alloc_transfer(0);
			libusb_fill_interrupt_transfer( write_transfer,
											handle,
											output_endpoint,
											buffer,
											sizeof(buffer),
											transfer_callback,
											0,
											timeout);
		}

		*UsbHandle = handle;

		libusb_free_config_descriptor(conf_desc);

		if (*UsbHandle)
			break;
	}

	libusb_free_device_list(devs, TRUE);

	return (*UsbHandle != NULL);

error:

	if (usb_context)
		libusb_exit(usb_context);
	usb_context = NULL;
	return FALSE;
}

void Sleep(DWORD dwMilliseconds)
{
	usleep(dwMilliseconds * 1000);
}


BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped )
{
	if (submit_transfer(usb_context, read_transfer))
		return FALSE;

	size_t bytes_to_copy = nNumberOfBytesToRead < bytes_transferred ? nNumberOfBytesToRead : bytes_transferred;
	memcpy(lpBuffer+1, buffer, bytes_to_copy);
	((char*)lpBuffer)[0] = 0;

	if (lpNumberOfBytesRead)
		*lpNumberOfBytesRead = bytes_to_copy+1;

	bytes_transferred = 0;
	fflush(stdout);
	return TRUE;
}

BOOL WriteFile(HANDLE hFile, const LPVOID lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	nNumberOfBytesToWrite--;

	size_t bytes_to_copy = nNumberOfBytesToWrite < sizeof(buffer) ? nNumberOfBytesToWrite : sizeof(buffer);
	memcpy(buffer, lpBuffer+1, bytes_to_copy);

	if (submit_transfer(usb_context, write_transfer))
		return FALSE;

	if (lpNumberOfBytesWritten)
		*lpNumberOfBytesWritten = bytes_transferred+1;

	bytes_transferred = 0;
	fflush(stdout);
	return TRUE;
}

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, DWORD* lpNumberOfBytesTransferred, BOOL bWait) {
	return TRUE;
}

BOOL HasOverlappedIoCompleted(LPOVERLAPPED lpOverlapped) {
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
