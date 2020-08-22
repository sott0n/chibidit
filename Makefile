CFLAGS=-std=c11 -g -fno-common -Wall -Wno-switch

chibidit: chibidit.c
	$(CC) -o $@ chibidit.c $(LDFLAGS)

clean:
	rm chibidit

.PHONY: test clean
