"""
KOSMOS v0.3 — Universal Physical Law Fracture Suite & C11 <-> RTL XOR == 0 Parity.
Verifies:
  1. SPEC/09: curl(grad(f)) == 0, div(curl(e)) == 0, checkerboard catch, Faraday-Lenz sign lock.
  2. SPEC/10: dt == 0 -> KILL, impulse spike -> TRAP, 1/2 kinetic energy non-truncation -> HOLD.
  3. SPEC/11: Pairwise 3rd-law catch (C_01 != C_10 -> TRAP, killing dead double-sum bug!) &
              strict discrete Nash equilibrium verified against direct potential shifts +/-1.
  4. SPEC/12: Minkowski lightcone, airgap work lock, entropy sink ceiling, Godel loop trap.
  5. Live C11 <-> SystemVerilog vvp streaming across 256 directed + random vectors (XOR == 0).
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
import os
import pathlib
import random
import subprocess
import tempfile
from kosmos_ctypes import (
    KOSMOS_GRID_CELLS,
    KOSMOS_GRID_N,
    KOSMOS_NASH_MAX_K,
    KOSMOS_OK,
    KOSMOS_VERDICT_HOLD,
    KOSMOS_VERDICT_KILL,
    KOSMOS_VERDICT_OPEN,
    KOSMOS_VERDICT_TRAP,
    KosmosInertiaCfg,
    KosmosNashSystem,
    KosmosPoint3,
    KosmosScalarGrid,
    KosmosU128,
    KosmosV03RtlVec,
    KosmosVecGrid,
    load_kosmos,
)

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
I32_MIN = -(1 << 31)
I32_MAX = (1 << 31) - 1


def test_cellular_exterior_calculus_and_lenz(lib: ctypes.CDLL) -> None:
    rng = random.Random(309)
    f = KosmosScalarGrid()
    for idx in range(KOSMOS_GRID_CELLS):
        f.v[idx] = rng.randint(-10**12, 10**12)

    grad_f = KosmosVecGrid()
    curl_grad = KosmosVecGrid()
    assert lib.kosmos_field_grad(ctypes.byref(f), ctypes.byref(grad_f)) == KOSMOS_OK
    assert lib.kosmos_field_curl(ctypes.byref(grad_f), ctypes.byref(curl_grad)) == KOSMOS_OK

    # Identity 1: curl(grad(f)) === 0 across every 2-cell plaquette!
    assert all(v == 0 for v in curl_grad.x)
    assert all(v == 0 for v in curl_grad.y)
    assert all(v == 0 for v in curl_grad.z)

    # Identity 2: div(curl(e)) === 0 across every 3-cell cube for arbitrary non-gradient edge field!
    edge = KosmosVecGrid()
    for idx in range(KOSMOS_GRID_CELLS):
        edge.x[idx] = rng.randint(-10**12, 10**12)
        edge.y[idx] = rng.randint(-10**12, 10**12)
        edge.z[idx] = rng.randint(-10**12, 10**12)
    curl_e = KosmosVecGrid()
    div_curl = KosmosScalarGrid()
    assert lib.kosmos_field_curl(ctypes.byref(edge), ctypes.byref(curl_e)) == KOSMOS_OK
    assert lib.kosmos_field_div(ctypes.byref(curl_e), ctypes.byref(div_curl)) == KOSMOS_OK
    assert any(v != 0 for v in curl_e.x), "Random edge field had trivial curl"
    assert all(v == 0 for v in div_curl.v)

    # Single edge vortex seam (+1 on one edge of grad_f) immediately detected by curl != 0
    grad_f.x[0] += 1
    assert lib.kosmos_field_curl(ctypes.byref(grad_f), ctypes.byref(curl_grad)) == KOSMOS_OK
    assert any(v != 0 for v in curl_grad.y) or any(v != 0 for v in curl_grad.z)

    # Checkerboard field (-1)^(x+y+z) * 1000: caught by both grad (+/-2000) and laplacian7 (+/-12000)
    chk = KosmosScalarGrid()
    for x in range(KOSMOS_GRID_N):
        for y in range(KOSMOS_GRID_N):
            for z in range(KOSMOS_GRID_N):
                c = (x * KOSMOS_GRID_N + y) * KOSMOS_GRID_N + z
                chk.v[c] = 1000 if ((x + y + z) % 2 == 0) else -1000
    chk_grad = KosmosVecGrid()
    chk_lap = KosmosScalarGrid()
    assert lib.kosmos_field_grad(ctypes.byref(chk), ctypes.byref(chk_grad)) == KOSMOS_OK
    assert lib.kosmos_field_laplacian7(ctypes.byref(chk), ctypes.byref(chk_lap)) == KOSMOS_OK
    assert all(abs(v) == 2000 for v in chk_grad.x)
    assert all(abs(v) == 12000 for v in chk_lap.v)
    # Conservation of Laplacian flux on closed 3-torus: sum(lap) == 0
    lap_f = KosmosScalarGrid()
    assert lib.kosmos_field_laplacian7(ctypes.byref(f), ctypes.byref(lap_f)) == KOSMOS_OK
    assert sum(int(v) for v in lap_f.v) == 0

    # Faraday-Lenz sign lock
    v = ctypes.c_uint8(0)
    assert lib.kosmos_eval_faraday_lenz(50, 10, 100, ctypes.byref(v)) == KOSMOS_OK and v.value == KOSMOS_VERDICT_KILL
    assert lib.kosmos_eval_faraday_lenz(50, -150, 100, ctypes.byref(v)) == KOSMOS_OK and v.value == KOSMOS_VERDICT_TRAP
    assert lib.kosmos_eval_faraday_lenz(50, -80, 100, ctypes.byref(v)) == KOSMOS_OK and v.value == KOSMOS_VERDICT_OPEN


def test_newtonian_inertia_and_half_unit_work(lib: ctypes.CDLL) -> None:
    cfg = KosmosInertiaCfg()
    for i in range(3):
        cfg.mass[i] = 1
        cfg.force_max[i] = 10
    cfg.energy_rem = KosmosU128(0, 0)
    cfg.w_min = KosmosU128(0, 0)
    cfg.entropy_toll = KosmosU128(0, 0)

    q0 = KosmosPoint3((ctypes.c_int64 * 3)(0, 0, 0))
    q1 = KosmosPoint3((ctypes.c_int64 * 3)(0, 0, 0))
    q2 = KosmosPoint3((ctypes.c_int64 * 3)(1, 0, 0))
    v = ctypes.c_uint8(0)

    # dt == 0 -> KILL
    assert lib.kosmos_eval_newtonian_step(ctypes.byref(cfg), ctypes.byref(q0), ctypes.byref(q1), ctypes.byref(q2), 0, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_KILL

    # Fracture test for 1/2 truncation: m=1, dq: 0->1 => dE_kin = 0.5 > E_rem = 0 -> MUST HOLD (not free OPEN!)
    assert lib.kosmos_eval_newtonian_step(ctypes.byref(cfg), ctypes.byref(q0), ctypes.byref(q1), ctypes.byref(q2), 1, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_HOLD

    # Supplying E_rem = 1 (>= 0.5) unlocks OPEN
    cfg.energy_rem = KosmosU128(1, 0)
    assert lib.kosmos_eval_newtonian_step(ctypes.byref(cfg), ctypes.byref(q0), ctypes.byref(q1), ctypes.byref(q2), 1, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_OPEN

    # Unphysical impulse spike (q2 = 20 -> m*|d2q| = 20 > F_max*dt = 10) -> TRAP
    q2_spike = KosmosPoint3((ctypes.c_int64 * 3)(20, 0, 0))
    cfg.energy_rem = KosmosU128(10000, 0)
    assert lib.kosmos_eval_newtonian_step(ctypes.byref(cfg), ctypes.byref(q0), ctypes.byref(q1), ctypes.byref(q2_spike), 1, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_TRAP


def test_nash_reciprocity_and_equilibrium(lib: ctypes.CDLL) -> None:
    sys_n = KosmosNashSystem()
    sys_n.k_channels = 2
    sys_n.has_pair_forces = 0
    sys_n.q[0] = 3
    sys_n.q[1] = -2

    # Fracture test: asymmetric C_01 = 999 != C_10 = -42 (which passed the dead double-sum!) -> MUST TRAP!
    sys_n.c_matrix[0 * KOSMOS_NASH_MAX_K + 0] = 100
    sys_n.c_matrix[0 * KOSMOS_NASH_MAX_K + 1] = 999
    sys_n.c_matrix[1 * KOSMOS_NASH_MAX_K + 0] = -42
    sys_n.c_matrix[1 * KOSMOS_NASH_MAX_K + 1] = 100
    v = ctypes.c_uint8(0)
    assert lib.kosmos_eval_nash_equilibrium(ctypes.byref(sys_n), ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_TRAP

    # Symmetric 3-channel coupled system at exact discrete Nash minimum
    sys_n = KosmosNashSystem()
    sys_n.k_channels = 3
    sys_n.has_pair_forces = 1
    q_vals = [10, -5, 4]
    c_rows = [
        [20, -4, 2],
        [-4, 16, -3],
        [2, -3, 14],
    ]
    for i in range(3):
        sys_n.q[i] = q_vals[i]
        for j in range(3):
            sys_n.c_matrix[i * KOSMOS_NASH_MAX_K + j] = c_rows[i][j]
            if i != j:
                # Pairwise opposite spring forces F_ij = -c_ij * (q_i - q_j) -> F_ij + F_ji == 0
                sys_n.f_pair[i * KOSMOS_NASH_MAX_K + j] = -c_rows[i][j] * (q_vals[i] - q_vals[j])

    # Set B_i close to sum_j C_ij q_j (within C_ii / 2 - 1) and verify potential U_i strictly rises for +/-1
    for i in range(3):
        exact_row = sum(c_rows[i][j] * q_vals[j] for j in range(3))
        sys_n.b_witness[i] = exact_row - 3  # 2 * |3| = 6 < C_ii (14, 16, 20)

        def u_pot(qi: int, idx: int = i) -> int:
            q_mod = list(q_vals)
            q_mod[idx] = qi
            off = sum(c_rows[idx][j] * q_mod[j] for j in range(3) if j != idx)
            return c_rows[idx][idx] * (qi**2) + 2 * qi * off - 2 * int(sys_n.b_witness[idx]) * qi

        assert u_pot(q_vals[i] + 1) > u_pot(q_vals[i]) and u_pot(q_vals[i] - 1) > u_pot(q_vals[i])

    assert lib.kosmos_eval_nash_equilibrium(ctypes.byref(sys_n), ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_OPEN

    # Perturb B_2 so 2 * |res_2| == 14 == C_22 -> unilateral step becomes weakly profitable -> HOLD!
    sys_n.b_witness[2] = sum(c_rows[2][j] * q_vals[j] for j in range(3)) - 7
    assert lib.kosmos_eval_nash_equilibrium(ctypes.byref(sys_n), ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_HOLD


def test_lightcone_airgap_entropy_godel(lib: ctypes.CDLL) -> None:
    v = ctypes.c_uint8(0)
    # Superluminal (dx=10, dt=3, c=3 -> 100 > 81) -> KILL
    assert lib.kosmos_eval_lightcone_airgap(10, 0, 0, 3, 3, 0, 1, 10, 0, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_KILL
    # Airgap fraud (airgap_sealed=0, receipt=1, w_lift=0) -> KILL
    assert lib.kosmos_eval_lightcone_airgap(1, 0, 0, 3, 3, 0, 1, 0, 0, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_KILL
    # Sealed airgap awaiting sync -> HOLD
    assert lib.kosmos_eval_lightcone_airgap(0, 0, 0, 3, 3, 1, 0, 0, 0, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_HOLD
    # Causal + witnessed + positive lift work -> OPEN
    assert lib.kosmos_eval_lightcone_airgap(3, 4, 0, 5, 1, 0, 1, 50, 0, ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_OPEN

    # Entropy sink ceiling
    assert lib.kosmos_eval_entropy_ceiling(KosmosU128(101, 0), KosmosU128(100, 0), ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_KILL
    assert lib.kosmos_eval_entropy_ceiling(KosmosU128(100, 0), KosmosU128(100, 0), ctypes.byref(v)) == KOSMOS_OK
    assert v.value == KOSMOS_VERDICT_OPEN

    # Godel boundary
    assert lib.kosmos_eval_godel_witness(101, 100, 0, 0, ctypes.byref(v)) == KOSMOS_OK and v.value == KOSMOS_VERDICT_TRAP
    assert lib.kosmos_eval_godel_witness(50, 100, 1, 0, ctypes.byref(v)) == KOSMOS_OK and v.value == KOSMOS_VERDICT_HOLD
    assert lib.kosmos_eval_godel_witness(101, 100, 1, 1, ctypes.byref(v)) == KOSMOS_OK and v.value == KOSMOS_VERDICT_OPEN


def make_clean_vec() -> KosmosV03RtlVec:
    vec = KosmosV03RtlVec()
    vec.rail_p = 0x1
    vec.rail_n = 0xE
    vec.airgap_sealed = 0
    vec.ancestry_valid = 1
    vec.unmerged_fork = 0
    vec.undecidable_hit = 0
    vec.has_ext_witness = 1
    vec.dt_ticks = 10
    vec.formal_iters = 5
    vec.max_iters = 100
    vec.dphi_b = 25
    vec.emf_induced = -25
    vec.emf_max = 100
    vec.force_in = -40
    vec.force_max = 100
    vec.f_ab = 75
    vec.f_ba = -75
    vec.nash_res_abs_2x = 8
    vec.nash_c_ii = 20
    vec.w_lift_work = 50
    vec.dr_sq = 400
    vec.ct_sq = 900
    vec.energy_rem_2x = 1000
    vec.w_min_2x = 200
    vec.energy_cost_2x = 300
    vec.s_dot_gen = 100
    vec.s_dot_sink = 200
    return vec


def test_c11_rtl_xor_zero_parity(lib: ctypes.CDLL) -> None:
    vectors: list[KosmosV03RtlVec] = []

    # 16 directed corner & fault vectors (including 32-bit wrap traps!)
    v0 = make_clean_vec()
    vectors.append(v0)

    # Dual-rail 11 fault
    v = make_clean_vec(); v.rail_n = 0xF; vectors.append(v)
    # INT32_MIN poison on force_in
    v = make_clean_vec(); v.force_in = I32_MIN; vectors.append(v)
    # dt_ticks == 0
    v = make_clean_vec(); v.dt_ticks = 0; vectors.append(v)
    # Lenz violation (same sign)
    v = make_clean_vec(); v.dphi_b = -5; v.emf_induced = -1; vectors.append(v)
    # Superluminal dr_sq > ct_sq
    v = make_clean_vec(); v.dr_sq = (1 << 60) + 1; v.ct_sq = (1 << 60); vectors.append(v)
    # Entropy overrun
    v = make_clean_vec(); v.s_dot_gen = (1 << 62) + 5; v.s_dot_sink = (1 << 62); vectors.append(v)
    # Airgap fraud (w_lift == 0)
    v = make_clean_vec(); v.airgap_sealed = 0; v.ancestry_valid = 1; v.w_lift_work = 0; vectors.append(v)
    # 33-bit f_ab + f_ba overflow trap (I32_MAX + I32_MAX != 0!)
    v = make_clean_vec(); v.f_ab = I32_MAX; v.f_ba = I32_MAX; vectors.append(v)
    # Over-EMF TRAP
    v = make_clean_vec(); v.emf_induced = -101; v.emf_max = 100; vectors.append(v)
    # Over-force TRAP
    v = make_clean_vec(); v.force_in = 101; v.force_max = 100; vectors.append(v)
    # Godel loop TRAP
    v = make_clean_vec(); v.formal_iters = 101; v.max_iters = 100; v.has_ext_witness = 0; vectors.append(v)
    # Unmerged fork HOLD
    v = make_clean_vec(); v.unmerged_fork = 1; vectors.append(v)
    # Right-to-rest HOLD
    v = make_clean_vec(); v.energy_rem_2x = 199; v.w_min_2x = 200; vectors.append(v)
    # Nash non-equilibrium HOLD (nash_res_abs_2x == nash_c_ii)
    v = make_clean_vec(); v.nash_res_abs_2x = 20; v.nash_c_ii = 20; vectors.append(v)
    # Undecidable seam HOLD
    v = make_clean_vec(); v.undecidable_hit = 1; v.has_ext_witness = 0; vectors.append(v)

    # + 240 randomized fuzz vectors across all 4 verdict basins
    rng = random.Random(20260300)
    for _ in range(240):
        rv = make_clean_vec()
        if rng.random() < 0.12:
            rv.rail_n = rng.randint(0, 15)
        if rng.random() < 0.10:
            rv.dt_ticks = 0
        if rng.random() < 0.15:
            rv.dphi_b = rng.choice([-50, 0, 50])
            rv.emf_induced = rng.randint(-150, 150)
        if rng.random() < 0.15:
            rv.force_in = rng.choice([I32_MIN, -120, -50, 0, 50, 120])
        if rng.random() < 0.15:
            rv.f_ba = -rv.f_ab + rng.choice([0, 1, -1])
        if rng.random() < 0.15:
            rv.dr_sq = rng.randint(0, 2000)
            rv.ct_sq = rng.randint(0, 2000)
        if rng.random() < 0.15:
            rv.nash_res_abs_2x = rng.randint(0, 30)
        if rng.random() < 0.15:
            rv.formal_iters = rng.randint(50, 150)
            rv.has_ext_witness = rng.choice([0, 1])
            rv.undecidable_hit = rng.choice([0, 1])
        if rng.random() < 0.15:
            rv.energy_rem_2x = rng.randint(100, 500)
        vectors.append(rv)

    vvp_bin = REPO_ROOT / "build" / "tb_kosmos_v03_physics_gate.vvp"
    assert vvp_bin.exists(), f"Missing {vvp_bin}; run 'make rtl' first."

    with tempfile.TemporaryDirectory() as tmpdir:
        vec_file = os.path.join(tmpdir, "vecs.txt")
        out_file = os.path.join(tmpdir, "rtl_out.txt")

        c_verdicts: list[int] = []
        with open(vec_file, "w", encoding="utf-8") as f:
            for item in vectors:
                c_verdicts.append(int(lib.kosmos_eval_v03_rtl_twin(ctypes.byref(item))))
                f.write(
                    f"{item.rail_p:x} {item.rail_n:x} "
                    f"{item.airgap_sealed} {item.ancestry_valid} {item.unmerged_fork} "
                    f"{item.undecidable_hit} {item.has_ext_witness} "
                    f"{item.dt_ticks} {item.formal_iters} {item.max_iters} "
                    f"{item.dphi_b} {item.emf_induced} {item.emf_max} "
                    f"{item.force_in} {item.force_max} {item.f_ab} {item.f_ba} "
                    f"{item.nash_res_abs_2x} {item.nash_c_ii} "
                    f"{item.w_lift_work:016x} {item.dr_sq:016x} {item.ct_sq:016x} "
                    f"{item.energy_rem_2x:016x} {item.w_min_2x:016x} {item.energy_cost_2x:016x} "
                    f"{item.s_dot_gen:016x} {item.s_dot_sink:016x}\n"
                )

        subprocess.run(
            ["vvp", str(vvp_bin), f"+VEC_FILE={vec_file}", f"+OUT_FILE={out_file}"],
            check=True,
            stdout=subprocess.DEVNULL,
        )

        rtl_lines = pathlib.Path(out_file).read_text(encoding="utf-8").strip().splitlines()
        assert len(rtl_lines) == len(vectors), f"Expected {len(vectors)} RTL outputs, got {len(rtl_lines)}"

        seen_classes: set[int] = set()
        for idx, (cv, rline) in enumerate(zip(c_verdicts, rtl_lines)):
            rv = int(rline.strip(), 16)
            xor_diff = cv ^ rv
            assert xor_diff == 0, f"C11 <-> RTL parity mismatch at vector #{idx}: C=0x{cv:02x}, RTL=0x{rv:02x}"
            seen_classes.add(cv)

        assert seen_classes == {
            KOSMOS_VERDICT_KILL,
            KOSMOS_VERDICT_TRAP,
            KOSMOS_VERDICT_HOLD,
            KOSMOS_VERDICT_OPEN,
        }, f"Not all 4 verdict classes covered: {seen_classes}"


def main() -> None:
    lib = load_kosmos()
    test_cellular_exterior_calculus_and_lenz(lib)
    test_newtonian_inertia_and_half_unit_work(lib)
    test_nash_reciprocity_and_equilibrium(lib)
    test_lightcone_airgap_entropy_godel(lib)
    test_c11_rtl_xor_zero_parity(lib)
    print(
        "[PASS] test_v03_physics_parity.py: 3D DEC (d^2==0), Lenz, Inertia, Pairwise Nash, Horizon/Godel, and 256-vector C11<->RTL XOR==0 verified."
    )


if __name__ == "__main__":
    main()
