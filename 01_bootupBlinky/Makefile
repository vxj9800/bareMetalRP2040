# Source code files
MYBOOT2 = boot2Blinky
REALBOOT2 = boot2
PADCRC32 = padCrc32
LNKSCRIPT = link.ld

# Directory to create temporary build files in
BUILDDIR = build

# Compilation related variables
TOOLCHAIN = arm-none-eabi-
CFLAGS ?= -mcpu=cortex-m0plus -mthumb
LDFLAGS ?= -T link.ld -nostdlib

# Utilities path
UTILS = ../utils

build: makeDir $(BUILDDIR)/$(MYBOOT2).bin $(BUILDDIR)/$(REALBOOT2).c $(BUILDDIR)/$(REALBOOT2).bin $(BUILDDIR)/$(MYBOOT2).uf2 copyUF2

makeDir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/$(MYBOOT2).bin: $(MYBOOT2).c
	$(TOOLCHAIN)gcc $(CFLAGS) $(MYBOOT2).c -c -o $(BUILDDIR)/$(MYBOOT2).o
	$(TOOLCHAIN)objcopy -O binary $(BUILDDIR)/$(MYBOOT2).o $@

$(BUILDDIR)/$(REALBOOT2).c: $(BUILDDIR)/$(MYBOOT2).bin $(PADCRC32).cpp
	g++ -I $(UTILS) $(PADCRC32).cpp -o $(BUILDDIR)/$(PADCRC32).out
	./$(BUILDDIR)/$(PADCRC32).out $(BUILDDIR)/$(MYBOOT2).bin

$(BUILDDIR)/$(REALBOOT2).bin: $(BUILDDIR)/$(REALBOOT2).c
	$(TOOLCHAIN)gcc $(BUILDDIR)/$(REALBOOT2).c $(CFLAGS) $(LDFLAGS) -o $(BUILDDIR)/$(REALBOOT2).elf
	$(TOOLCHAIN)objcopy -O binary $(BUILDDIR)/$(REALBOOT2).elf $@

$(BUILDDIR)/$(MYBOOT2).uf2: $(BUILDDIR)/$(REALBOOT2).bin
	python3 $(UTILS)/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $(BUILDDIR)/$(REALBOOT2).bin -o $@

copyUF2: $(BUILDDIR)/$(MYBOOT2).uf2
	cp $(BUILDDIR)/$(MYBOOT2).uf2 ./$(MYBOOT2).uf2

clean:
	rm -rf $(BUILDDIR) $(MYBOOT2).uf2