#include <libusb-1.0/libusb.h>
#include <event2/event.h>
#include <event2/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils.h>
#include <unistd.h>

#define RASPI_VENDOR_ID 0x0a5c
#define LIBUSB_MAX_TRANSFER (16 * 1024)

static int out_ep;
static int in_ep;

int ep_write(void *buf, int len, libusb_device_handle * usb_device)
{
	int a_len = 0;
	int sending = 0;
	int sent = 0;
	int ret =
	    libusb_control_transfer(usb_device, LIBUSB_REQUEST_TYPE_VENDOR, 0,
				    len & 0xffff, len >> 16, NULL, 0, 1000);

	if(ret != 0)
	{
		log_info("Failed control transfer (%d,%d)\n", ret, len);
		return ret;
	}

	a_len   = 0;
	sent    = 0;
	sending = 0;
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
	log_info("libusb_bulk_transfer sent %d bytes; returned %d\n", a_len, ret);

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
		log_info("Failed to allocate memory\n");
		return -1;
	}

	size = fread(second_stage_txbuf, 1, boot_message.length, fp);
	if(size != boot_message.length)
	{
		log_info("Failed to read second stage\n");
		return -1;
	}

	return 0;
}

void async_bootmsg_send_data(struct libusb_transfer *xfr);

// TODO - write a generic function for building control transfers and bulk transfers, with arbitrary user data and callback

// starts an async boot
void async_boot_start(libusb_device_handle *usb_device) {
     log_info("ASync boot started");
     struct libusb_transfer *xfr;

     // we start by sending the boot message control transfer
     xfr = libusb_alloc_transfer(0);
     if(!xfr) {
	log_info("Async boot: failed to allocate transfer!");
	return;
     }

     unsigned char* buffer = malloc(LIBUSB_CONTROL_SETUP_SIZE);
     if(!buffer) {
	log_info("Async boot: failed to allocate buffer!");
	return;
     }

     int len = sizeof(boot_message);

     libusb_fill_control_setup(buffer, LIBUSB_REQUEST_TYPE_VENDOR, 0, len & 0xffff, len >> 16, 0);
     libusb_fill_control_transfer(xfr, usb_device, buffer, async_bootmsg_send_data, &boot_message, 1000);
     xfr->flags = LIBUSB_TRANSFER_FREE_BUFFER;

     int ret = libusb_submit_transfer(xfr);
     if(ret < 0) {
	libusb_free_transfer(xfr);
     }
//     int ret =
//	    libusb_control_transfer(usb_device, LIBUSB_REQUEST_TYPE_VENDOR, 0,
//				    len & 0xffff, len >> 16, NULL, 0, 1000);

     
}

void async_bootcode_send(struct libusb_transfer *xfr);

void async_bootcode_send_data(struct libusb_transfer *xfr);
// send the boot message as a single bulk transfer
void async_bootmsg_send_data(struct libusb_transfer *xfr) {
	log_info("Async bootmsg_send_data");
	struct libusb_transfer *bootmsg_bulk_transfer; // don't want to confuse with xfr

	bootmsg_bulk_transfer = libusb_alloc_transfer(0);
	if(!bootmsg_bulk_transfer) {
	   log_info("Async boot: failed to allocate bulk transfer");
	}
	libusb_fill_bulk_transfer(bootmsg_bulk_transfer, xfr->dev_handle, out_ep, (unsigned char*)&boot_message, sizeof(boot_message), async_bootcode_send, NULL, 1000);
	bootmsg_bulk_transfer->type = LIBUSB_TRANSFER_TYPE_BULK;
	int ret = libusb_submit_transfer(bootmsg_bulk_transfer);
        if(ret < 0) {
	   libusb_free_transfer(bootmsg_bulk_transfer);
	}
	libusb_free_transfer(xfr);
}

// called when a message is read
void async_boot_recv_cb(struct libusb_transfer *xfr) {
}

void async_bootcode_send(struct libusb_transfer *xfr) {
     log_info("async_bootcode_send");
   	struct libusb_transfer *bootcode_xfr;

     bootcode_xfr = libusb_alloc_transfer(0);
     if(!bootcode_xfr) {
	log_info("Async boot: failed to allocate transfer!");
	return;
     }

     unsigned char* buffer = malloc(LIBUSB_CONTROL_SETUP_SIZE);
     if(!buffer) {
	log_info("Async boot: failed to allocate buffer!");
	return;
     }

     int len = boot_message.length;

     libusb_fill_control_setup(buffer, LIBUSB_REQUEST_TYPE_VENDOR, 0, len & 0xffff, len >> 16, 0);
     libusb_fill_control_transfer(bootcode_xfr, xfr->dev_handle, buffer, async_bootcode_send_data, NULL, 1000);
     bootcode_xfr->flags = LIBUSB_TRANSFER_FREE_BUFFER;

     int ret = libusb_submit_transfer(bootcode_xfr);
     if(ret < 0) {
	libusb_free_transfer(bootcode_xfr);
     }
     libusb_free_transfer(xfr);
}

