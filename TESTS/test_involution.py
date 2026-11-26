"""
KOSMOS — Test 1: Phase Mirror Involution, Antisymplectic Form, and Disconnection.
Verifies J(J(z)) == z, J* omega == -omega, INT64_MIN poison rejection, and
pi o gamma == const inside HOLD.
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
from kosmos_ctypes import (
    KOSMOS_ERR_DOMAIN,
    KOSMOS_OK,
    KOSMOS_PHASE_MAX,
    KOSMOS_POISON_MIN,
    KOSMOS_STATE_KILL,
    KosmosGate,
    KosmosI192,
    KosmosLinfBox,
    KosmosMom3,
    KosmosPhase,
    KosmosPoint3,
    KosmosVec3,
    load_kosmos,
)


def make_default_box(tol: int) -> KosmosLinfBox:
    box = KosmosLinfBox()
    box.count = 6
    for i in range(6):
        box.ref[i] = 0
        box.tol[i] = tol
    return box


def main() -> None:
    lib = load_kosmos()

    vectors = [
        ((0, 0, 0), (0, 0, 0)),
        ((17, -42, 999999), (-123456789, 987654321, -1)),
        ((-KOSMOS_PHASE_MAX, 0, KOSMOS_PHASE_MAX), (KOSMOS_PHASE_MAX, -KOSMOS_PHASE_MAX, 1)),
    ]

    for q_tup, p_tup in vectors:
        z = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(*q_tup)), KosmosMom3((ctypes.c_int64 * 3)(*p_tup)))
        z1 = KosmosPhase()
        z2 = KosmosPhase()
        assert lib.kosmos_mirror_apply(ctypes.byref(z), ctypes.byref(z1)) == KOSMOS_OK
        assert list(z1.q.q) == list(q_tup)
        assert list(z1.p.p) == [-x for x in p_tup]
        assert lib.kosmos_mirror_apply(ctypes.byref(z1), ctypes.byref(z2)) == KOSMOS_OK
        assert list(z2.q.q) == list(q_tup)
        assert list(z2.p.p) == list(p_tup)

    # Antisymplectic identity J* omega(dz1, dz2) == -omega(dz1, dz2) verified against Python arbitrary-precision int
    dq1 = KosmosVec3((ctypes.c_int64 * 3)(KOSMOS_PHASE_MAX, -76543210987654321, 42))
    dp1 = KosmosMom3((ctypes.c_int64 * 3)(-KOSMOS_PHASE_MAX, 11111111111111111, -99))
    dq2 = KosmosVec3((ctypes.c_int64 * 3)(22222222222222222, KOSMOS_PHASE_MAX, -KOSMOS_PHASE_MAX))
    dp2 = KosmosMom3((ctypes.c_int64 * 3)(33333333333333333, -44444444444444444, KOSMOS_PHASE_MAX))

    neg_dp1 = KosmosMom3((ctypes.c_int64 * 3)(*[-dp1.p[i] for i in range(3)]))
    neg_dp2 = KosmosMom3((ctypes.c_int64 * 3)(*[-dp2.p[i] for i in range(3)]))

    w_pos = KosmosI192()
    w_mir = KosmosI192()
    w_neg = KosmosI192()
    assert lib.kosmos_symplectic_form(
        ctypes.byref(dq1), ctypes.byref(dp1), ctypes.byref(dq2), ctypes.byref(dp2), ctypes.byref(w_pos)
    ) == KOSMOS_OK
    assert lib.kosmos_symplectic_form(
        ctypes.byref(dq1), ctypes.byref(neg_dp1), ctypes.byref(dq2), ctypes.byref(neg_dp2), ctypes.byref(w_mir)
    ) == KOSMOS_OK
    lib.kosmos_i192_neg(ctypes.byref(w_pos), ctypes.byref(w_neg))
    assert lib.kosmos_i192_eq(ctypes.byref(w_mir), ctypes.byref(w_neg)) == 1

    # Cross-check exact 192-bit two's-complement value against Python big-int
    py_omega = sum(int(dq1.dq[i]) * int(dp2.p[i]) - int(dp1.p[i]) * int(dq2.dq[i]) for i in range(3))
    raw_u192 = int(w_pos.limb[0]) | (int(w_pos.limb[1]) << 64) | (int(w_pos.limb[2]) << 128)
    c_omega = raw_u192 - (1 << 192) if (raw_u192 & (1 << 191)) else raw_u192
    assert c_omega == py_omega

    # Disconnection theorem in chamber: mirror flips preserve history h; poison trips KILL
    gate = KosmosGate()
    box = make_default_box(10000)
    assert lib.kosmos_gate_init(ctypes.byref(gate), ctypes.byref(box), ctypes.byref(box), 1, 1) == KOSMOS_OK
    head_before = bytes(gate.history.head_hash)
    z_live = KosmosPhase(KosmosPoint3((ctypes.c_int64 * 3)(5, 6, 7)), KosmosMom3((ctypes.c_int64 * 3)(8, -9, 10)))
    for _ in range(16):
        assert lib.kosmos_gate_mirror_chamber(ctypes.byref(gate), ctypes.byref(z_live)) == KOSMOS_OK
    assert gate.history.len == 1
    assert bytes(gate.history.head_hash) == head_before

    z_live.p.p[2] = KOSMOS_POISON_MIN
    assert lib.kosmos_gate_mirror_chamber(ctypes.byref(gate), ctypes.byref(z_live)) == KOSMOS_ERR_DOMAIN
    assert gate.state == KOSMOS_STATE_KILL

    print("[PASS] test_involution.py: J^2 = I, J*omega = -omega, INT64_MIN fence, pi o gamma = const.")


if __name__ == "__main__":
    main()
