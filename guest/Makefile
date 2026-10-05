NASM:=nasm

all: guest.bin

guest.bin: guest.asm
	$(NASM) -f bin $< -o $@

clean:
	rm -rf guest.bin