/*
 * KOSMOS v0.3 — Relativistic Lightcone, Airgap, Entropy Sink & Godel Witness
 * CORE/kosmos_horizon_godel.c (0 float, 0 heap, 0 compiler extensions)
 *
 * Includes kosmos_eval_v03_rtl_twin() with bit-exact XOR == 0 parity against
 * RTL/kosmos_v03_physics_gate.sv.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_v03.h"

static kosmos_u128_t mul_u64(uint64_t a, uint64_t b) {
    uint64_t a_lo = a & 0xFFFFFFFFULL;
    uint64_t a_hi = a >> 32u;
    uint64_t b_lo = b & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32u;

    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;

    uint64_t mid = (p0 >> 32u) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
    kosmos_u128_t r;
    r.lo = (p0 & 0xFFFFFFFFULL) | (mid << 32u);
    r.hi = p3 + (p1 >> 32u) + (p2 >> 32u) + (mid >> 32u);
    return r;
}

static int u128_add(kosmos_u128_t a, kosmos_u128_t b, kosmos_u128_t *out) {
    uint64_t lo = a.lo + b.lo;
    uint64_t c = (lo < a.lo) ? 1u : 0u;
    uint64_t hi1 = a.hi + b.hi;
    uint64_t c1 = (hi1 < a.hi) ? 1u : 0u;
    uint64_t hi2 = hi1 + c;
    uint64_t c2 = (hi2 < hi1) ? 1u : 0u;
    if ((c1 + c2) > 0u) {
        return 0;
    }
    out->lo = lo;
    out->hi = hi2;
    return 1;
}

static int u128_gt(kosmos_u128_t a, kosmos_u128_t b) {
    if (a.hi != b.hi) {
        return (a.hi > b.hi) ? 1 : 0;
    }
    return (a.lo > b.lo) ? 1 : 0;
}

kosmos_status_t kosmos_eval_lightcone_airgap(int64_t dx,
                                             int64_t dy,
                                             int64_t dz,
                                             uint64_t dt_ticks,
                                             uint64_t c_per_tick,
                                             uint8_t airgap_sealed,
                                             uint8_t ancestry_receipt_valid,
                                             uint64_t w_lift_work,
                                             uint8_t unmerged_fork,
                                             uint8_t *verdict_out) {
    uint64_t ux, uy, uz;
    kosmos_u128_t x2, y2, z2, rxy2, dr_sq;
    kosmos_u128_t ct_128, ct_sq;

    if (verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    *verdict_out = KOSMOS_VERDICT_KILL;
    if (!kosmos_i64_valid(dx) || !kosmos_i64_valid(dy) || !kosmos_i64_valid(dz) || c_per_tick == 0u) {
        return KOSMOS_ERR_DOMAIN;
    }

    ux = (dx < 0) ? (uint64_t)(-dx) : (uint64_t)dx;
    uy = (dy < 0) ? (uint64_t)(-dy) : (uint64_t)dy;
    uz = (dz < 0) ? (uint64_t)(-dz) : (uint64_t)dz;

    x2 = mul_u64(ux, ux);
    y2 = mul_u64(uy, uy);
    z2 = mul_u64(uz, uz);
    if (!u128_add(x2, y2, &rxy2) || !u128_add(rxy2, z2, &dr_sq)) {
        return KOSMOS_ERR_OVERFLOW;
    }

    ct_128 = mul_u64(c_per_tick, dt_ticks);
    if (ct_128.hi == 0u) {
        ct_sq = mul_u64(ct_128.lo, ct_128.lo);
        if (u128_gt(dr_sq, ct_sq)) {
            /* ds^2 > 0 : superluminal / spacelike injection -> KILL */
            *verdict_out = KOSMOS_VERDICT_KILL;
            return KOSMOS_OK;
        }
    }
    /* Note: if ct_128 >= 2^64, then (c * dt)^2 >= 2^128 > dr_sq, so dr_sq <= ct_sq holds automatically! */

    /* Claimed cross-barrier receipt after opening airgap with zero physical work -> KILL */
    if (airgap_sealed == 0u && ancestry_receipt_valid != 0u && w_lift_work == 0u) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_OK;
    }

    /* Sealed airgap without receipt or unmerged fork -> HOLD (AWAITING_SYNC) */
    if ((airgap_sealed != 0u && ancestry_receipt_valid == 0u) ||
        ancestry_receipt_valid == 0u ||
        unmerged_fork != 0u) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }

    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_eval_entropy_ceiling(kosmos_u128_t s_dot_gen,
                                            kosmos_u128_t s_dot_sink,
                                            uint8_t *verdict_out) {
    if (verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    *verdict_out = u128_gt(s_dot_gen, s_dot_sink) ? KOSMOS_VERDICT_KILL : KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_eval_godel_witness(uint64_t formal_iters,
                                          uint64_t max_iters,
                                          uint8_t undecidable_hit,
                                          uint8_t has_ext_witness,
                                          uint8_t *verdict_out) {
    if (verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (formal_iters > max_iters && has_ext_witness == 0u) {
        *verdict_out = KOSMOS_VERDICT_TRAP;
        return KOSMOS_OK;
    }
    if (undecidable_hit != 0u && has_ext_witness == 0u) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }
    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}

/* --- Bit-exact software twin of RTL/kosmos_v03_physics_gate.sv --- */
static int is_i32_poison(int32_t v) {
    return (v == INT32_MIN) ? 1 : 0;
}

static uint32_t abs_i32_safe(int32_t v) {
    return (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
}

uint8_t kosmos_eval_v03_rtl_twin(const kosmos_v03_rtl_vec_t *in) {
    int rail_ok;
    int any_poison;
    int lenz_violation;
    uint32_t abs_emf;
    uint32_t abs_force;
    int64_t f_sum;

    if (in == NULL) {
        return KOSMOS_VERDICT_KILL;
    }

    rail_ok = (((in->rail_p ^ in->rail_n) & 0x0Fu) == 0x0Fu) ? 1 : 0;
    any_poison = is_i32_poison(in->dphi_b) ||
                 is_i32_poison(in->emf_induced) ||
                 is_i32_poison(in->emf_max) ||
                 is_i32_poison(in->force_in) ||
                 is_i32_poison(in->force_max) ||
                 is_i32_poison(in->f_ab) ||
                 is_i32_poison(in->f_ba) ||
                 is_i32_poison(in->nash_res_abs_2x) ||
                 is_i32_poison(in->nash_c_ii);

    lenz_violation = ((in->emf_induced > 0 && in->dphi_b > 0) ||
                      (in->emf_induced < 0 && in->dphi_b < 0)) ? 1 : 0;

    /* Priority 1: KILL (0x00) */
    if (!rail_ok || any_poison ||
        in->dt_ticks == 0u ||
        in->emf_max <= 0 ||
        in->force_max <= 0 ||
        in->nash_c_ii <= 0 ||
        in->nash_res_abs_2x < 0 ||
        lenz_violation ||
        (in->dr_sq > in->ct_sq) ||
        (in->s_dot_gen > in->s_dot_sink) ||
        (in->airgap_sealed == 0u && in->ancestry_valid != 0u && in->w_lift_work == 0u)) {
        return KOSMOS_VERDICT_KILL;
    }

    abs_emf   = abs_i32_safe(in->emf_induced);
    abs_force = abs_i32_safe(in->force_in);
    f_sum     = (int64_t)in->f_ab + (int64_t)in->f_ba;

    /* Priority 2: TRAP (0x5A) */
    if ((abs_emf > (uint32_t)in->emf_max) ||
        (abs_force > (uint32_t)in->force_max) ||
        (f_sum != 0) ||
        (in->formal_iters > in->max_iters && in->has_ext_witness == 0u)) {
        return KOSMOS_VERDICT_TRAP;
    }

    /* Priority 3: HOLD (0x3C) */
    if (in->ancestry_valid == 0u ||
        in->unmerged_fork != 0u ||
        (in->energy_rem_2x < in->w_min_2x) ||
        (in->energy_rem_2x < in->energy_cost_2x) ||
        (in->nash_res_abs_2x >= in->nash_c_ii) ||
        (in->undecidable_hit != 0u && in->has_ext_witness == 0u)) {
        return KOSMOS_VERDICT_HOLD;
    }

    /* Priority 4: OPEN (0xA5) */
    return KOSMOS_VERDICT_OPEN;
}

size_t kosmos_sizeof_v03_rtl_vec(void) {
    return sizeof(kosmos_v03_rtl_vec_t);
}
