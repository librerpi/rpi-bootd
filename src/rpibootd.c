#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <utils.h>
#include <usbboot.h>
#include <event2/event.h>

unsigned int portnum = 1337;

struct event_base* base;

void init_rpibootd() {
	init_logging();
	if(is_daemon) daemonize();
	base = event_base_new();
	
	usbboot_init(base);
}

void exit_rpibootd() {
	usbboot_exit();
	exit_logging();
}

void usage() {
	printf("rpibootd [-D] [-v] [-h] [-d device] [-l file] [-p port]\n");
	printf("\n");
	printf("\t -D start in debug mode, do not detach from terminal\n");
	printf("\t -v verbose logging\n");
	printf("\t -h display this usage information\n");
	printf("\t -d device\n");
	printf("\t    set the UART device to use to communicate with the raspberry pi\n");
	printf("\t    e.g -d /dev/ttyUSB0\n");
	printf("\t -l file\n");
	printf("\t    set name of log file\n");
	printf("\t -b bootcode.bin\n");
	printf("\t    specify which bootcode.bin file to send\n");
	printf("\t -p port\n");
	printf("\t    specify TCP port to listen on for console\n");
}

int main(int argc, char** argv) {

	int c;
	while ((c = getopt(argc, argv, "Dvhd:l:p:")) != -1) {
		switch(c) {
			case 'D':
				is_daemon = false;
				break;
			case 'v':
				verbose = true;
			break;
			case 'h':
				usage();
				exit(0);
			break;
			case 'd':
				uart_device = malloc(strlen(optarg)+1);
				sprintf(uart_device,"%s",uart_device);
			break;
			case 'l':
				log_file = malloc(strlen(optarg)+1);
				sprintf(log_file, "%s", log_file);
			break;
			case 'b':
				bootcode = malloc(strlen(optarg)+1);
				sprintf(bootcode, "%s", bootcode);
			break;
			case 'p':
				portnum = atoi(optarg);
			break;
		}
	}
	char* uart_dev_path = realpath(uart_device, NULL);
	uart_device         = uart_dev_path;

	char* bootcode_path = realpath(bootcode, NULL);
	bootcode            = bootcode_path;
	// TODO: worker thread pool
	init_rpibootd();

	for(;;) {
		event_base_dispatch(base);
	}
}
