CC = gcc
CFLAGS = -Wall -Wextra -std=c99

TARGET = microshell
SRC = microshell.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
