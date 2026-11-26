"""
KOSMOS — Shared ctypes binding to build/libkosmos.so (0 float).
SPDX-License-Identifier: Apache-2.0
"""

import ctypes
import pathlib
import sys

KOSMOS_DIM = 3
KOSMOS_CTRL_DIM = 3
KOSMOS_GUARD_DIM = 6
KOSMOS_MAX_WITNESSES = 8
KOSMOS_ALPHABET_SIZE = 8
KOSMOS_HISTORY_CAP = 256

KOSMOS_PHASE_MAX = (1 << 63) - 1
KOSMOS_POISON_MIN = -(1 << 63)

KOSMOS_VERDICT_KILL = 0x00
KOSMOS_VERDICT_HOLD = 0x3C
KOSMOS_VERDICT_TRAP = 0x5A
KOSMOS_VERDICT_OPEN = 0xA5

KOSMOS_CHANNEL_COUNT = 1024
KOSMOS_MAX_DEGREE = 4
KOSMOS_GRID_N = 4
KOSMOS_GRID_CELLS = 64
KOSMOS_NASH_MAX_K = 8

KOSMOS_L01_KEY_MAGIC = 0x4B4F534D4F533031

KOSMOS_OK = 0
KOSMOS_ERR_NULL = -1
KOSMOS_ERR_DOMAIN = -2
KOSMOS_ERR_OVERFLOW = -3
KOSMOS_ERR_LOCKED = -4
KOSMOS_ERR_REPLAY = -5
KOSMOS_ERR_CLOCK = -6
KOSMOS_ERR_TETHER = -7
KOSMOS_ERR_CAPACITY = -8
KOSMOS_ERR_TAMPER = -9
KOSMOS_ERR_AUTH = -10

KOSMOS_CHU_UNKNOWN = 0
KOSMOS_CHU_REFUTED = 1
KOSMOS_CHU_VERIFIED = 2
KOSMOS_CHU_CONFLICT = 3

KOSMOS_STATE_HOLD = 0
KOSMOS_STATE_IN_FLIGHT = 1
KOSMOS_STATE_UNRESOLVED = 2
KOSMOS_STATE_KILL = 3


class KosmosPoint3(ctypes.Structure):
    _fields_ = [("q", ctypes.c_int64 * KOSMOS_DIM)]


class KosmosVec3(ctypes.Structure):
    _fields_ = [("dq", ctypes.c_int64 * KOSMOS_DIM)]


class KosmosMom3(ctypes.Structure):
    _fields_ = [("p", ctypes.c_int64 * KOSMOS_DIM)]


class KosmosPhase(ctypes.Structure):
    _fields_ = [("q", KosmosPoint3), ("p", KosmosMom3)]


class KosmosCtrl(ctypes.Structure):
    _fields_ = [("u", ctypes.c_int64 * KOSMOS_CTRL_DIM)]


class KosmosI192(ctypes.Structure):
    _fields_ = [("limb", ctypes.c_uint64 * 3)]


