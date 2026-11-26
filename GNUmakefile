# Candidate entry points. The sealed v0.1 Makefile remains intact.
.PHONY: all verify baseline v02-c11 v02-rtl surface clean

CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion
SAN := -fsanitize=address,undefined
V02_CORE := CORE/kosmos_newton.c CORE/kosmos_channel_sluice.c CORE/kosmos_tdc_window.c
V02_HEADERS := $(V02_CORE:.c=.h)

all: verify

verify: baseline v02-c11 v02-rtl surface
	@echo "[PASS] KOSMOS v0.2 candidate verification. Physical evidence remains L3 HOLD."

baseline:
	@set -eu; tmp=$$(mktemp -d); trap 'rm -rf "$$tmp"' EXIT; \
	git archive c3df7a5 | tar -x -C "$$tmp"; \
	$(MAKE) -C "$$tmp" verify

build/libkosmos_v02.so: $(V02_CORE) $(V02_HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) -shared -fPIC $(V02_CORE) -o $@

v02-c11: build/libkosmos_v02.so TESTS/test_v02.c
	$(CC) $(CFLAGS) $(SAN) -ICORE $(V02_CORE) TESTS/test_v02.c -o build/test_v02
	./build/test_v02

v02-rtl:
	@mkdir -p build
	iverilog -g2012 -Wall -s tb_kosmos_dual_rail_gate -o build/tb_v02.vvp RTL/kosmos_dual_rail_gate.sv TESTS/tb_kosmos_dual_rail_gate.sv
	vvp build/tb_v02.vvp
	yosys -q -p 'read_verilog -sv RTL/kosmos_dual_rail_gate.sv; hierarchy -check -top kosmos_dual_rail_gate; proc; opt; select -assert-none t:$$dlatch; check -assert'

surface:
	python3 -B TESTS/verify_surface.py
	@git diff --quiet c3df7a5 -- MANIFEST.sha256

clean:
	$(MAKE) -f Makefile clean
