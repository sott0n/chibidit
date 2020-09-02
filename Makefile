CFLAGS=-std=c11 -g -fno-common -Wall -Wno-switch
SRCROOT=./src
SRCDIRS:=$(shell find $(SRCROOT) -type d)
SRCS=$(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
OBJS=$(SRCS:.c=.o)

chibidit: ${OBJS}
	$(CC) -o $@ ${OBJS} $(LDFLAGS)

$(OBJS): $(SRCROOT)/chibidit.h

clean:
	rm chibidit $(SRCROOT)/*.o

.PHONY: test clean
