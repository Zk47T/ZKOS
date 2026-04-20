CROSS_COMPILER = aarch64-linux-gnu-

all:
	$(CROSS_COMPILER)gcc -c -ffreestanding -nostdlib -O2 main.c -o main.o
	$(CROSS_COMPILER)as stub.S -o stub.o
	$(CROSS_COMPILER)ld -T zkos.ld -nostdlib stub.o main.o -o zkos.elf
	$(CROSS_COMPILER)objcopy -O binary zkos.elf zkos.bin

clean:
	rm -f *.o *.elf *.bin
