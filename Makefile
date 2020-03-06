CC=gcc

all: rpibootd

clean:
	rm -f src/*.o
	rm -f rpibootd

src/%.o: src/%.c
	$(CC) -c -I/usr/local/include -Iinclude/ $< -o $@

rpibootd: src/rpibootd.o src/usbboot.o
	$(CC) -L/usr/local/lib -lusb-1.0 $^ -o $@
