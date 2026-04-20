all:
	aarch64-linux-gnu-gcc -c -ffreestanding -nostdlib -O2 main.c -o main.o
	aarch64-linux-gnu-as stub.S -o stub.o
	aarch64-linux-gnu-ld -T zkos.ld -nostdlib stub.o main.o -o zkos.elf
	aarch64-linux-gnu-objcopy -O binary zkos.elf zkos-c.bin

clean:
	rm -f *.o *.elf *.bin
