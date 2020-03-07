#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils.h>

#define RASPI_VENDOR_ID 0x0a5c
#define LIBUSB_MAX_TRANSFER (16 * 1024)

static int out_ep;
static int in_ep;

int ep_write(void *buf, int len, libusb_device_handle * usb_device)
{
	int a_len = 0;
	int sending, sent;
	int ret =
	    libusb_control_transfer(usb_device, LIBUSB_REQUEST_TYPE_VENDOR, 0,
				    len & 0xffff, len >> 16, NULL, 0, 1000);

	if(ret != 0)
	{
		printf("Failed control transfer (%d,%d)\n", ret, len);
		return ret;
	}

	while(len > 0)
	{
		sending = len < LIBUSB_MAX_TRANSFER ? len : LIBUSB_MAX_TRANSFER;
		ret = libusb_bulk_transfer(usb_device, out_ep, buf, sending, &sent, 5000);
		if (ret)
			break;
		a_len += sent;
		buf += sent;
		len -= sent;
	}
	fprintf(stderr,"libusb_bulk_transfer sent %d bytes; returned %d\n", a_len, ret);

	return a_len;
}

int ep_read(void *buf, int len, libusb_device_handle * usb_device)
{
	int ret =
	    libusb_control_transfer(usb_device,
				    LIBUSB_REQUEST_TYPE_VENDOR |
				    LIBUSB_ENDPOINT_IN, 0, len & 0xffff,
				    len >> 16, buf, len, 3000);
	if(ret >= 0)
		return len;
	else
		return ret;
}

typedef struct MESSAGE_S {
		int length;
		unsigned char signature[20];
} boot_message_t;

boot_message_t boot_message;
void *second_stage_txbuf;

int second_stage_prep(FILE *fp, FILE *fp_sig)
{
	int size;

	fseek(fp, 0, SEEK_END);
	boot_message.length = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(fp_sig != NULL)
	{
		fread(boot_message.signature, 1, sizeof(boot_message.signature), fp_sig);
	}

	second_stage_txbuf = (uint8_t *) malloc(boot_message.length);
	if (second_stage_txbuf == NULL)
	{
		printf("Failed to allocate memory\n");
		return -1;
	}

	size = fread(second_stage_txbuf, 1, boot_message.length, fp);
	if(size != boot_message.length)
	{
		printf("Failed to read second stage\n");
		return -1;
	}

	return 0;
}


int second_stage_boot(libusb_device_handle *usb_device)
{
	int size, retcode = 0;

	size = ep_write(&boot_message, sizeof(boot_message), usb_device);
	if (size != sizeof(boot_message))
	{
		printf("Failed to write correct length, returned %d\n", size);
		return -1;
	}

	fprintf(stderr,"Writing %d bytes\n", boot_message.length);
	size = ep_write(second_stage_txbuf, boot_message.length, usb_device);
	if (size != boot_message.length)
	{
		printf("Failed to read correct length, returned %d\n", size);
		return -1;
	}

	sleep(1);
	size = ep_read((unsigned char *)&retcode, sizeof(retcode), usb_device);

	if (size > 0 && retcode == 0)
	{
		printf("Successful read %d bytes \n", size);
	}
	else
	{
		printf("Failed : 0x%x\n", retcode);
	}

	return retcode;

}

static void usbboot_init_raspi(libusb_device_handle *dev_handle) {
	struct libusb_config_descriptor *config;
	libusb_get_active_config_descriptor(libusb_get_device(dev_handle),&config);

	int interface;

	if(config->bNumInterfaces == 1) {
		interface = 0;
		out_ep    = 1;
		in_ep     = 2;
	} else {
		interface = 1;
		out_ep    = 3;
		in_ep     = 4;
	}

	if(libusb_claim_interface(dev_handle, interface)) {
		fprintf(stderr,"Failed to claim interface!\n");
		libusb_close(dev_handle);
	}
}

static libusb_device_handle *handle = NULL;

static int LIBUSB_CALL usbboot_hotplug_cb(libusb_context *ctx, libusb_device *dev,
			libusb_hotplug_event event, void* user_data) {
	struct libusb_device_descriptor  desc;
	
	libusb_get_device_descriptor(dev, &desc);

	int rc;

	if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
		rc = libusb_open(dev, &handle);
		if (rc != LIBUSB_SUCCESS) {
			fprintf(stderr, "Could not open USB device!\n");
		} else {
			fprintf(stderr, "Found device!\n");

		}
	} else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
		if(handle) {
			fprintf(stderr, "Closing device!\n");
			libusb_close(handle);
			handle = NULL;
		}
	} else {
		fprintf(stderr, "Unknown event %d\n", event);
	}
	return 0;

}

static libusb_hotplug_callback_handle callback;

int usbboot_init() {
	libusb_init(NULL);

	libusb_hotplug_register_callback(NULL,LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
					LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0,
					RASPI_VENDOR_ID,
					LIBUSB_HOTPLUG_MATCH_ANY,
					LIBUSB_HOTPLUG_MATCH_ANY, usbboot_hotplug_cb, NULL, &callback);


	FILE* fp = fopen(bootcode,"rb");
	second_stage_prep(fp, NULL);
	for(;;) {
		libusb_handle_events(NULL);
		if(handle) {
			usbboot_init_raspi(handle);
			second_stage_boot(handle);
			break;
		}
	}
}

void usbboot_shutdown() {
	libusb_exit (NULL);
}
