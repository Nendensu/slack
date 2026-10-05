CC:=gcc-14

CFLAGS := -Wall -Wextra -g -O0 -std=c23

.PHONY: all clean

all: virt

virt: virt.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf virt