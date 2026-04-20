CROSS_COMPILER = aarch64-linux-gnu-

all:
	$(CROSS_COMPILER)as start.S -o start.o
	$(CROSS_COMPILER)ld -T zkos.ld -nostdlib start.o -o zkos.elf
	$(CROSS_COMPILER)objcopy -O binary zkos.elf zkos-minimal.bin

clean:
	rm -f start.o zkos.elf zkos-minimal.bin
