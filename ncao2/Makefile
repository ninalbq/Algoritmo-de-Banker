CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -O2
TARGET = banker

all: $(TARGET)

$(TARGET): banker.c
	$(CC) $(CFLAGS) -o $(TARGET) banker.c

clean:
	rm -f $(TARGET) result.txt

.PHONY: all clean
