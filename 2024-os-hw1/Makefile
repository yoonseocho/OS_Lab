CC = gcc
CFLAGS = -std=c99 -lreadline
TARGET = sish

all: $(TARGET)

$(TARGET): sish.c
	$(CC) $(CFLAGS) -o $(TARGET) sish.c

clean:
	rm -f $(TARGET)
