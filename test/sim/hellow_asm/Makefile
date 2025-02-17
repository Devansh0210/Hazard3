#=======================================================================
# Makefile for riscv-tests/isa
#-----------------------------------------------------------------------

XLEN ?= 32

src_dir := ../riscv-tests/riscv-tests/isa

#--------------------------------------------------------------------
# Build rules
#--------------------------------------------------------------------

RISCV_PREFIX ?= riscv$(XLEN)-unknown-elf-
RISCV_GCC ?= $(RISCV_PREFIX)gcc
RISCV_GCC_OPTS ?= -static -mcmodel=medany -fvisibility=hidden -nostdlib -nostartfiles
RISCV_OBJDUMP ?= $(RISCV_PREFIX)objdump --disassemble-all --disassemble-zeroes --section=.text --section=.text.startup --section=.text.init --section=.data
RISCV_SIM ?= spike

vpath %.S $(src_dir)

#------------------------------------------------------------
# Build assembly tests
.PHONY: main
default: main

%.dump: %
	$(RISCV_OBJDUMP) $< > $@

%.bin: %
	$(RISCV_PREFIX)objcopy -O binary $< $@

main: main_test.dump main_test.bin 
	rm main_test

main_test: main.S
	$(RISCV_GCC) -march=rv32imac_zicsr_zifencei -mabi=ilp32 $(RISCV_GCC_OPTS) -I$(src_dir)/../env_p_hazard3 -I$(src_dir)/macros/scalar -T$(src_dir)/../env_p_hazard3/link.ld $< -o $@


junk += main.o main_test.dump main_test.bin main_test main_test.vcd
#------------------------------------------------------------
# Clean up

clean:
	rm -rf $(junk)

run: main
	../tb_cxxrtl/tb --bin $<_test.bin --cycles 10000 --vcd $<_test.vcd --cpuret
