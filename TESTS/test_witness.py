"""
KOSMOS — Test 3: Belnap-Dunn 4-Valued Chu Bench & Loop-Space Seam Check.
Verifies UNKNOWN/REFUTED -> HOLD, VERIFIED -> OPEN, VERIFIED|REFUTED == CONFLICT -> KILL,
missing witness_id -> HOLD, and non-zero seam_defect -> KILL.
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
from kosmos_ctypes import (
    KOSMOS_CHU_CONFLICT,
    KOSMOS_CHU_REFUTED,
    KOSMOS_CHU_UNKNOWN,
    KOSMOS_CHU_VERIFIED,
    KOSMOS_L01_KEY_MAGIC,
    KOSMOS_OK,
    KOSMOS_STATE_HOLD,
    KOSMOS_STATE_IN_FLIGHT,
    KOSMOS_STATE_KILL,
    KOSMOS_VERDICT_HOLD,
    KOSMOS_VERDICT_KILL,
    KOSMOS_VERDICT_OPEN,
    KosmosCtrl,
    KosmosGate,
    KosmosLinfBox,
    KosmosMom3,
    KosmosPhase,
    KosmosPoint3,
    KosmosWitnessPre,
    load_kosmos,
)


def init_gate(lib: ctypes.CDLL) -> KosmosGate:
    box = KosmosLinfBox()
    box.count = 6
    for i in range(6):
        box.ref[i] = 0
        box.tol[i] = 1000
    gate = KosmosGate()
    assert lib.kosmos_gate_init(ctypes.byref(gate), ctypes.byref(box), ctypes.byref(box), 2, 100) == KOSMOS_OK
    return gate


def main() -> None:
    lib = load_kosmos()

    # 1. Direct Chu algebra checks
    rows = (ctypes.c_uint8 * 8)(KOSMOS_CHU_VERIFIED, KOSMOS_CHU_UNKNOWN, 0, 0, 0, 0, 0, 0)
    assert lib.kosmos_chu_evaluate(rows, 2, 2) == KOSMOS_CHU_UNKNOWN

    rows[1] = KOSMOS_CHU_REFUTED
    # Witness 0 says VERIFIED (0b10), Witness 1 says REFUTED (0b01) -> CONFLICT (0b11)!
    assert lib.kosmos_chu_evaluate(rows, 2, 1) == KOSMOS_CHU_CONFLICT

    rows[0] = KOSMOS_CHU_REFUTED
    rows[1] = KOSMOS_CHU_UNKNOWN
    assert lib.kosmos_chu_evaluate(rows, 2, 1) == KOSMOS_CHU_REFUTED

    rows[0] = KOSMOS_CHU_VERIFIED
    rows[1] = KOSMOS_CHU_VERIFIED
    assert lib.kosmos_chu_evaluate(rows, 2, 2) == KOSMOS_CHU_VERIFIED

    # 2. Gate witness enforcement
    gate = init_gate(lib)
    hard = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
    z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(1, 2, 3)), KosmosMom3((ctypes.c_int64 * 3)(4, 5, 6)))
    u = KosmosCtrl((ctypes.c_int64 * 3)(25, 0, 0))
    u_out = KosmosCtrl()
    verdict = ctypes.c_uint8(0)

    # Missing witness_id == 0 blocks OPEN even if Chu rows say VERIFIED
    w_pre = KosmosWitnessPre()
    w_pre.witness_id = 0
    w_pre.auth_nonce = 1
    w_pre.cycle = 101
    w_pre.witness_count = 2
    w_pre.chu_rows[0] = KOSMOS_CHU_VERIFIED
    w_pre.chu_rows[1] = KOSMOS_CHU_VERIFIED
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_HOLD and list(u_out.u) == [0, 0, 0]

    # Contradictory witnesses (1 VERIFIED + 1 REFUTED) -> KILL
    w_pre.witness_id = 77
    w_pre.auth_nonce = 2
    w_pre.cycle = 102
    w_pre.chu_rows[1] = KOSMOS_CHU_REFUTED
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_KILL and gate.state == KOSMOS_STATE_KILL

    # Recover via Level-01 key and test non-closing loop seam (seam_defect != 0 -> KILL)
    assert lib.kosmos_gate_l01_recover(ctypes.byref(gate), KOSMOS_L01_KEY_MAGIC, 103, ctypes.byref(verdict)) == KOSMOS_OK
    assert gate.state == KOSMOS_STATE_HOLD

    w_pre.auth_nonce = 3
    w_pre.cycle = 104
    w_pre.chu_rows[1] = KOSMOS_CHU_VERIFIED
    w_pre.seam_defect = -1  # Non-closing holonomy loop!
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_KILL and gate.state == KOSMOS_STATE_KILL

    # Recover and confirm clean 2-of-2 VERIFIED + seam_defect == 0 -> OPEN
    assert lib.kosmos_gate_l01_recover(ctypes.byref(gate), KOSMOS_L01_KEY_MAGIC, 105, ctypes.byref(verdict)) == KOSMOS_OK
    w_pre.auth_nonce = 4
    w_pre.cycle = 106
    w_pre.seam_defect = 0
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_OPEN and gate.state == KOSMOS_STATE_IN_FLIGHT
    assert list(u_out.u) == [25, 0, 0]

    print("[PASS] test_witness.py: Belnap-Dunn Chu bench, witness contradiction KILL, loop seam check.")


if __name__ == "__main__":
    main()
