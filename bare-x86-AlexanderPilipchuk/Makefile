.DEFAULT_GOAL = kernel

CC = gcc
LD = ld
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector -Wall -Wextra
ASFLAGS = -m32
LDFLAGS = -m elf_i386 -T link.ld

start.o: start.S
	$(CC) $(ASFLAGS) -c start.S -o start.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel: kernel.o start.o
	$(LD) $(LDFLAGS) kernel.o start.o -o kernel

clean:
	rm -f *.o kernel

run: kernel
	qemu-system-x86_64 -kernel kernel
