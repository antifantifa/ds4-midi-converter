CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=199309L -I/usr/include/libevdev-1.0/
LIBS = -levdev -lasound
TARGET = gcmidi
SOURCES = gcmidi.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

run: $(TARGET)
	sudo ./$(TARGET)

clean:
	rm -f $(TARGET)

install:
	sudo cp $(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/$(TARGET)"

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstalled from /usr/local/bin/$(TARGET)"

.PHONY: run clean install uninstall
