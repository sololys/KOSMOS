"""
KOSMOS — Test 4: Append-Only Prefix Law, Cantor Cylinder Narrowing, and SHA-256 Chain.
Verifies h_n <= h_{n+1}, C_{h||e} subset C_h, zero-float Cantor ultrametric ordering,
absence of history rewrite APIs, and cryptographic detection of single-bit tampering.
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
from kosmos_ctypes import (
    KOSMOS_ERR_CLOCK,
    KOSMOS_ERR_TAMPER,
    KOSMOS_OK,
    KOSMOS_VERDICT_HOLD,
    KOSMOS_VERDICT_OPEN,
    KosmosHistory,
    load_kosmos,
)


def main() -> None:
    lib = load_kosmos()

    # 1. Verify no rewrite/delete/pop symbols exist on libkosmos.so
    for forbidden in ("kosmos_history_pop", "kosmos_history_clear", "kosmos_history_truncate", "kosmos_history_rewrite"):
        assert not hasattr(lib, forbidden), f"Forbidden history-mutating symbol exposed: {forbidden}"

    # 2. Initialize history B = A* (genesis symbol 0 = BOOT_HOLD) and test Cantor cylinder narrowing
    hist = KosmosHistory()
    assert lib.kosmos_history_init(ctypes.byref(hist), 10) == KOSMOS_OK
    assert hist.len == 1 and hist.symbols[0] == 0

    seq_a = (ctypes.c_uint8 * 6)(0, 3, 4, 3, 4, 7)
    seq_b = (ctypes.c_uint8 * 6)(0, 3, 5, 7, 3, 4)
    seq_c = (ctypes.c_uint8 * 6)(1, 0, 3, 4, 0, 0)

    # At h = (0), both seq_a and seq_b lie inside C_h, while seq_c does not
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_a, 6) == 1
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_b, 6) == 1
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_c, 6) == 0

    payload = (ctypes.c_uint8 * 32)(*([0xAA] * 32))
    # Append symbol 3 (OPEN_AUTHORIZED) -> h = (0, 3): still contains both seq_a and seq_b
    assert lib.kosmos_history_append(ctypes.byref(hist), 11, 3, KOSMOS_VERDICT_OPEN, 1, payload) == KOSMOS_OK
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_a, 6) == 1
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_b, 6) == 1

    # Append symbol 4 (POST_REALIZED) -> h = (0, 3, 4): C_{0,3,4} strictly narrows, excluding seq_b!
    assert lib.kosmos_history_append(ctypes.byref(hist), 12, 4, KOSMOS_VERDICT_HOLD, 2, payload) == KOSMOS_OK
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_a, 6) == 1
    assert lib.kosmos_cantor_in_cylinder(ctypes.byref(hist), seq_b, 6) == 0

    # 3. Zero-float Cantor ultrametric comparison: l(seq_a, seq_b) == 2 > l(seq_a, seq_c) == 0
    assert lib.kosmos_cantor_common_prefix(seq_a, 6, seq_b, 6) == 2
    assert lib.kosmos_cantor_common_prefix(seq_a, 6, seq_c, 6) == 0
    assert lib.kosmos_cantor_closer(seq_a, 6, seq_b, 6, seq_c, 6) == 1
    assert lib.kosmos_cantor_closer(seq_a, 6, seq_c, 6, seq_b, 6) == 0

    # 4. Non-monotonic clock regression is rejected
    assert lib.kosmos_history_append(ctypes.byref(hist), 11, 4, KOSMOS_VERDICT_HOLD, 3, payload) == KOSMOS_ERR_CLOCK

    # 5. Cryptographic SHA-256 chain verification & single-bit tamper detection
    assert lib.kosmos_history_verify_chain(ctypes.byref(hist)) == KOSMOS_OK
    hist.entries[1].witness_id ^= 1  # Flip 1 bit in historical entry #1
    assert lib.kosmos_history_verify_chain(ctypes.byref(hist)) == KOSMOS_ERR_TAMPER

    print("[PASS] test_prefix.py: h_{n+1} = h_n || e_n, Cantor cylinder narrowing, SHA-256 tamper catch.")


if __name__ == "__main__":
    main()
