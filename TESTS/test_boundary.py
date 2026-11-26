"""
KOSMOS — Test 2: Division-Free L-Infinity Safety & Candidate Boundaries.
Verifies single-spike KILL vs L2 masking, 15/10 non-truncation, extreme int64
offset-binary distance, and separation of candidate REJECT (HOLD) from hard KILL.
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
from kosmos_ctypes import (
    KOSMOS_CHU_VERIFIED,
    KOSMOS_ERR_DOMAIN,
    KOSMOS_OK,
    KOSMOS_PHASE_MAX,
    KOSMOS_POISON_MIN,
    KOSMOS_STATE_HOLD,
    KOSMOS_STATE_KILL,
    KOSMOS_VERDICT_HOLD,
    KOSMOS_VERDICT_KILL,
    KosmosCtrl,
    KosmosGate,
    KosmosLinfBox,
    KosmosMom3,
    KosmosPhase,
    KosmosPoint3,
    KosmosWitnessPre,
    load_kosmos,
)


def main() -> None:
    lib = load_kosmos()

    # 1. Invalid tolerance (tol <= 0) must fail closed
    bad_box = KosmosLinfBox()
    bad_box.count = 3
    bad_box.tol[0] = 100
    bad_box.tol[1] = 0
    bad_box.tol[2] = 100
    assert lib.kosmos_box_validate(ctypes.byref(bad_box)) == KOSMOS_ERR_DOMAIN

    # 2. Extreme opposite-sign int64 distance without signed overflow
    huge_box = KosmosLinfBox()
    huge_box.count = 2
    huge_box.ref[0] = -(1 << 62)
    huge_box.tol[0] = (1 << 63) - 10
    huge_box.ref[1] = 0
    huge_box.tol[1] = 10

    vals = (ctypes.c_int64 * 6)((1 << 62), 10, 0, 0, 0, 0)
    inside = ctypes.c_uint8(1)
    breach_idx = ctypes.c_uint32(99)
    assert (
        lib.kosmos_box_check(ctypes.byref(huge_box), vals, 6, ctypes.byref(inside), ctypes.byref(breach_idx))
        == KOSMOS_OK
    )
    # Distance between +2^62 and -2^62 is 2^63 > 2^63 - 10 -> must be detected outside without overflow!
    assert inside.value == 0 and breach_idx.value == 0

    # Poison value inside vector must return KOSMOS_ERR_DOMAIN and inside == 0
    vals[0] = -(1 << 62)
    vals[1] = KOSMOS_POISON_MIN
    assert (
        lib.kosmos_box_check(ctypes.byref(huge_box), vals, 6, ctypes.byref(inside), ctypes.byref(breach_idx))
        == KOSMOS_ERR_DOMAIN
    )
    assert inside.value == 0 and breach_idx.value == 1

    # 3. Single-spike L-inf hard breach triggers KILL even when 5 other channels are 0
    hard_box = KosmosLinfBox()
    cand_box = KosmosLinfBox()
    hard_box.count = 6
    cand_box.count = 6
    for i in range(6):
        hard_box.ref[i] = 0
        hard_box.tol[i] = 100
        cand_box.ref[i] = 0
        cand_box.tol[i] = 10

    gate = KosmosGate()
    assert lib.kosmos_gate_init(ctypes.byref(gate), ctypes.byref(hard_box), ctypes.byref(cand_box), 1, 10) == KOSMOS_OK

    # Candidate breach (15 vs tol 10: catches naive integer division 15 // 10 == 1) stays in HOLD
    hard_meas = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
    cand_z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(0, 0, 0)), KosmosMom3((ctypes.c_int64 * 3)(1, 2, 3)))
    cand_u = KosmosCtrl((ctypes.c_int64 * 3)(15, 0, 0))
    w_pre = KosmosWitnessPre()
    w_pre.witness_id = 1
    w_pre.auth_nonce = 1
    w_pre.cycle = 11
    w_pre.witness_count = 1
    w_pre.chu_rows[0] = KOSMOS_CHU_VERIFIED

    u_out = KosmosCtrl()
    verdict = ctypes.c_uint8(0)
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate),
            hard_meas,
            ctypes.byref(cand_z),
            ctypes.byref(cand_u),
            ctypes.byref(w_pre),
            ctypes.byref(u_out),
            ctypes.byref(verdict),
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_HOLD
    assert gate.state == KOSMOS_STATE_HOLD
    assert list(u_out.u) == [0, 0, 0]

    # Single hard channel at 101 > 100 (while L2 average across 6 channels would be ~41.2 < 100) -> KILL!
    hard_meas[5] = 101
    cand_u.u[0] = 5
    w_pre.auth_nonce = 2
    w_pre.cycle = 12
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate),
            hard_meas,
            ctypes.byref(cand_z),
            ctypes.byref(cand_u),
            ctypes.byref(w_pre),
            ctypes.byref(u_out),
            ctypes.byref(verdict),
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_KILL
    assert gate.state == KOSMOS_STATE_KILL
    assert list(u_out.u) == [0, 0, 0]

    # Sanity check KOSMOS_PHASE_MAX exact boundary hit
    max_box = KosmosLinfBox()
    max_box.count = 1
    max_box.ref[0] = 0
    max_box.tol[0] = KOSMOS_PHASE_MAX
    v_max = (ctypes.c_int64 * 6)(KOSMOS_PHASE_MAX, 0, 0, 0, 0, 0)
    assert lib.kosmos_box_check(ctypes.byref(max_box), v_max, 6, ctypes.byref(inside), None) == KOSMOS_OK
    assert inside.value == 1

    print("[PASS] test_boundary.py: L-infinity spike detection, 15/10 non-truncation, HOLD vs KILL split.")


if __name__ == "__main__":
    main()
