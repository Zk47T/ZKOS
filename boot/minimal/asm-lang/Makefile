all:
	aarch64-linux-gnu-as start.S -o start.o
	aarch64-linux-gnu-ld -T zkos.ld -nostdlib start.o -o zkos.elf
	aarch64-linux-gnu-objcopy -O binary zkos.elf zkos-minimal.bin

clean:
	rm -f start.o zkos.elf zkos-minimal.bin
