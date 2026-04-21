CROSS_COMPILE = aarch64-linux-gnu-
CFLAGS = -c -ffreestanding -nostdlib -O2 -Iinclude

OBJS = stub.o drivers/uart.o lib/string.o main.o

all: zkos.bin

zkos.bin: zkos.elf
	$(CROSS_COMPILE)objcopy -O binary $< $@

zkos.elf: $(OBJS)
	$(CROSS_COMPILE)ld -T zkos.ld -nostdlib $^ -o $@

%.o: %.S
	$(CROSS_COMPILE)as $< -o $@

%.o: %.c
	$(CROSS_COMPILE)gcc $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) zkos.elf zkos.bin
