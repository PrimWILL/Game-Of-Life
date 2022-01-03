CC=g++
CFLAGS=-g -Wall
OBJS=glife.o
TARGET=glife

$(TARGET) : glife.cpp
	$(CC) -o $@ glife.cpp -pthread

clean:
	rm -f *.o
	rm -f $(TARGET)