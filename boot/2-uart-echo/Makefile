CROSS_COMPILE = aarch64-linux-gnu-

all:
	$(CROSS_COMPILE)gcc -c -ffreestanding -nostdlib -O2 main.c -o main.o
	$(CROSS_COMPILE)as stub.S -o stub.o
	$(CROSS_COMPILE)ld -T zkos.ld -nostdlib stub.o main.o -o zkos.elf
	$(CROSS_COMPILE)objcopy -O binary zkos.elf zkos.bin

clean:
	rm -f *.o *.elf *.bin
