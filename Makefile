# KOSMOS — Symplectic Witness-Gated Realization Space (0 float, 0 external deps)
.PHONY: all verify c11 rtl py surface manifest clean

CC       := gcc
CFLAGS   := -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion
SAN      := -fsanitize=address,undefined
CORE_SRC := CORE/kosmos_sha256.c \
            CORE/kosmos_mirror.c \
            CORE/kosmos_boundary.c \
            CORE/kosmos_history.c \
            CORE/kosmos_gate.c \
            CORE/kosmos_wal.c \
            CORE/kosmos_channel_sluice.c \
            CORE/kosmos_tdc_window.c \
            CORE/kosmos_energy_128.c \
            CORE/kosmos_cellular_field.c \
            CORE/kosmos_newtonian_inertia.c \
            CORE/kosmos_nash_reciprocity.c \
            CORE/kosmos_horizon_godel.c

all: verify

verify: c11 rtl py surface
	@echo "[PASS] KOSMOS v0.3 granite verification 100% green (C11 + ASan/UBSan + 3xRTL + Yosys + XOR==0 Parity + Manifest)."

build/libkosmos.so: $(CORE_SRC) CORE/kosmos_space.h CORE/kosmos_v02.h CORE/kosmos_v03.h CORE/kosmos_sha256.h
	@mkdir -p build
	$(CC) $(CFLAGS) -fPIC -shared $(CORE_SRC) -ICORE -o build/libkosmos.so

c11: build/libkosmos.so TESTS/test_core_c11.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(SAN) $(CORE_SRC) TESTS/test_core_c11.c -ICORE -o build/test_core_c11
	./build/test_core_c11

rtl:
	@mkdir -p build
	iverilog -g2012 -Wall -o build/tb_kosmos_gate.vvp RTL/kosmos_gate.sv RTL/tb_kosmos_gate.sv
	vvp -N build/tb_kosmos_gate.vvp
	iverilog -g2012 -Wall -o build/tb_kosmos_dual_rail_gate.vvp RTL/kosmos_dual_rail_gate.sv RTL/tb_kosmos_dual_rail_gate.sv
	vvp -N build/tb_kosmos_dual_rail_gate.vvp
	iverilog -g2012 -Wall -o build/tb_kosmos_v03_physics_gate.vvp RTL/kosmos_v03_physics_gate.sv RTL/tb_kosmos_v03_physics_gate.sv
	vvp -N build/tb_kosmos_v03_physics_gate.vvp
	yosys -q -p "read_verilog -sv RTL/kosmos_gate.sv RTL/kosmos_dual_rail_gate.sv RTL/kosmos_v03_physics_gate.sv; proc; opt; check -assert; stat"
	@echo "[PASS] Yosys synthesis check clean across all 3 RTL gates (0 latches, 0 errors)."

py: build/libkosmos.so rtl
	python3 -B TESTS/test_involution.py
	python3 -B TESTS/test_boundary.py
	python3 -B TESTS/test_witness.py
	python3 -B TESTS/test_prefix.py
	python3 -B TESTS/test_no_unauthorized_commit.py
	python3 -B TESTS/test_v02_extensions.py
	python3 -B TESTS/test_round2_adversarial.py
	python3 -B TESTS/test_v03_physics_parity.py

manifest:
	python3 -B TESTS/verify_surface.py --write-manifest

surface:
	python3 -B TESTS/verify_surface.py --verify-manifest

clean:
	rm -rf build __pycache__ TESTS/__pycache__
