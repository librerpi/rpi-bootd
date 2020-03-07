#pragma once

#include <stdbool.h>

extern bool  is_daemon;
extern bool  verbose;
extern char* uart_device;
extern char* log_file;
extern char* bootcode;
extern FILE* log_fd;

void daemonize();
void log_info(const char *fmt, ...);
void init_logging();
void exit_logging();
