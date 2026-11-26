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
KOSMOS_VERDICT_OPEN = 0xA5

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

    return lib