void async_boot_finish(struct libusb_transfer *xfr);

void async_bootcode_send_data(struct libusb_transfer *xfr) {
	log_info("Async bootcode_send_data");
	struct libusb_transfer *bootcode_bulk_transfer; // don't want to confuse with xfr

	bootcode_bulk_transfer = libusb_alloc_transfer(0);
	if(!bootcode_bulk_transfer) {
	   log_info("Async boot: failed to allocate bulk transfer");
	}
	libusb_fill_bulk_transfer(bootcode_bulk_transfer, xfr->dev_handle, out_ep, (unsigned char*)second_stage_txbuf, boot_message.length, async_boot_finish, NULL, 20000);
	bootcode_bulk_transfer->type = LIBUSB_TRANSFER_TYPE_BULK;
	int ret = libusb_submit_transfer(bootcode_bulk_transfer);
        if(ret < 0) {
	   libusb_free_transfer(bootcode_bulk_transfer);
	}
	libusb_free_transfer(xfr);

}

bool async_done = false;

void async_boot_finish(struct libusb_transfer* xfr) {
	async_done = true;
	return;
}

int second_stage_boot(libusb_device_handle *usb_device)
{
	int size, retcode = 0;

	/*size = ep_write(&boot_message, sizeof(boot_message), usb_device);
	if (size != sizeof(boot_message))
	{
		log_info("Failed to write correct length, returned %d\n", size);
		return -1;
	}*/

	/*log_info("Writing %d bytes\n", boot_message.length);
	size = ep_write(second_stage_txbuf, boot_message.length, usb_device);
	if (size != boot_message.length)
	{
		log_info("Failed to read correct length, returned %d\n", size);
		return -1;
	}*/

//	sleep(1);
	size = ep_read((unsigned char *)&retcode, sizeof(retcode), usb_device);

	if (size > 0 && retcode == 0)
	{
		log_info("Successful read %d bytes \n", size);
	}
	else
	{
		log_info("Failed : 0x%x\n", retcode);
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
		log_info("Failed to claim interface!\n");
		libusb_close(dev_handle);
	}
}

static libusb_device_handle *handle = NULL;
static 	struct libusb_device_descriptor  desc;

static int LIBUSB_CALL usbboot_hotplug_cb(libusb_context *ctx, libusb_device *dev,
			libusb_hotplug_event event, void* user_data) {

	
	libusb_get_device_descriptor(dev, &desc);

	int rc;

	if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
		rc = libusb_open(dev, &handle);
		if (rc != LIBUSB_SUCCESS) {
			log_info("Could not open USB device!\n");
		} else {
			log_info("Found device!\n");
			usbboot_init_raspi(handle);
			if(desc.iSerialNumber ==0 || desc.iSerialNumber == 3) async_boot_start(handle);
		}
	} else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
		if(handle) {
			log_info( "Closing device!\n");
			libusb_close(handle);
			handle = NULL;
		}
	} else {
		log_info("Unknown event %d\n", event);
	}
	return 0;

}

static libusb_hotplug_callback_handle callback;
static struct timeval zero_tv;

void timer_libusb_cb(evutil_socket_t fd, short what, void* arg) {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	libusb_handle_events_timeout_completed(NULL,&tv,NULL);
	evtimer_add((struct event*)arg,&tv);
		if(handle && async_done) {
			if(desc.iSerialNumber == 0 || desc.iSerialNumber == 3) {
				second_stage_boot(handle);
				libusb_close(handle);
				handle = NULL;
			}
		}


}



int usbboot_init(struct event_base* base) {
	libusb_init(NULL);

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1;

	struct event* usb_cb_ev = evtimer_new(base, timer_libusb_cb, event_self_cbarg());
	evtimer_add(usb_cb_ev, &tv);

	FILE* fp = fopen(bootcode,"rb");
	second_stage_prep(fp, NULL);

	libusb_hotplug_register_callback(NULL,LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
					LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0,
					RASPI_VENDOR_ID,
					LIBUSB_HOTPLUG_MATCH_ANY,
					LIBUSB_HOTPLUG_MATCH_ANY, usbboot_hotplug_cb, NULL, &callback);


/*	FILE* fp = fopen(bootcode,"rb");
	second_stage_prep(fp, NULL);
	for(;;) {
		libusb_handle_events_timeout_completed(NULL,&zero_tv,NULL);
	}*/
}

void usbboot_exit() {
	libusb_exit (NULL);
}
