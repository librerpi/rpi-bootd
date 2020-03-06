#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

bool  is_daemon  = true;
bool  verbose = false;
char* uart_device = "/dev/ttyUSB0";
char* log_file    = "./rpibootd.log";

void daemonize() {
}

void usage() {
	printf("rpibootd [-D] [-v] [-h] [-d device] [-l file]\n");
	printf("\n");
	printf("\t -D start in debug mode, do not detach from terminal\n");
	printf("\t -v verbose logging\n");
	printf("\t -h display this usage information\n");
	printf("\t -d device\n");
	printf("\t    set the UART device to use to communicate with the raspberry pi\n");
	printf("\t    e.g -d /dev/ttyUSB0\n");
	printf("\t -l file\n");
	printf("\t    set name of log file\n");
}

int main(int argc, char** argv) {
	int c;
	while ((c = getopt(argc, argv, "Dvhd:l:")) != -1) {
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
		}
	}
	// TODO: worker thread pool
	usbboot_init();
	for(;;);

}
