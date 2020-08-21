CFLAGS=-std=c11 -g -fno-common -Wall -Wno-switch

chibidit:
	$(CC) -o $@ chibidit.c $(LDFLAGS)

clean:
	rm chibidit

.PHONY: test clean
