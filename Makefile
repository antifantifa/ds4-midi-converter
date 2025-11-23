CFLAGS = -Wall -Wextra -O2 -std=gnu11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -I/usr/include/libevdev-1.0/
LIBS = -levdev -lasound

gcmidi: gcmidi.c
	$(CC) $(CFLAGS) -o gcmidi gcmidi.c $(LIBS)

clean:
	rm -f gcmidi
