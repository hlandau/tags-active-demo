CFLAGS=-Wall -Os -fno-stack-protector -ffreestanding -nostdlib -nostartfiles -mno-vsx -mbig
LDFLAGS=-nostdlib -melf64ppc

.PHONY: all run clean

all: kernel.elf

run: kernel.elf
	qemu-system-ppc64 -nographic -kernel "$<" -accel kvm -prom-env input-device=hvterm -machine pseries

clean:
	rm -f *.bin *.elf *.o

kernel.elf: kernel.ld start.o main.o
	ld $(LDFLAGS) -o "$@" -T $^

%.o: %.c
	gcc $(CFLAGS) -c -Os -o "$@" "$<"

%.o: %.S
	gcc $(CFLAGS) -c -o "$@" "$<"
