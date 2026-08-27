# Copyright 2021 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Author: Matheus Cavalcante, ETH Zurich
#         Samuel Riedel, ETH Zurich

SHELL = /usr/bin/env bash
ROOT_DIR := $(patsubst %/,%, $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
MEMPOOL_DIR := $(shell git rev-parse --show-toplevel 2>/dev/null || echo $$MEMPOOL_DIR)
SOFTWARE_DIR := $(abspath $(ROOT_DIR)/../..)
APPS_DIR := $(ROOT_DIR)
BIN_DIR := $(abspath $(SOFTWARE_DIR)/bin/$(subst $(SOFTWARE_DIR),,$(APPS_DIR)))
DATA_DIR := $(abspath $(SOFTWARE_DIR)/data)
RUNTIME_DIR := $(abspath $(SOFTWARE_DIR)/runtime)
# This will overwrite the ROOT_DIR variable from the included makefile
include $(RUNTIME_DIR)/runtime.mk

BIFURCATION_DEFINES ?= -DSB_NUM_ITER=$(BIFURCATION_SIM_ITER)

# Bifurcation graph data uses a standalone generator (no pyflexfloat dependency).
BIFURCATION_LATTICE      ?= g4096_lattice.txt
BIFURCATION_LATTICE_PATH := $(MEMPOOL_DIR)/matlab_scripts/bifurcation/$(BIFURCATION_LATTICE)

$(DATA_DIR)/data_bifurcation.h: $(DATA_DIR)/gen_bifurcation_data.py \
    $(BIFURCATION_LATTICE_PATH)
	python3 $(DATA_DIR)/gen_bifurcation_data.py \
		$(BIFURCATION_LATTICE_PATH) $(DATA_DIR)/data_bifurcation.h

data_bifurcation.h: $(DATA_DIR)/data_bifurcation.h
	cp $< $@

# Bifurcation uses a deeper call tree (barriers + printf) than the 512 B default stack.
BIFURCATION_STACK_SIZE  ?= 2048
BIFURCATION_SEQ_MEM_SIZE ?= 2048
BIFURCATION_SIM_ITER     ?= 10

.PHONY: bifurcation bifurcation_runtime_clean
bifurcation_runtime_clean:
	rm -vf $(RUNTIME_DIR)/crt0.S.o $(RUNTIME_DIR)/arch.ld \
		$(RUNTIME_DIR)/synchronization.c.o bifurcation/main.c.o bifurcation/sb.c.o

bifurcation: bifurcation_runtime_clean
	$(MAKE) stack_size=$(BIFURCATION_STACK_SIZE) \
		seq_mem_size=$(BIFURCATION_SEQ_MEM_SIZE) \
		BIFURCATION_SIM_ITER=$(BIFURCATION_SIM_ITER) \
		$(BIN_DIR)/bifurcation

$(BIN_DIR)/bifurcation: stack_size := $(BIFURCATION_STACK_SIZE)
$(BIN_DIR)/bifurcation: seq_mem_size := $(BIFURCATION_SEQ_MEM_SIZE)

bifurcation/main.c.o: bifurcation/main.c
	$(RISCV_CC) $(RISCV_CCFLAGS) $(BIFURCATION_DEFINES) -c $< -o $@

bifurcation/sb.c.o: bifurcation/sb.c bifurcation/sb.h data_bifurcation.h
	$(RISCV_CC) $(RISCV_CCFLAGS) $(BIFURCATION_DEFINES) -c $< -o $@

$(BIN_DIR)/bifurcation: bifurcation/main.c.o bifurcation/sb.c.o $(RUNTIME) \
		$(LINKER_SCRIPT) $(DATA_DIR)/data_bifurcation.h data_bifurcation.h update_opcodes
	mkdir -p $(dir $@)
	$(RISCV_CC) -Iinclude -o $@ bifurcation/main.c.o bifurcation/sb.c.o \
		$(RUNTIME) $(RISCV_LDFLAGS) -T$(RUNTIME_DIR)/link.ld
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) -D $@ > $@.dump

APPS := $(patsubst $(APPS_DIR)/%/main.c,%,$(shell find $(APPS_DIR) -name "main.c"))
BINARIES := $(filter-out $(BIN_DIR)/bifurcation,$(addprefix $(BIN_DIR)/,$(APPS)))
ALL := $(APPS)

FP_SUFFIXES := f16 f32 f8
I_SUFFIXES := q16 q32 i16 i32 i8
I_APPS := $(foreach suf, $(FP_SUFFIXES), $(filter %_$(suf), $(ALL)))
FP_APPS := $(foreach suf, $(I_SUFFIXES), $(filter %_$(suf), $(ALL)))
# Filter out applications
ALL_GCC := $(filter-out $(I_APPS), $(ALL))
ALL_LLVM := $(filter-out $(FP_APPS), $(ALL))

# Make all applications
all: $(ALL_GCC)

# Make only applications with full llvm support
all_llvm: $(ALL_LLVM)

$(APPS): % : $(BIN_DIR)/% $(APPS_DIR)/Makefile $(shell find $(RUNTIME_DIR)/**.{S,c,h,ld} -type f)

.PHONY: $(BINARIES)
$(BINARIES): $(BIN_DIR)/%: %/main.c.o $(RUNTIME) $(LINKER_SCRIPT) data_%.h update_opcodes
	mkdir -p $(dir $@)
	$(RISCV_CC) -Iinclude -o $@ $< $(RUNTIME) $(RISCV_LDFLAGS) -T$(RUNTIME_DIR)/link.ld
	$(RISCV_OBJDUMP) $(RISCV_OBJDUMP_FLAGS) -D $@ > $@.dump

# Helper targets
update_opcodes:
	make -C $(MEMPOOL_DIR) update_opcodes

.PHONY: clean
clean:
	rm -vf $(BINARIES)
	rm -vf $(addsuffix .dump,$(BINARIES))
	rm -vf $(addsuffix /main.c.o,$(APPS))
	rm -vf $(RUNTIME)
	rm -vf $(LINKER_SCRIPT)
	rm -vf $(wildcard $(DATA_DIR)/data_*.h)

.INTERMEDIATE: $(addsuffix /main.c.o,$(APPS))
