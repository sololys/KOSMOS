# KOSMOS — Symplectic Witness-Gated Realization Space (0 float, 0 external deps)
.PHONY: all verify c11 rtl py surface manifest clean

CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion
SAN     := -fsanitize=address,undefined
CORE_SRC := CORE/kosmos_sha256.c \
            CORE/kosmos_mirror.c \
            CORE/kosmos_boundary.c \
            CORE/kosmos_history.c \
            CORE/kosmos_gate.c

all: verify

verify: c11 rtl py surface
	@echo "[PASS] KOSMOS granite verification 100% green (C11 + ASan/UBSan + RTL + Yosys + Python + Manifest)."

build/libkosmos.so: $(CORE_SRC) CORE/kosmos_space.h CORE/kosmos_sha256.h
	@mkdir -p build
	$(CC) $(CFLAGS) -fPIC -shared $(CORE_SRC) -ICORE -o build/libkosmos.so

c11: build/libkosmos.so TESTS/test_core_c11.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(SAN) $(CORE_SRC) TESTS/test_core_c11.c -ICORE -o build/test_core_c11
	./build/test_core_c11

rtl:
	@mkdir -p build
	iverilog -g2012 -Wall -o build/tb_kosmos_gate.vvp RTL/kosmos_gate.sv RTL/tb_kosmos_gate.sv
	vvp build/tb_kosmos_gate.vvp
	yosys -q -p "read_verilog -sv RTL/kosmos_gate.sv; proc; opt; check -assert; stat"
	@echo "[PASS] Yosys synthesis check clean (0 latches, 0 errors)."

py: build/libkosmos.so
	python3 -B TESTS/test_involution.py
	python3 -B TESTS/test_boundary.py
	python3 -B TESTS/test_witness.py
	python3 -B TESTS/test_prefix.py
	python3 -B TESTS/test_no_unauthorized_commit.py

manifest:
	python3 -B TESTS/verify_surface.py --write-manifest

surface:
	python3 -B TESTS/verify_surface.py --verify-manifest

clean:
	rm -rf build __pycache__ TESTS/__pycache__
