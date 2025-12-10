CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O3
LDFLAGS ?=

SOURCES = main.c wudparts.c rijndael.c sha1.c
TARGET = wud2app

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
