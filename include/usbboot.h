#pragma once
#include <libusb-1.0/libusb.h>
#include <event2/event.h>
#include <event2/util.h>

int usbboot_init(struct event_base* base);
void usbboot_shutdown();
void usbboot_exit();
