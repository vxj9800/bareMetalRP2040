# Source code files
BOOT2 = boot2Blinky
COMPCRC = compCrc32
CRCVALUE = crc
LNKSCRIPT = link.ld

# Directory to create temporary build files in
BUILDDIR = build

# Compilation related variables
TOOLCHAIN = arm-none-eabi-
CFLAGS ?= -mcpu=cortex-m0plus
LDFLAGS ?= -T link.ld -nostdlib

# Utilities path
UTILS = ../utils

build: makeDir $(BUILDDIR)/$(BOOT2).bin $(BUILDDIR)/$(BOOT2).uf2 copyUF2

makeDir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/$(BOOT2).bin: $(BOOT2).c
	$(TOOLCHAIN)gcc $(CFLAGS) $(BOOT2).c -c -o $(BUILDDIR)/$(BOOT2)_temp.o
	$(TOOLCHAIN)objdump -hSD $(BUILDDIR)/$(BOOT2)_temp.o > $(BUILDDIR)/$(BOOT2)_temp.objdump
	$(TOOLCHAIN)objcopy -O binary $(BUILDDIR)/$(BOOT2)_temp.o $(BUILDDIR)/$(BOOT2)_temp.bin
	g++ -I $(UTILS) $(COMPCRC).cpp -o $(BUILDDIR)/$(COMPCRC).out
	./$(BUILDDIR)/$(COMPCRC).out $(BUILDDIR)/$(BOOT2)_temp.bin
	$(TOOLCHAIN)gcc $(BOOT2).c $(BUILDDIR)/$(CRCVALUE).c $(CFLAGS) $(LDFLAGS) -o $(BUILDDIR)/$(BOOT2).elf
	$(TOOLCHAIN)objdump -hSD $(BUILDDIR)/$(BOOT2).elf > $(BUILDDIR)/$(BOOT2).objdump
	$(TOOLCHAIN)objcopy -O binary $(BUILDDIR)/$(BOOT2).elf $@

$(BUILDDIR)/$(BOOT2).uf2: $(BUILDDIR)/$(BOOT2).bin
	python3 $(UTILS)/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $(BUILDDIR)/$(BOOT2).bin -o $@

copyUF2: $(BUILDDIR)/$(BOOT2).uf2
	cp $(BUILDDIR)/$(BOOT2).uf2 ./$(BOOT2).uf2

clean:
	rm -rf $(BUILDDIR) $(BOOT2).uf2