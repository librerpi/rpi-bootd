# rpi-bootd
This is an in-development boot server for the raspberry pi that manages USB booting and UART (sending kernels over UART and console I/O) all in one place.

# planned features / TODO

1. USB boot support, loading bootcode.bin from the server on an attached raspi
2. UART console I/O via telnet
3. UART kernel loading using the raspbootin or bootboot protocol
4. With DTR of a USB FTDI device connected to RUN on the raspi, reset via SIGHUP
5. Seperate monitor interface via telnet
6. Programmatic usage of the monitor and console
7. GDB support, extracting GDB packets for a seperate TCP socket
8. Support for halting the raspi without resetting
