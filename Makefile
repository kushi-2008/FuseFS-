CC = gcc

CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 $(shell pkg-config fuse3 --cflags)

LIBS = $(shell pkg-config fuse3 --libs)

TARGET = fusefs

SRC = src/fusefs.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
