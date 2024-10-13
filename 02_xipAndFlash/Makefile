# Name of the project
PROJECT = flashBlinky

# Boot Stage 2 code files
BOOT2DIR = boot2
BOOT2 = bootStage2
COMPCRC = compCrc32
CRCVALUE = crc

# Linker Script
LNKSCRIPT = link.ld

# Directory to create temporary build files in
BUILDDIR = build
BUILDBOOT2DIR = $(BUILDDIR)/$(BOOT2DIR)

# Compilation related variables
TOOLCHAIN = arm-none-eabi-
GCC = $(TOOLCHAIN)gcc
GPP = $(TOOLCHAIN)g++
LNK = $(TOOLCHAIN)ld
DMP = $(TOOLCHAIN)objdump
CPY = $(TOOLCHAIN)objcopy
GCCFLAGS ?= -mcpu=cortex-m0plus -O3
LNKFLAGS ?= -T $(LNKSCRIPT) -nostdlib -O3

# Utilities path
UTILS = ../utils

build: makeDir $(BUILDBOOT2DIR)/$(BOOT2).o $(BUILDBOOT2DIR)/$(CRCVALUE).o $(BUILDDIR)/$(PROJECT).bin $(BUILDDIR)/$(PROJECT).uf2 copyUF2

makeDir:
	mkdir -p $(BUILDBOOT2DIR)

# Compile bootStage2
$(BUILDBOOT2DIR)/$(BOOT2).o: $(BOOT2DIR)/$(BOOT2).c
	cp $(BOOT2DIR)/$(BOOT2).c $(BUILDBOOT2DIR)/$(BOOT2).c
	$(GCC) $(BUILDBOOT2DIR)/$(BOOT2).c $(GCCFLAGS) -c -o $@
	$(DMP) -hSD $(BUILDBOOT2DIR)/$(BOOT2).o > $(BUILDBOOT2DIR)/$(BOOT2).objdump

# Compute CRC32 value and compile it
$(BUILDBOOT2DIR)/$(CRCVALUE).o: $(BUILDBOOT2DIR)/$(BOOT2).o $(BOOT2DIR)/$(COMPCRC).cpp $(UTILS)/CRCpp/inc/CRC.h
	$(CPY) -O binary $(BUILDBOOT2DIR)/$(BOOT2).o $(BUILDBOOT2DIR)/$(BOOT2).bin
	g++ -I $(UTILS) $(BOOT2DIR)/$(COMPCRC).cpp -o $(BUILDBOOT2DIR)/$(COMPCRC).out
	./$(BUILDBOOT2DIR)/$(COMPCRC).out $(BUILDBOOT2DIR)/$(BOOT2).bin
	$(GCC) $(BUILDBOOT2DIR)/$(CRCVALUE).c $(GCCFLAGS) -c -o $@

# Compile the project and link everything into a binary
$(BUILDDIR)/$(PROJECT).bin: $(BUILDBOOT2DIR)/$(BOOT2).o $(BUILDBOOT2DIR)/$(CRCVALUE).o $(LNKSCRIPT)
	$(GCC) *.c $(BUILDBOOT2DIR)/$(BOOT2).o $(BUILDBOOT2DIR)/$(CRCVALUE).o $(GCCFLAGS) $(LNKFLAGS) -o $(BUILDDIR)/$(PROJECT).elf
	$(DMP) -hSD $(BUILDDIR)/$(PROJECT).elf > $(BUILDDIR)/$(PROJECT).objdump
	$(CPY) -O binary $(BUILDDIR)/$(PROJECT).elf $@

# Convert bin to uf2 file
$(BUILDDIR)/$(PROJECT).uf2: $(BUILDDIR)/$(PROJECT).bin
	python3 $(UTILS)/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c $(BUILDDIR)/$(PROJECT).bin -o $@

copyUF2: $(BUILDDIR)/$(PROJECT).uf2
	cp $(BUILDDIR)/$(PROJECT).uf2 ./$(PROJECT).uf2

clean:
	rm -rf $(BUILDDIR) $(PROJECT).uf2