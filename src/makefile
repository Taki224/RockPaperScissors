CC = gcc
CFLAGS = -g -Wall
LIBS = -lncurses
TARGET = Main

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LIBS)

clean:
	$(RM) $(TARGET)
