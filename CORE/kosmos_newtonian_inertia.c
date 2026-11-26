/*
 * KOSMOS v0.3 — Newtonian Local Inertia, Doubled Kinetic Work & Right to Rest
 * CORE/kosmos_newtonian_inertia.c (0 float, 0 heap, 0 compiler extensions)
 *
 * Enforces:
 *   - dt_ticks > 0 and mass_i >= 1 (no massless / instantaneous jumps -> KILL)
 *   - m_i * |d2q_i| <= F_max_i * dt_ticks in exact 128-bit unsigned arithmetic (-> TRAP)
 *   - Exact doubled kinetic energy check:
 *       E_rem >= W_min  &&  2 * E_rem >= max(0, W_2x) + 2 * Q_toll
 *     eliminating the 1/2 == 0 integer division truncation bug (-> HOLD on low energy).
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

static int u128_sub(kosmos_u128_t a, kosmos_u128_t b, kosmos_u128_t *out) {
    uint64_t borrow;
    if (a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo)) {
        return 0;
    }
    borrow = (a.lo < b.lo) ? 1u : 0u;
    out->lo = a.lo - b.lo;
    out->hi = a.hi - b.hi - borrow;
    return 1;
}

static int u128_lt(kosmos_u128_t a, kosmos_u128_t b) {
    if (a.hi != b.hi) {
        return (a.hi < b.hi) ? 1 : 0;
    }
    return (a.lo < b.lo) ? 1 : 0;
}

static int u128_mul_u64(kosmos_u128_t a, uint64_t m, kosmos_u128_t *out) {
    kosmos_u128_t plo;
    kosmos_u128_t phi;
    uint64_t hi_sum;
    if (m == 0u) {
        out->lo = 0u;
        out->hi = 0u;
        return 1;
    }
    plo = mul_u64(a.lo, m);
    phi = mul_u64(a.hi, m);
    if (phi.hi != 0u) {
        return 0;
    }
    hi_sum = plo.hi + phi.lo;
    if (hi_sum < plo.hi) {
        return 0;
    }
    out->lo = plo.lo;
    out->hi = hi_sum;
    return 1;
}

static uint64_t ordered_u64(int64_t v) {
    return ((uint64_t)v) ^ ((uint64_t)1ULL << 63u);
}

/* Represent signed diff (a - b) as (sign_neg, mag_u64) with zero UB */
static void signed_diff_i64(int64_t a, int64_t b, int *neg_out, uint64_t *mag_out) {
    uint64_t ua = ordered_u64(a);
    uint64_t ub = ordered_u64(b);
    if (ua >= ub) {
        *neg_out = 0;
        *mag_out = ua - ub;
    } else {
        *neg_out = 1;
        *mag_out = ub - ua;
    }
}

/* Compute |dq2 - dq1| as exact u128 where dq = (neg, mag_u64) */
static kosmos_u128_t abs_second_diff(int neg1, uint64_t mag1, int neg2, uint64_t mag2) {
    kosmos_u128_t r = { 0u, 0u };
    /* dq2 - dq1 = dq2 + (-dq1). Let neg1_flipped = !neg1 (if mag1 > 0). */
    int n1 = (mag1 == 0u) ? neg2 : (1 - neg1);
    if (mag1 == 0u) {
        r.lo = mag2;
        return r;
    }
    if (mag2 == 0u) {
        r.lo = mag1;
        return r;
    }
    if (neg2 == n1) {
        /* Same sign: magnitude adds into u128 */
        uint64_t s = mag2 + mag1;
        r.lo = s;
        r.hi = (s < mag2) ? 1u : 0u;
    } else {
        r.lo = (mag2 >= mag1) ? (mag2 - mag1) : (mag1 - mag2);
    }
    return r;
}