class KosmosLinfBox(ctypes.Structure):
    _fields_ = [
        ("count", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
        ("ref", ctypes.c_int64 * KOSMOS_GUARD_DIM),
        ("tol", ctypes.c_int64 * KOSMOS_GUARD_DIM),
    ]


class KosmosWitnessPre(ctypes.Structure):
    _fields_ = [
        ("witness_id", ctypes.c_uint64),
        ("auth_nonce", ctypes.c_uint64),
        ("cycle", ctypes.c_uint64),
        ("seam_defect", ctypes.c_int64),
        ("witness_count", ctypes.c_uint32),
        ("chu_rows", ctypes.c_uint8 * KOSMOS_MAX_WITNESSES),
        ("pre_hash", ctypes.c_uint8 * 32),
        ("pad", ctypes.c_uint8 * 4),
    ]


class KosmosWitnessPost(ctypes.Structure):
    _fields_ = [
        ("witness_id", ctypes.c_uint64),
        ("cycle", ctypes.c_uint64),
        ("tether_pre_hash", ctypes.c_uint8 * 32),
        ("outcome_ok", ctypes.c_uint8),
        ("pad", ctypes.c_uint8 * 7),
    ]


class KosmosLedgerEntry(ctypes.Structure):
    _fields_ = [
        ("index", ctypes.c_uint64),
        ("cycle", ctypes.c_uint64),
        ("witness_id", ctypes.c_uint64),
        ("symbol", ctypes.c_uint8),
        ("verdict", ctypes.c_uint8),
        ("pad", ctypes.c_uint8 * 6),
        ("prev_hash", ctypes.c_uint8 * 32),
        ("payload_hash", ctypes.c_uint8 * 32),
        ("entry_hash", ctypes.c_uint8 * 32),
    ]


class KosmosHistory(ctypes.Structure):
    _fields_ = [
        ("len", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
        ("symbols", ctypes.c_uint8 * KOSMOS_HISTORY_CAP),
        ("head_hash", ctypes.c_uint8 * 32),
        ("entries", KosmosLedgerEntry * KOSMOS_HISTORY_CAP),
    ]


class KosmosGate(ctypes.Structure):
    _fields_ = [
        ("state", ctypes.c_uint32),
        ("min_witnesses", ctypes.c_uint32),
        ("unresolved_latched", ctypes.c_uint32),
        ("reserved_gate", ctypes.c_uint32),
        ("last_cycle", ctypes.c_uint64),
        ("last_nonce", ctypes.c_uint64),
        ("open_cycle", ctypes.c_uint64),
        ("realized_count", ctypes.c_uint64),
        ("pending_pre_hash", ctypes.c_uint8 * 32),
        ("hard_box", KosmosLinfBox),
        ("cand_box", KosmosLinfBox),
        ("active_u", KosmosCtrl),
        ("history", KosmosHistory),
    ]


class KosmosU128(ctypes.Structure):
    _fields_ = [("lo", ctypes.c_uint64), ("hi", ctypes.c_uint64)]


class KosmosChannelSlot(ctypes.Structure):
    _fields_ = [
        ("verdict", ctypes.c_uint8),
        ("degree", ctypes.c_uint8),
        ("neighbors", ctypes.c_uint16 * KOSMOS_MAX_DEGREE),
        ("last_nonce", ctypes.c_uint64),
        ("out_u", ctypes.c_int64),
        ("trap_count", ctypes.c_uint32),
        ("open_count", ctypes.c_uint32),
    ]


class KosmosChannelBank(ctypes.Structure):
    _fields_ = [
        ("global_kill", ctypes.c_uint8),
        ("pad", ctypes.c_uint8 * 7),
        ("hard_limit", ctypes.c_int64),
        ("cand_limit", ctypes.c_int64),
        ("slots", KosmosChannelSlot * KOSMOS_CHANNEL_COUNT),
    ]


class KosmosTdcWindow(ctypes.Structure):
    _fields_ = [
        ("tau_min_ps", ctypes.c_uint64),
        ("tau_max_ps", ctypes.c_uint64),
        ("cal_uncert_ps", ctypes.c_uint64),
    ]


class KosmosCornerGuard(ctypes.Structure):
    _fields_ = [
        ("ref", ctypes.c_int64 * KOSMOS_DIM),
        ("weight", ctypes.c_uint64 * KOSMOS_DIM),
        ("max_energy_sq", KosmosU128),
    ]


class KosmosLandauerLedger(ctypes.Structure):
    _fields_ = [
        ("pruned_branches", ctypes.c_uint64),
        ("erased_bits_min", ctypes.c_uint64),
        ("ebit_zepto_joule_milli", ctypes.c_uint64),
        ("logical_min_milli_zj", KosmosU128),
        ("budget_milli_zj", KosmosU128),
    ]


class KosmosScalarGrid(ctypes.Structure):
    _fields_ = [("v", ctypes.c_int64 * KOSMOS_GRID_CELLS)]


class KosmosVecGrid(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_int64 * KOSMOS_GRID_CELLS),
        ("y", ctypes.c_int64 * KOSMOS_GRID_CELLS),
        ("z", ctypes.c_int64 * KOSMOS_GRID_CELLS),
    ]


class KosmosInertiaCfg(ctypes.Structure):
    _fields_ = [
        ("mass", ctypes.c_int64 * KOSMOS_DIM),
        ("force_max", ctypes.c_int64 * KOSMOS_DIM),
        ("energy_rem", KosmosU128),
        ("w_min", KosmosU128),
        ("entropy_toll", KosmosU128),
    ]


class KosmosNashSystem(ctypes.Structure):
    _fields_ = [
        ("k_channels", ctypes.c_uint32),
        ("has_pair_forces", ctypes.c_uint32),
        ("q", ctypes.c_int64 * KOSMOS_NASH_MAX_K),
        ("b_witness", ctypes.c_int64 * KOSMOS_NASH_MAX_K),
        ("c_matrix", ctypes.c_int64 * (KOSMOS_NASH_MAX_K * KOSMOS_NASH_MAX_K)),
        ("f_pair", ctypes.c_int64 * (KOSMOS_NASH_MAX_K * KOSMOS_NASH_MAX_K)),
    ]


class KosmosV03RtlVec(ctypes.Structure):
    _fields_ = [
        ("rail_p", ctypes.c_uint8),
        ("rail_n", ctypes.c_uint8),
        ("airgap_sealed", ctypes.c_uint8),
        ("ancestry_valid", ctypes.c_uint8),
        ("unmerged_fork", ctypes.c_uint8),
        ("undecidable_hit", ctypes.c_uint8),
        ("has_ext_witness", ctypes.c_uint8),
        ("pad0", ctypes.c_uint8),
        ("dt_ticks", ctypes.c_uint32),
        ("formal_iters", ctypes.c_uint32),
        ("max_iters", ctypes.c_uint32),
        ("pad1", ctypes.c_uint32),
        ("dphi_b", ctypes.c_int32),
        ("emf_induced", ctypes.c_int32),
        ("emf_max", ctypes.c_int32),
        ("force_in", ctypes.c_int32),
        ("force_max", ctypes.c_int32),
        ("f_ab", ctypes.c_int32),
        ("f_ba", ctypes.c_int32),
        ("nash_res_abs_2x", ctypes.c_int32),
        ("nash_c_ii", ctypes.c_int32),
        ("pad2", ctypes.c_int32),
        ("w_lift_work", ctypes.c_uint64),
        ("dr_sq", ctypes.c_uint64),
        ("ct_sq", ctypes.c_uint64),
        ("energy_rem_2x", ctypes.c_uint64),
        ("w_min_2x", ctypes.c_uint64),
        ("energy_cost_2x", ctypes.c_uint64),
        ("s_dot_gen", ctypes.c_uint64),
        ("s_dot_sink", ctypes.c_uint64),
    ]


def load_kosmos() -> ctypes.CDLL:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    so_path = repo_root / "build" / "libkosmos.so"
    if not so_path.exists():
        print(f"Missing {so_path}; run 'make c11' first.", file=sys.stderr)
        sys.exit(1)

    lib = ctypes.CDLL(str(so_path))

    lib.kosmos_sizeof_gate.argtypes = []
    lib.kosmos_sizeof_gate.restype = ctypes.c_size_t
    lib.kosmos_sizeof_history.argtypes = []
    lib.kosmos_sizeof_history.restype = ctypes.c_size_t

    assert ctypes.sizeof(KosmosHistory) == lib.kosmos_sizeof_history(), "ABI size mismatch: KosmosHistory"
    assert ctypes.sizeof(KosmosGate) == lib.kosmos_sizeof_gate(), "ABI size mismatch: KosmosGate"

    lib.kosmos_mirror_apply.argtypes = [ctypes.POINTER(KosmosPhase), ctypes.POINTER(KosmosPhase)]
    lib.kosmos_mirror_apply.restype = ctypes.c_int

    lib.kosmos_symplectic_form.argtypes = [
        ctypes.POINTER(KosmosVec3),
        ctypes.POINTER(KosmosMom3),
        ctypes.POINTER(KosmosVec3),
        ctypes.POINTER(KosmosMom3),
        ctypes.POINTER(KosmosI192),
    ]
    lib.kosmos_symplectic_form.restype = ctypes.c_int

    lib.kosmos_i192_neg.argtypes = [ctypes.POINTER(KosmosI192), ctypes.POINTER(KosmosI192)]
    lib.kosmos_i192_neg.restype = None
    lib.kosmos_i192_eq.argtypes = [ctypes.POINTER(KosmosI192), ctypes.POINTER(KosmosI192)]
    lib.kosmos_i192_eq.restype = ctypes.c_int

    lib.kosmos_box_validate.argtypes = [ctypes.POINTER(KosmosLinfBox)]
    lib.kosmos_box_validate.restype = ctypes.c_int
    lib.kosmos_box_check.argtypes = [
        ctypes.POINTER(KosmosLinfBox),
        ctypes.POINTER(ctypes.c_int64),
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_uint32),
    ]
    lib.kosmos_box_check.restype = ctypes.c_int

    lib.kosmos_history_init.argtypes = [ctypes.POINTER(KosmosHistory), ctypes.c_uint64]
    lib.kosmos_history_init.restype = ctypes.c_int
    lib.kosmos_history_append.argtypes = [
        ctypes.POINTER(KosmosHistory),
        ctypes.c_uint64,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_history_append.restype = ctypes.c_int
    lib.kosmos_history_verify_chain.argtypes = [ctypes.POINTER(KosmosHistory)]
    lib.kosmos_history_verify_chain.restype = ctypes.c_int

    lib.kosmos_cantor_common_prefix.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    lib.kosmos_cantor_common_prefix.restype = ctypes.c_size_t

    lib.kosmos_cantor_closer.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    lib.kosmos_cantor_closer.restype = ctypes.c_int

    lib.kosmos_cantor_in_cylinder.argtypes = [
        ctypes.POINTER(KosmosHistory),
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    lib.kosmos_cantor_in_cylinder.restype = ctypes.c_int

    lib.kosmos_chu_evaluate.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_uint32,
        ctypes.c_uint32,
    ]
    lib.kosmos_chu_evaluate.restype = ctypes.c_int

    lib.kosmos_gate_init.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.POINTER(KosmosLinfBox),
        ctypes.POINTER(KosmosLinfBox),
        ctypes.c_uint32,
        ctypes.c_uint64,
    ]
    lib.kosmos_gate_init.restype = ctypes.c_int

    lib.kosmos_gate_mirror_chamber.argtypes = [ctypes.POINTER(KosmosGate), ctypes.POINTER(KosmosPhase)]
    lib.kosmos_gate_mirror_chamber.restype = ctypes.c_int

    lib.kosmos_gate_step_hold.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.POINTER(ctypes.c_int64),
        ctypes.POINTER(KosmosPhase),
        ctypes.POINTER(KosmosCtrl),
        ctypes.c_uint64,
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_gate_step_hold.restype = ctypes.c_int

    lib.kosmos_gate_authorize.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.POINTER(ctypes.c_int64),
        ctypes.POINTER(KosmosPhase),
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(KosmosWitnessPre),
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_gate_authorize.restype = ctypes.c_int

    lib.kosmos_gate_confirm_post.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.POINTER(KosmosWitnessPost),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_gate_confirm_post.restype = ctypes.c_int

    lib.kosmos_gate_power_loss.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_gate_power_loss.restype = ctypes.c_int

    lib.kosmos_gate_l01_recover.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.c_uint64,
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_gate_l01_recover.restype = ctypes.c_int

    # --- v0.2 bindings ---
    lib.kosmos_sizeof_channel_bank.argtypes = []
    lib.kosmos_sizeof_channel_bank.restype = ctypes.c_size_t
    assert (
        ctypes.sizeof(KosmosChannelBank) == lib.kosmos_sizeof_channel_bank()
    ), "ABI size mismatch: KosmosChannelBank"

    lib.kosmos_l01_sign_challenge.argtypes = [
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_uint64,
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_l01_sign_challenge.restype = None

    lib.kosmos_gate_cold_boot.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.c_char_p,
        ctypes.POINTER(KosmosLinfBox),
        ctypes.POINTER(KosmosLinfBox),
        ctypes.c_uint32,
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(KosmosLedgerEntry),
    ]
    lib.kosmos_gate_cold_boot.restype = ctypes.c_int

    lib.kosmos_wal_authorize.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_int64),
        ctypes.POINTER(KosmosPhase),
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(KosmosWitnessPre),
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_wal_authorize.restype = ctypes.c_int

    lib.kosmos_wal_confirm_post.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.c_char_p,
        ctypes.POINTER(KosmosWitnessPost),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_wal_confirm_post.restype = ctypes.c_int

    lib.kosmos_wal_l01_recover_signed.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_wal_l01_recover_signed.restype = ctypes.c_int

    lib.kosmos_channel_bank_init.argtypes = [
        ctypes.POINTER(KosmosChannelBank),
        ctypes.c_int64,
        ctypes.c_int64,
    ]
    lib.kosmos_channel_bank_init.restype = ctypes.c_int

    lib.kosmos_channel_set_neighbors.argtypes = [
        ctypes.POINTER(KosmosChannelBank),
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint16),
        ctypes.c_uint32,
    ]
    lib.kosmos_channel_set_neighbors.restype = ctypes.c_int

    lib.kosmos_channel_step.argtypes = [
        ctypes.POINTER(KosmosChannelBank),
        ctypes.c_uint32,
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.c_uint64,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_int64),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_channel_step.restype = ctypes.c_int

    lib.kosmos_tdc_check.argtypes = [
        ctypes.POINTER(KosmosTdcWindow),
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_tdc_check.restype = ctypes.c_int

    lib.kosmos_tdc_gated_authorize.argtypes = [
        ctypes.POINTER(KosmosGate),
        ctypes.POINTER(KosmosTdcWindow),
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_int64),
        ctypes.POINTER(KosmosPhase),
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(KosmosWitnessPre),
        ctypes.POINTER(KosmosCtrl),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_tdc_gated_authorize.restype = ctypes.c_int

    lib.kosmos_corner_guard_eval.argtypes = [
        ctypes.POINTER(KosmosCornerGuard),
        ctypes.POINTER(ctypes.c_int64),
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(KosmosU128),
    ]
    lib.kosmos_corner_guard_eval.restype = ctypes.c_int

    lib.kosmos_landauer_init.argtypes = [
        ctypes.POINTER(KosmosLandauerLedger),
        ctypes.c_uint64,
        KosmosU128,
    ]
    lib.kosmos_landauer_init.restype = ctypes.c_int

    lib.kosmos_landauer_record_prune.argtypes = [
        ctypes.POINTER(KosmosLandauerLedger),
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_landauer_record_prune.restype = ctypes.c_int

    # --- v0.3 bindings ---
    lib.kosmos_sizeof_v03_rtl_vec.argtypes = []
    lib.kosmos_sizeof_v03_rtl_vec.restype = ctypes.c_size_t
    assert (
        ctypes.sizeof(KosmosV03RtlVec) == lib.kosmos_sizeof_v03_rtl_vec()
    ), "ABI size mismatch: KosmosV03RtlVec"

    lib.kosmos_field_grad.argtypes = [ctypes.POINTER(KosmosScalarGrid), ctypes.POINTER(KosmosVecGrid)]
    lib.kosmos_field_grad.restype = ctypes.c_int
    lib.kosmos_field_curl.argtypes = [ctypes.POINTER(KosmosVecGrid), ctypes.POINTER(KosmosVecGrid)]
    lib.kosmos_field_curl.restype = ctypes.c_int
    lib.kosmos_field_div.argtypes = [ctypes.POINTER(KosmosVecGrid), ctypes.POINTER(KosmosScalarGrid)]
    lib.kosmos_field_div.restype = ctypes.c_int
    lib.kosmos_field_laplacian7.argtypes = [ctypes.POINTER(KosmosScalarGrid), ctypes.POINTER(KosmosScalarGrid)]
    lib.kosmos_field_laplacian7.restype = ctypes.c_int

    lib.kosmos_eval_faraday_lenz.argtypes = [
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_eval_faraday_lenz.restype = ctypes.c_int

    lib.kosmos_eval_newtonian_step.argtypes = [
        ctypes.POINTER(KosmosInertiaCfg),
        ctypes.POINTER(KosmosPoint3),
        ctypes.POINTER(KosmosPoint3),
        ctypes.POINTER(KosmosPoint3),
        ctypes.c_uint64,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_eval_newtonian_step.restype = ctypes.c_int

    lib.kosmos_eval_nash_equilibrium.argtypes = [
        ctypes.POINTER(KosmosNashSystem),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_eval_nash_equilibrium.restype = ctypes.c_int

    lib.kosmos_eval_lightcone_airgap.argtypes = [
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.c_int64,
        ctypes.c_uint64,
        ctypes.c_uint64,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.c_uint64,
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_eval_lightcone_airgap.restype = ctypes.c_int

    lib.kosmos_eval_entropy_ceiling.argtypes = [
        KosmosU128,
        KosmosU128,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_eval_entropy_ceiling.restype = ctypes.c_int

    lib.kosmos_eval_godel_witness.argtypes = [
        ctypes.c_uint64,
        ctypes.c_uint64,
        ctypes.c_uint8,
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_uint8),
    ]
    lib.kosmos_eval_godel_witness.restype = ctypes.c_int

    lib.kosmos_eval_v03_rtl_twin.argtypes = [ctypes.POINTER(KosmosV03RtlVec)]
    lib.kosmos_eval_v03_rtl_twin.restype = ctypes.c_uint8

    return lib
