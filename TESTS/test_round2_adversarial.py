"""
KOSMOS v0.2.1 — Permanent Round-2 Adversarial Regression Suite.
Verifies:
  1. Torn 128B KWAL tail preserves prefix last_nonce + unresolved_latched across
     cold boot AND across subsequent reboots via KTRN v2 quarantine seal;
     clearing KILL steps down to UNRESOLVED first, and replaying pre-tear nonce -> KILL.
  2. Extreme 64-bit corner vectors near M = 2^63 - 1 have zero 128-bit wrap-around
     against Python arbitrary-precision int.
  3. Sticky unresolved_latched in RAM gate survives hard KILL (KILL -> UNRESOLVED -> HOLD).
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
import os
import tempfile
from kosmos_ctypes import (
    KOSMOS_CHU_VERIFIED,
    KOSMOS_ERR_DOMAIN,
    KOSMOS_ERR_LOCKED,
    KOSMOS_ERR_OVERFLOW,
    KOSMOS_ERR_REPLAY,
    KOSMOS_ERR_TAMPER,
    KOSMOS_OK,
    KOSMOS_PHASE_MAX,
    KOSMOS_POISON_MIN,
    KOSMOS_STATE_HOLD,
    KOSMOS_STATE_KILL,
    KOSMOS_STATE_UNRESOLVED,
    KOSMOS_VERDICT_KILL,
    KOSMOS_VERDICT_OPEN,
    KosmosCornerGuard,
    KosmosCtrl,
    KosmosGate,
    KosmosLinfBox,
    KosmosMom3,
    KosmosPhase,
    KosmosPoint3,
    KosmosU128,
    KosmosWitnessPre,
    load_kosmos,
)

U64_MAX = (1 << 64) - 1
U128_MAX = (1 << 128) - 1


def make_box(tol: int) -> KosmosLinfBox:
    box = KosmosLinfBox()
    box.count = 6
    for i in range(6):
        box.ref[i] = 0
        box.tol[i] = tol
    return box


def test_torn_wal_preserves_nonce_and_unresolved(lib: ctypes.CDLL) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        wal_path = os.path.join(tmpdir, "round2_torn.wal")
        box = make_box(1000)

        # Boot 1: Authorize OPEN with auth_nonce = 42 (leaving PRE unresolved), then append 60 torn bytes
        g1 = KosmosGate()
        assert (
            lib.kosmos_gate_cold_boot(ctypes.byref(g1), wal_path.encode(), ctypes.byref(box), ctypes.byref(box), 1, 10, None, None)
            == KOSMOS_OK
        )
        hard = (ctypes.c_int64 * 6)(0, 0, 0, 0, 0, 0)
        z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(1, 2, 3)), KosmosMom3((ctypes.c_int64 * 3)(1, 2, 3)))
        u = KosmosCtrl((ctypes.c_int64 * 3)(50, 0, 0))
        w_pre = KosmosWitnessPre()
        w_pre.witness_id = 0xABCD1234
        w_pre.auth_nonce = 42
        w_pre.cycle = 11
        w_pre.witness_count = 1
        w_pre.chu_rows[0] = KOSMOS_CHU_VERIFIED
        u_out = KosmosCtrl()
        verdict = ctypes.c_uint8(0)

        assert (
            lib.kosmos_wal_authorize(
                ctypes.byref(g1),
                wal_path.encode(),
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

        torn_bytes = b"KWAL" + b"\xAA" * 56  # 60-byte half-written frame
        with open(wal_path, "ab") as f:
            f.write(torn_bytes)
        assert os.path.getsize(wal_path) == 316

        # Boot 2: Cold boot on torn WAL -> ERR_TAMPER + KILL, WITH last_nonce=42 & unresolved_latched=1 preserved!
        g2 = KosmosGate()
        had_unres = ctypes.c_uint8(0)
        assert (
            lib.kosmos_gate_cold_boot(
                ctypes.byref(g2), wal_path.encode(), ctypes.byref(box), ctypes.byref(box), 1, 20, ctypes.byref(had_unres), None
            )
            == KOSMOS_ERR_TAMPER
        )
        assert g2.state == KOSMOS_STATE_KILL
        assert g2.last_nonce == 42
        assert g2.unresolved_latched == 1 and had_unres.value == 1
        # Torn slot was forward-padded (68 B) + 128 B KTRN quarantine cert -> 512 B aligned!
        assert os.path.getsize(wal_path) == 512
        with open(wal_path, "rb") as f:
            raw_disk = f.read()
        # Original 60 torn bytes at offset 256..316 must remain completely untouched (zero truncation!)
        assert raw_disk[256:316] == torn_bytes

        # Boot 3 (reboot before L01 recovery): KTRN v2 quarantine replays deterministically into KILL with nonce=42!
        g3 = KosmosGate()
        assert (
            lib.kosmos_gate_cold_boot(
                ctypes.byref(g3), wal_path.encode(), ctypes.byref(box), ctypes.byref(box), 1, 30, ctypes.byref(had_unres), None
            )
            == KOSMOS_OK
        )
        assert g3.state == KOSMOS_STATE_KILL and g3.last_nonce == 42 and g3.unresolved_latched == 1

        # Signed L01 Recovery #1 steps down KILL -> STATE_UNRESOLVED (does NOT skip straight to HOLD!)
        key = (ctypes.c_uint8 * 32)(*([0x77] * 32))
        sig1 = (ctypes.c_uint8 * 32)()
        lib.kosmos_l01_sign_challenge(key, g3.history.head_hash, 31, g3.state, sig1)
        assert (
            lib.kosmos_wal_l01_recover_signed(ctypes.byref(g3), wal_path.encode(), key, sig1, 31, ctypes.byref(verdict))
            == KOSMOS_OK
        )
        assert g3.state == KOSMOS_STATE_UNRESOLVED

        # Authorize while still in UNRESOLVED is blocked!
        w_pre.auth_nonce = 43
        w_pre.cycle = 32
        assert (
            lib.kosmos_wal_authorize(
                ctypes.byref(g3),
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

        # Signed L01 Recovery #2 clears UNRESOLVED -> STATE_HOLD
        sig2 = (ctypes.c_uint8 * 32)()
        lib.kosmos_l01_sign_challenge(key, g3.history.head_hash, 33, g3.state, sig2)
        assert (
            lib.kosmos_wal_l01_recover_signed(ctypes.byref(g3), wal_path.encode(), key, sig2, 33, ctypes.byref(verdict))
            == KOSMOS_OK
        )
        assert g3.state == KOSMOS_STATE_HOLD

        # Now replay pre-tear auth_nonce = 42 -> MUST trip KILL (KOSMOS_ERR_REPLAY)!
        w_pre_replay = KosmosWitnessPre()
        w_pre_replay.witness_id = 0xABCD1234
        w_pre_replay.auth_nonce = 42
        w_pre_replay.cycle = 34
        w_pre_replay.witness_count = 1
        w_pre_replay.chu_rows[0] = KOSMOS_CHU_VERIFIED
        assert (
            lib.kosmos_wal_authorize(
                ctypes.byref(g3),
                wal_path.encode(),
                hard,
                ctypes.byref(z),
                ctypes.byref(u),
                ctypes.byref(w_pre_replay),
                ctypes.byref(u_out),
                ctypes.byref(verdict),
            )
            == KOSMOS_ERR_REPLAY
        )
        assert verdict.value == KOSMOS_VERDICT_KILL and g3.state == KOSMOS_STATE_KILL
        assert list(u_out.u) == [0, 0, 0]


def test_extreme_128bit_no_wraparound(lib: ctypes.CDLL) -> None:
    M = KOSMOS_PHASE_MAX
    cases = [
        ((0, 0, 0), (1, 1, 1), (M, M, M), U128_MAX),
        ((0, 0, 0), (1, 1, 1), (M, M, M), 3 * (M**2) - 1),
        ((-M, 0, 0), (1, 1, 1), (M, 0, 0), (2 * M) ** 2),
        ((-M, -M, 0), (1, 1, 1), (M, M, 0), U128_MAX),
        ((-M, 0, 0), (4, 1, 1), (M, 0, 0), U128_MAX),
        ((0, 0, 0), (1, 1, 1), (KOSMOS_POISON_MIN, 0, 0), U128_MAX),
    ]
    for ref, w, vec, emax in cases:
        g = KosmosCornerGuard()
        for i in range(3):
            g.ref[i] = ref[i]
            g.weight[i] = w[i]
        g.max_energy_sq = KosmosU128(emax & U64_MAX, (emax >> 64) & U64_MAX)
        c_vec = (ctypes.c_int64 * 3)(*vec)
        inside = ctypes.c_uint8(99)
        esq = KosmosU128(0, 0)
        rc = lib.kosmos_corner_guard_eval(ctypes.byref(g), c_vec, ctypes.byref(inside), ctypes.byref(esq))

        if any(v == KOSMOS_POISON_MIN for v in vec):
            assert rc == KOSMOS_ERR_DOMAIN and inside.value == 0
            continue

        py_tot = sum(w[i] * ((vec[i] - ref[i]) ** 2) for i in range(3))
        if py_tot > U128_MAX:
            assert rc == KOSMOS_ERR_OVERFLOW and inside.value == 0
        else:
            c_tot = int(esq.lo) | (int(esq.hi) << 64)
            assert rc == KOSMOS_OK and c_tot == py_tot and bool(inside.value) == (py_tot <= emax)


def main() -> None:
    lib = load_kosmos()
    test_torn_wal_preserves_nonce_and_unresolved(lib)
    test_extreme_128bit_no_wraparound(lib)
    print(
        "[PASS] test_round2_adversarial.py: torn-WAL KTRN quarantine, nonce survival, KILL->UNRESOLVED->HOLD stepdown, and 128-bit M-boundary verified."
    )


if __name__ == "__main__":
    main()
