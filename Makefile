TARGET = vt1211_nto
SRCS = vt1211_nto.c vt1211_gpio/src/vt1211_gpio.c libds/src/iterator.c libds/src/hashmap.c 
OBJS = $(SRCS:.c=.o)
CC = gcc
CFLAGS = -std=gnu99 -O2

.PHONY:     		all clean

all:			$(TARGET)

clean:
			rm -rf $(TARGET) $(OBJS)

$(TARGET):  $(OBJS)
			$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)
			usemsg -c $(TARGET) vt1211_nto_use.h
.c.o:
			$(CC) $(CFLAGS) -c $< -o $@
