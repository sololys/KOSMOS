"""
KOSMOS — Test 5: No Unauthorized Commit, Anti-Replay, and Power-Loss Lockout.
Verifies:
  - Elapsed cycles in HOLD never authorize OPEN
  - OPEN != REALIZED (requires tethered W_POST)
  - Concurrent OPEN during IN_FLIGHT is blocked
  - Replayed auth_nonce trips KILL
  - Power cut or untethered W_POST locks into UNRESOLVED until L01 recovery
  - L01 recovery appends to history without erasing prior fault entries
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
from kosmos_ctypes import (
    KOSMOS_CHU_VERIFIED,
    KOSMOS_ERR_AUTH,
    KOSMOS_ERR_LOCKED,
    KOSMOS_ERR_REPLAY,
    KOSMOS_ERR_TETHER,
    KOSMOS_L01_KEY_MAGIC,
    KOSMOS_OK,
    KOSMOS_STATE_HOLD,
    KOSMOS_STATE_IN_FLIGHT,
    KOSMOS_STATE_KILL,
    KOSMOS_STATE_UNRESOLVED,
    KOSMOS_VERDICT_HOLD,
    KOSMOS_VERDICT_KILL,
    KOSMOS_VERDICT_OPEN,
    KosmosCtrl,
    KosmosGate,
    KosmosLinfBox,
    KosmosMom3,
    KosmosPhase,
    KosmosPoint3,
    KosmosWitnessPost,
    KosmosWitnessPre,
    load_kosmos,
)


def main() -> None:
    lib = load_kosmos()

    box = KosmosLinfBox()
    box.count = 6
    for i in range(6):
        box.ref[i] = 0
        box.tol[i] = 5000

    gate = KosmosGate()
    assert lib.kosmos_gate_init(ctypes.byref(gate), ctypes.byref(box), ctypes.byref(box), 1, 1000) == KOSMOS_OK

    hard = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
    z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(10, 20, 30)), KosmosMom3((ctypes.c_int64 * 3)(-5, 5, -5)))
    u = KosmosCtrl((ctypes.c_int64 * 3)(100, -50, 25))
    u_out = KosmosCtrl()
    verdict = ctypes.c_uint8(0)

    # 1. Passing 500 clock cycles in HOLD never opens the gate or changes history
    for cyc in range(1001, 1501):
        assert (
            lib.kosmos_gate_step_hold(
                ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), cyc, ctypes.byref(u_out), ctypes.byref(verdict)
            )
            == KOSMOS_OK
        )
        assert verdict.value == KOSMOS_VERDICT_HOLD and list(u_out.u) == [0, 0, 0]
    assert gate.history.len == 1 and gate.realized_count == 0

    # 2. Stroke 1 (OPEN) enters IN_FLIGHT with realized_count == 0; blocks second OPEN
    w_pre = KosmosWitnessPre()
    w_pre.witness_id = 0xAA01
    w_pre.auth_nonce = 10
    w_pre.cycle = 1501
    w_pre.witness_count = 1
    w_pre.chu_rows[0] = KOSMOS_CHU_VERIFIED
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_OPEN and gate.realized_count == 0
    assert gate.state == KOSMOS_STATE_IN_FLIGHT

    w_pre2 = KosmosWitnessPre()
    w_pre2.witness_id = 0xAA02
    w_pre2.auth_nonce = 11
    w_pre2.cycle = 1502
    w_pre2.witness_count = 1
    w_pre2.chu_rows[0] = KOSMOS_CHU_VERIFIED
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre2), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_ERR_LOCKED
    )
    assert list(u_out.u) == [0, 0, 0]

    # 3. Power loss while IN_FLIGHT locks into UNRESOLVED and blocks future authorizations
    assert lib.kosmos_gate_power_loss(ctypes.byref(gate), 1600, ctypes.byref(verdict)) == KOSMOS_OK
    assert gate.state == KOSMOS_STATE_UNRESOLVED and gate.realized_count == 0

    w_pre2.cycle = 1601
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre2), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_ERR_LOCKED
    )
    assert list(u_out.u) == [0, 0, 0]

    # Wrong L01 key rejected; valid L01 key appends recovery event and restores HOLD
    assert lib.kosmos_gate_l01_recover(ctypes.byref(gate), 0xDEADBEEF, 1602, ctypes.byref(verdict)) == KOSMOS_ERR_AUTH
    len_before = gate.history.len
    assert lib.kosmos_gate_l01_recover(ctypes.byref(gate), KOSMOS_L01_KEY_MAGIC, 1602, ctypes.byref(verdict)) == KOSMOS_OK
    assert gate.state == KOSMOS_STATE_HOLD and gate.history.len == len_before + 1

    # 4. Untethered W_POST (forged pre_hash) locks into UNRESOLVED
    w_pre.auth_nonce = 12
    w_pre.cycle = 1603
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    bad_post = KosmosWitnessPost()
    bad_post.witness_id = 0xBB01
    bad_post.cycle = 1604
    bad_post.tether_pre_hash[0] = w_pre.pre_hash[0] ^ 0xFF
    bad_post.outcome_ok = 1
    assert lib.kosmos_gate_confirm_post(ctypes.byref(gate), ctypes.byref(bad_post), ctypes.byref(verdict)) == KOSMOS_ERR_TETHER
    assert gate.state == KOSMOS_STATE_UNRESOLVED and gate.realized_count == 0

    # Recover and test replay of auth_nonce == 12 -> KILL!
    assert lib.kosmos_gate_l01_recover(ctypes.byref(gate), KOSMOS_L01_KEY_MAGIC, 1605, ctypes.byref(verdict)) == KOSMOS_OK
    w_pre.auth_nonce = 12  # Reused!
    w_pre.cycle = 1606
    assert (
        lib.kosmos_gate_authorize(
            ctypes.byref(gate), hard, ctypes.byref(z), ctypes.byref(u), ctypes.byref(w_pre), ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_ERR_REPLAY
    )
    assert verdict.value == KOSMOS_VERDICT_KILL and gate.state == KOSMOS_STATE_KILL
    assert lib.kosmos_history_verify_chain(ctypes.byref(gate.history)) == KOSMOS_OK

    print("[PASS] test_no_unauthorized_commit.py: OPEN != REALIZED, power-loss lockout, anti-replay KILL.")


if __name__ == "__main__":
    main()
