#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

bool  is_daemon   = true;
bool  verbose     = false;
char* uart_device = "/dev/ttyUSB0";
char* bootcode    = "./bootcode.bin";
char* log_file    = "./rpibootd.log";
FILE* log_fd      = NULL;

void daemonize() {
	pid_t pid = fork();
	if(fork < 0) {
		fprintf(stderr,"Fork failed!\n");
		exit(1);
	}
	if(pid >0) {
		exit(0);
	}
	umask(0);
	pid_t sid = setsid();
	if(sid < 0) {
		exit(1);
	}
	chdir("/");
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

void log_info(const char *fmt, ...) {
	time_t current_time = time(NULL);
	char formatted_time[32];
	strftime(formatted_time,32,"%d/%b/%Y:%H:%M:%S %z", localtime(&current_time));

	va_list args;
	va_start(args, fmt);
	fprintf(log_fd,"[%s] ", formatted_time);
	vfprintf(log_fd, fmt, args);
	fprintf(log_fd,"\n");
	fflush(log_fd);
	va_end(args);
}

void init_logging() {
	char* log_file_path = realpath(log_file, NULL);
	if(log_file_path == NULL) {
		log_fd = fopen(log_file,"w");
		fclose(log_fd);
		log_file_path = realpath(log_file, NULL);
	}
	log_file            = log_file_path;
	if(is_daemon) {
		log_fd = fopen(log_file,"w+");
	} else {
		log_fd = stderr;
	}
	log_info("Starting rpibootd");
}

void exit_logging() {
	fflush(log_fd);
	fclose(log_fd);
}
