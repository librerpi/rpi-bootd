CC=gcc

all: rpibootd

clean:
	rm -f src/*.o
	rm -f rpibootd


src/%.o: src/%.c
	$(CC) -c -g -I/usr/local/include -Iinclude/ $< -o $@

rpibootd: src/rpibootd.o src/utils.o src/usbboot.o
	$(CC) -g -L/usr/local/lib -levent -lusb-1.0 $^ -o $@