kosmos_status_t kosmos_eval_newtonian_step(const kosmos_inertia_cfg_t *cfg,
                                           const kosmos_point3_t *q_prev2,
                                           const kosmos_point3_t *q_prev1,
                                           const kosmos_point3_t *q_curr,
                                           uint64_t dt_ticks,
                                           uint8_t *verdict_out) {
    uint32_t i;
    kosmos_u128_t pos_work_2x = { 0u, 0u };
    kosmos_u128_t neg_work_2x = { 0u, 0u };
    kosmos_u128_t net_pos_work_2x = { 0u, 0u };
    kosmos_u128_t toll_2x;
    kosmos_u128_t total_req_2x;
    kosmos_u128_t erem_2x;

    if (verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    *verdict_out = KOSMOS_VERDICT_KILL;
    if (cfg == NULL || q_prev2 == NULL || q_prev1 == NULL || q_curr == NULL) {
        return KOSMOS_ERR_NULL;
    }

    /* Axiom 1: Zero duration or non-positive mass/force ceiling -> immediate KILL */
    if (dt_ticks == 0u) {
        return KOSMOS_OK;
    }
    for (i = 0u; i < KOSMOS_DIM; ++i) {
        if (!kosmos_i64_valid(cfg->mass[i]) || cfg->mass[i] <= 0 ||
            !kosmos_i64_valid(cfg->force_max[i]) || cfg->force_max[i] <= 0 ||
            !kosmos_i64_valid(q_prev2->q[i]) ||
            !kosmos_i64_valid(q_prev1->q[i]) ||
            !kosmos_i64_valid(q_curr->q[i])) {
            return KOSMOS_ERR_DOMAIN;
        }
    }

    /* Check impulse ceiling per axis: m_i * |d2q_i| <= F_max_i * dt_ticks */
    for (i = 0u; i < KOSMOS_DIM; ++i) {
        int neg1 = 0, neg2 = 0;
        uint64_t dq1 = 0u, dq2 = 0u;
        kosmos_u128_t d2q_abs;
        kosmos_u128_t impulse_lhs;
        kosmos_u128_t impulse_rhs;
        kosmos_u128_t dq1_sq, dq2_sq, term_kin;

        signed_diff_i64(q_prev1->q[i], q_prev2->q[i], &neg1, &dq1);
        signed_diff_i64(q_curr->q[i],  q_prev1->q[i], &neg2, &dq2);

        d2q_abs = abs_second_diff(neg1, dq1, neg2, dq2);
        if (!u128_mul_u64(d2q_abs, (uint64_t)cfg->mass[i], &impulse_lhs)) {
            return KOSMOS_ERR_OVERFLOW;
        }
        impulse_rhs = mul_u64((uint64_t)cfg->force_max[i], dt_ticks);
        if (u128_lt(impulse_rhs, impulse_lhs)) {
            *verdict_out = KOSMOS_VERDICT_TRAP;
            return KOSMOS_OK;
        }

        /* Accumulate positive and negative kinetic energy deltas separately in u128 */
        dq1_sq = mul_u64(dq1, dq1);
        dq2_sq = mul_u64(dq2, dq2);
        if (u128_lt(dq1_sq, dq2_sq)) {
            kosmos_u128_t diff_sq;
            (void)u128_sub(dq2_sq, dq1_sq, &diff_sq);
            if (!u128_mul_u64(diff_sq, (uint64_t)cfg->mass[i], &term_kin) ||
                !u128_add(pos_work_2x, term_kin, &pos_work_2x)) {
                return KOSMOS_ERR_OVERFLOW;
            }
        } else if (u128_lt(dq2_sq, dq1_sq)) {
            kosmos_u128_t diff_sq;
            (void)u128_sub(dq1_sq, dq2_sq, &diff_sq);
            if (!u128_mul_u64(diff_sq, (uint64_t)cfg->mass[i], &term_kin) ||
                !u128_add(neg_work_2x, term_kin, &neg_work_2x)) {
                return KOSMOS_ERR_OVERFLOW;
            }
        }
    }

    /* max(0, W_2x) = (pos_work_2x > neg_work_2x) ? (pos_work_2x - neg_work_2x) : 0 */
    if (u128_lt(neg_work_2x, pos_work_2x)) {
        (void)u128_sub(pos_work_2x, neg_work_2x, &net_pos_work_2x);
    }

    /* Axiom 3: Right to Rest -> HOLD if E_rem < W_min or 2*E_rem < net_pos_work_2x + 2*Q_toll */
    if (u128_lt(cfg->energy_rem, cfg->w_min)) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }
    if (!u128_add(cfg->entropy_toll, cfg->entropy_toll, &toll_2x) ||
        !u128_add(net_pos_work_2x, toll_2x, &total_req_2x) ||
        !u128_add(cfg->energy_rem, cfg->energy_rem, &erem_2x)) {
        return KOSMOS_ERR_OVERFLOW;
    }
    if (u128_lt(erem_2x, total_req_2x)) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }

    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}
