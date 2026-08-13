CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

TARGET = prog04

OBJS = prog04.o


.PHONY: all clean


all: $(TARGET)


$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)


prog04.o: prog04.c prog04.h
	$(CC) $(CFLAGS) -c prog04.c


clean:
	rm -f $(TARGET) $(OBJS)