"""
KOSMOS v0.2 — Falsification Suite for Cold-Boot WAL, Signed L01, 1024-Channel
TRAP Isolation, Alexandrov Picosecond ToF, and 128-Bit Corner & Landauer Guard.
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
from fractions import Fraction
import os
import subprocess
import sys
import tempfile
from kosmos_ctypes import (
    KOSMOS_CHANNEL_COUNT,
    KOSMOS_CHU_UNKNOWN,
    KOSMOS_CHU_VERIFIED,
    KOSMOS_ERR_AUTH,
    KOSMOS_ERR_DOMAIN,
    KOSMOS_ERR_LOCKED,
    KOSMOS_ERR_REPLAY,
    KOSMOS_ERR_TAMPER,
    KOSMOS_OK,
    KOSMOS_STATE_HOLD,
    KOSMOS_STATE_KILL,
    KOSMOS_STATE_UNRESOLVED,
    KOSMOS_VERDICT_HOLD,
    KOSMOS_VERDICT_KILL,
    KOSMOS_VERDICT_OPEN,
    KOSMOS_VERDICT_TRAP,
    KosmosChannelBank,
    KosmosCornerGuard,
    KosmosCtrl,
    KosmosGate,
    KosmosLandauerLedger,
    KosmosLedgerEntry,
    KosmosLinfBox,
    KosmosMom3,
    KosmosPhase,
    KosmosPoint3,
    KosmosTdcWindow,
    KosmosU128,
    KosmosWitnessPre,
    load_kosmos,
)


def make_box(tol: int) -> KosmosLinfBox:
    box = KosmosLinfBox()
    box.count = 6
    for i in range(6):
        box.ref[i] = 0
        box.tol[i] = tol
    return box


def run_child_pre_then_power_cut(wal_file: str) -> None:
    """Runs inside an isolated child process, authorizes PRE, and exits immediately."""
    lib = load_kosmos()
    box = make_box(1000)
    gate = KosmosGate()
    had_unres = ctypes.c_uint8(0)
    assert (
        lib.kosmos_gate_cold_boot(
            ctypes.byref(gate), wal_file.encode(), ctypes.byref(box), ctypes.byref(box), 1, 100, ctypes.byref(had_unres), None
        )
        == KOSMOS_OK
    )

    hard = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
    z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(5, 10, 15)), KosmosMom3((ctypes.c_int64 * 3)(1, -1, 2)))
    u = KosmosCtrl((ctypes.c_int64 * 3)(75, 0, 0))
    w_pre = KosmosWitnessPre()
    w_pre.witness_id = 0x778899AA
    w_pre.auth_nonce = 42
    w_pre.cycle = 101
    w_pre.witness_count = 1
    w_pre.chu_rows[0] = KOSMOS_CHU_VERIFIED
    u_out = KosmosCtrl()
    verdict = ctypes.c_uint8(0)

    assert (
        lib.kosmos_wal_authorize(
            ctypes.byref(gate),
            wal_file.encode(),
            hard,
            ctypes.byref(z),
            ctypes.byref(u),
            ctypes.byref(w_pre),
            ctypes.byref(u_out),
            ctypes.byref(verdict),
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_OPEN
    # Hard abrupt process exit simulating total power loss before W_POST!
    os._exit(0)


def test_cold_boot_across_process_death(lib: ctypes.CDLL) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        wal_path = os.path.join(tmpdir, "kosmos.wal")

        proc = subprocess.run(
            [sys.executable, "-B", __file__, "--child-power-cut", wal_path],
            check=False,
        )
        assert proc.returncode == 0, "Child power-cut process failed"

        # Cold-boot a completely fresh KosmosGate in parent process from disk WAL
        box = make_box(1000)
        gate = KosmosGate()
        had_unres = ctypes.c_uint8(0)
        recovered = KosmosLedgerEntry()
        assert (
            lib.kosmos_gate_cold_boot(
                ctypes.byref(gate),
                wal_path.encode(),
                ctypes.byref(box),
                ctypes.byref(box),
                1,
                200,
                ctypes.byref(had_unres),
                ctypes.byref(recovered),
            )
            == KOSMOS_OK
        )

        # 1. Output stays OFF, gate wakes in UNRESOLVED, interrupted PRE event is recovered
        assert had_unres.value == 1
        assert gate.state == KOSMOS_STATE_UNRESOLVED
        assert list(gate.active_u.u) == [0, 0, 0]
        assert recovered.witness_id == 0x778899AA
        assert gate.last_nonce == 42
        assert gate.history.len == 3  # BOOT_HOLD (0), OPEN_AUTHORIZED (1), OUTCOME_UNRESOLVED (2)

        # 2. Any new OPEN attempt is strictly blocked
        hard = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
        z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(1, 1, 1)), KosmosMom3((ctypes.c_int64 * 3)(1, 1, 1)))
        u = KosmosCtrl((ctypes.c_int64 * 3)(10, 0, 0))
        w_pre = KosmosWitnessPre()
        w_pre.witness_id = 0x1122
        w_pre.auth_nonce = 43
        w_pre.cycle = 201
        w_pre.witness_count = 1
        w_pre.chu_rows[0] = KOSMOS_CHU_VERIFIED
        u_out = KosmosCtrl()
        verdict = ctypes.c_uint8(0)

        assert (
            lib.kosmos_wal_authorize(
                ctypes.byref(gate),
                wal_path.encode(),
                hard,
                ctypes.byref(z),
                ctypes.byref(u),
                ctypes.byref(w_pre),
                ctypes.byref(u_out),
                ctypes.byref(verdict),
            )
            == KOSMOS_ERR_LOCKED
        )
        assert list(u_out.u) == [0, 0, 0]

        # 3. Signed L01 recovery extends history; replaying the signature later fails
        key = (ctypes.c_uint8 * 32)(*([0x33] * 32))
        sig = (ctypes.c_uint8 * 32)()
        lib.kosmos_l01_sign_challenge(key, gate.history.head_hash, 202, gate.state, sig)

        bad_sig = (ctypes.c_uint8 * 32)(*sig)
        bad_sig[0] ^= 0x01
        assert (
            lib.kosmos_wal_l01_recover_signed(ctypes.byref(gate), wal_path.encode(), key, bad_sig, 202, ctypes.byref(verdict))
            == KOSMOS_ERR_AUTH
        )

        len_before = gate.history.len
        assert (
            lib.kosmos_wal_l01_recover_signed(ctypes.byref(gate), wal_path.encode(), key, sig, 202, ctypes.byref(verdict))
            == KOSMOS_OK
        )
        assert gate.state == KOSMOS_STATE_HOLD and gate.history.len == len_before + 1

        # Replaying old nonce <= 42 after cold boot trips KILL (proving nonce persisted!)
        w_pre.auth_nonce = 42
        w_pre.cycle = 203
        assert (
            lib.kosmos_wal_authorize(
                ctypes.byref(gate),
                wal_path.encode(),
                hard,
                ctypes.byref(z),
                ctypes.byref(u),
                ctypes.byref(w_pre),
                ctypes.byref(u_out),
                ctypes.byref(verdict),
            )
            == KOSMOS_ERR_REPLAY
        )
        assert gate.state == KOSMOS_STATE_KILL

        # Replaying the exact same L01 signature 'sig' on this new KILL state MUST FAIL!
        assert (
            lib.kosmos_wal_l01_recover_signed(ctypes.byref(gate), wal_path.encode(), key, sig, 204, ctypes.byref(verdict))
            == KOSMOS_ERR_AUTH
        )

        # 4. Truncated/torn WAL file on disk boots directly into KILL
        with open(wal_path, "ab") as f:
            f.write(b"TORN")
        torn_gate = KosmosGate()
        assert (
            lib.kosmos_gate_cold_boot(
                ctypes.byref(torn_gate), wal_path.encode(), ctypes.byref(box), ctypes.byref(box), 1, 300, None, None
            )
            == KOSMOS_ERR_TAMPER
        )
        assert torn_gate.state == KOSMOS_STATE_KILL


def test_channel_isolation_and_degree_bound(lib: ctypes.CDLL) -> None:
    bank = KosmosChannelBank()
    assert lib.kosmos_channel_bank_init(ctypes.byref(bank), 1000, 100) == KOSMOS_OK

    # Degree > 4 or self-loop must fail closed
    too_many = (ctypes.c_uint16 * 5)(1, 2, 3, 4, 5)
    assert lib.kosmos_channel_set_neighbors(ctypes.byref(bank), 0, too_many, 5) == KOSMOS_ERR_DOMAIN
    self_loop = (ctypes.c_uint16 * 1)(42)
    assert lib.kosmos_channel_set_neighbors(ctypes.byref(bank), 42, self_loop, 1) == KOSMOS_ERR_DOMAIN

    # Wire a 4-regular ring across all 1024 channels
    for k in range(KOSMOS_CHANNEL_COUNT):
        nbrs = (ctypes.c_uint16 * 4)(
            (k - 2) % KOSMOS_CHANNEL_COUNT,
            (k - 1) % KOSMOS_CHANNEL_COUNT,
            (k + 1) % KOSMOS_CHANNEL_COUNT,
            (k + 2) % KOSMOS_CHANNEL_COUNT,
        )
        assert lib.kosmos_channel_set_neighbors(ctypes.byref(bank), k, nbrs, 4) == KOSMOS_OK

    u_out = ctypes.c_int64(0)
    verdict = ctypes.c_uint8(0)

    # Quarantine channel 42 into TRAP via local overload (cand_u = 150 > 100)
    assert (
        lib.kosmos_channel_step(
            ctypes.byref(bank), 42, 0, 150, 1, KOSMOS_CHU_VERIFIED, 0, ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_TRAP and u_out.value == 0

    # Verify all 4 direct neighbors (40, 41, 43, 44) and distant channel 500 remain completely unaffected!
    for ch in (40, 41, 43, 44, 500):
        assert (
            lib.kosmos_channel_step(
                ctypes.byref(bank), ch, 10, 80, 1, KOSMOS_CHU_VERIFIED, 0, ctypes.byref(u_out), ctypes.byref(verdict)
            )
            == KOSMOS_OK
        )
        assert verdict.value == KOSMOS_VERDICT_OPEN and u_out.value == 80

    # Channel 42 stays sticky in TRAP at 0 mV
    assert (
        lib.kosmos_channel_step(
            ctypes.byref(bank), 42, 0, 50, 2, KOSMOS_CHU_VERIFIED, 0, ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_ERR_LOCKED
    )
    assert verdict.value == KOSMOS_VERDICT_TRAP and u_out.value == 0

    # Global hard plant breach on channel 7 trips global_kill across all channels
    assert (
        lib.kosmos_channel_step(
            ctypes.byref(bank), 7, 1001, 10, 1, KOSMOS_CHU_VERIFIED, 0, ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_KILL and bank.global_kill == 1
    assert (
        lib.kosmos_channel_step(
            ctypes.byref(bank), 43, 0, 10, 2, KOSMOS_CHU_VERIFIED, 0, ctypes.byref(u_out), ctypes.byref(verdict)
        )
        == KOSMOS_ERR_LOCKED
    )
    assert verdict.value == KOSMOS_VERDICT_KILL and u_out.value == 0


def test_alexandrov_tof_and_echo_guard(lib: ctypes.CDLL) -> None:
    win = KosmosTdcWindow(133000, 133425, 0)
    inside = ctypes.c_uint8(0)
    assert lib.kosmos_tdc_check(ctypes.byref(win), 133425, ctypes.byref(inside)) == KOSMOS_OK and inside.value == 1
    assert lib.kosmos_tdc_check(ctypes.byref(win), 133426, ctypes.byref(inside)) == KOSMOS_OK and inside.value == 0

    # Calibrated uncertainty shrinks inner safe window [133050, 133375]
    win_cal = KosmosTdcWindow(133000, 133425, 50)
    assert lib.kosmos_tdc_check(ctypes.byref(win_cal), 133400, ctypes.byref(inside)) == KOSMOS_OK and inside.value == 0
    assert lib.kosmos_tdc_check(ctypes.byref(win_cal), 133375, ctypes.byref(inside)) == KOSMOS_OK and inside.value == 1

    # Echo inside window (133200 ps) WITHOUT Chu witness must still stay in HOLD at 0 mV
    box = make_box(1000)
    gate = KosmosGate()
    assert lib.kosmos_gate_init(ctypes.byref(gate), ctypes.byref(box), ctypes.byref(box), 1, 10) == KOSMOS_OK
    hard = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
    z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(1, 2, 3)), KosmosMom3((ctypes.c_int64 * 3)(1, 2, 3)))
    u = KosmosCtrl((ctypes.c_int64 * 3)(40, 0, 0))
    w_echo = KosmosWitnessPre()
    w_echo.witness_id = 99
    w_echo.auth_nonce = 1
    w_echo.cycle = 11
    w_echo.witness_count = 1
    w_echo.chu_rows[0] = KOSMOS_CHU_UNKNOWN
    u_out = KosmosCtrl()
    verdict = ctypes.c_uint8(0)

    assert (
        lib.kosmos_tdc_gated_authorize(
            ctypes.byref(gate),
            ctypes.byref(win_cal),
            133200,
            hard,
            ctypes.byref(z),
            ctypes.byref(u),
            ctypes.byref(w_echo),
            ctypes.byref(u_out),
            ctypes.byref(verdict),
        )
        == KOSMOS_OK
    )
    assert verdict.value == KOSMOS_VERDICT_HOLD and list(u_out.u) == [0, 0, 0]


def test_corner_128_and_logical_landauer(lib: ctypes.CDLL) -> None:
    guard = KosmosCornerGuard()
    for i in range(3):
        guard.ref[i] = 0
        guard.weight[i] = 1
    guard.max_energy_sq = KosmosU128(10000, 0)  # 100^2 = 10,000

    # Corner trap (80, 80, 10): |x_i| <= 100 on every axis, but 80^2 + 80^2 + 10^2 = 12,900 > 10,000
    corner = (ctypes.c_int64 * 3)(80, 80, 10)
    inside = ctypes.c_uint8(1)
    esq = KosmosU128()
    assert lib.kosmos_corner_guard_eval(ctypes.byref(guard), corner, ctypes.byref(inside), ctypes.byref(esq)) == KOSMOS_OK
    assert inside.value == 0 and esq.lo == 12900 and esq.hi == 0

    # High-dynamic-range > 64-bit check verified against Python Fraction
    guard_big = KosmosCornerGuard()
    weights = [3, 5, 7]
    refs = [-10**14, 2 * 10**14, -3 * 10**14]
    vec = (ctypes.c_int64 * 3)(9 * 10**14, -8 * 10**14, 7 * 10**14)
    for i in range(3):
        guard_big.ref[i] = refs[i]
        guard_big.weight[i] = weights[i]

    exact_frac = sum(Fraction(weights[i]) * (Fraction(int(vec[i]) - refs[i]) ** 2) for i in range(3))
    exact_int = int(exact_frac)
    guard_big.max_energy_sq = KosmosU128(exact_int & ((1 << 64) - 1), exact_int >> 64)

    assert lib.kosmos_corner_guard_eval(ctypes.byref(guard_big), vec, ctypes.byref(inside), ctypes.byref(esq)) == KOSMOS_OK
    c_u128 = int(esq.lo) | (int(esq.hi) << 64)
    assert c_u128 == exact_int and inside.value == 1

    # Logical Landauer lower-bound counter (7 discarded branches -> ceil(log2(8)) = 3 bits * 2871 = 8613 milli-zJ)
    led = KosmosLandauerLedger()
    assert lib.kosmos_landauer_init(ctypes.byref(led), 2871, KosmosU128(10000, 0)) == KOSMOS_OK
    ok_budget = ctypes.c_uint8(0)
    assert lib.kosmos_landauer_record_prune(ctypes.byref(led), 7, ctypes.byref(ok_budget)) == KOSMOS_OK
    assert led.erased_bits_min == 3 and led.logical_min_milli_zj.lo == 8613 and ok_budget.value == 1

    assert lib.kosmos_landauer_record_prune(ctypes.byref(led), 3, ctypes.byref(ok_budget)) == KOSMOS_OK
    # +2 bits -> 5 * 2871 = 14355 > 10000 budget!
    assert led.erased_bits_min == 5 and led.logical_min_milli_zj.lo == 14355 and ok_budget.value == 0


def main() -> None:
    if len(sys.argv) == 3 and sys.argv[1] == "--child-power-cut":
        run_child_pre_then_power_cut(sys.argv[2])
        return

    lib = load_kosmos()
    test_cold_boot_across_process_death(lib)
    test_channel_isolation_and_degree_bound(lib)
    test_alexandrov_tof_and_echo_guard(lib)
    test_corner_128_and_logical_landauer(lib)
    print(
        "[PASS] test_v02_extensions.py: cold-boot WAL across process death, signed L01, 1024-ch TRAP isolation, ToF echo guard, 128-bit (80,80,10) & Landauer."
    )


if __name__ == "__main__":
    main()
