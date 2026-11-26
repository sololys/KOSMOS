/*
 * KOSMOS v0.3 — Pairwise 3rd-Law Reciprocity & Strict Discrete Nash Equilibrium
 * CORE/kosmos_nash_reciprocity.c (0 float, 0 heap, 0 compiler extensions)
 *
 * Fixes the dead-cancellation double-sum bug by verifying C_ij == C_ji and
 * F_ij + F_ji == 0 pairwise for EVERY 0 <= i < j < K, and evaluates
 * 2 * |sum_j C_ij * q_j - B_i| < C_ii in exact 192-bit signed arithmetic.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_v03.h"

static void u64_mul_128(uint64_t a, uint64_t b, uint64_t *lo_out, uint64_t *hi_out) {
    uint64_t a_lo = a & 0xFFFFFFFFULL;
    uint64_t a_hi = a >> 32u;
    uint64_t b_lo = b & 0xFFFFFFFFULL;
    uint64_t b_hi = b >> 32u;

    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;

    uint64_t mid = (p0 >> 32u) + (p1 & 0xFFFFFFFFULL) + (p2 & 0xFFFFFFFFULL);
    *lo_out = (p0 & 0xFFFFFFFFULL) | (mid << 32u);
    *hi_out = p3 + (p1 >> 32u) + (p2 >> 32u) + (mid >> 32u);
}

static void i192_add(kosmos_i192_t *acc, const kosmos_i192_t *rhs) {
    uint64_t c = 0u;
    uint32_t k;
    for (k = 0u; k < 3u; ++k) {
        uint64_t s1 = acc->limb[k] + rhs->limb[k];
        uint64_t c1 = (s1 < acc->limb[k]) ? 1u : 0u;
        uint64_t s2 = s1 + c;
        uint64_t c2 = (s2 < s1) ? 1u : 0u;
        acc->limb[k] = s2;
        c = c1 + c2;
    }
}

static kosmos_i192_t i64_to_i192(int64_t v) {
    kosmos_i192_t r;
    r.limb[0] = (uint64_t)v;
    r.limb[1] = (v < 0) ? UINT64_MAX : 0u;
    r.limb[2] = (v < 0) ? UINT64_MAX : 0u;
    return r;
}

static kosmos_i192_t i64_mul_to_i192(int64_t a, int64_t b) {
    int neg = ((a < 0) ? 1 : 0) ^ ((b < 0) ? 1 : 0);
    uint64_t ua = (a < 0) ? (uint64_t)(-a) : (uint64_t)a;
    uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;
    kosmos_i192_t r;

    u64_mul_128(ua, ub, &r.limb[0], &r.limb[1]);
    r.limb[2] = 0u;
    if (neg && !kosmos_i192_is_zero(&r)) {
        kosmos_i192_neg(&r, &r);
    }
    return r;
}

static int i192_is_neg(const kosmos_i192_t *a) {
    return ((a->limb[2] >> 63u) != 0u) ? 1 : 0;
}

static int u192_lt(const kosmos_i192_t *a, const kosmos_i192_t *b) {
    if (a->limb[2] != b->limb[2]) {
        return (a->limb[2] < b->limb[2]) ? 1 : 0;
    }
    if (a->limb[1] != b->limb[1]) {
        return (a->limb[1] < b->limb[1]) ? 1 : 0;
    }
    return (a->limb[0] < b->limb[0]) ? 1 : 0;
}

kosmos_status_t kosmos_eval_nash_equilibrium(const kosmos_nash_system_t *sys,
                                             uint8_t *verdict_out) {
    uint32_t k, i, j;

    if (verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    *verdict_out = KOSMOS_VERDICT_KILL;
    if (sys == NULL) {
        return KOSMOS_ERR_NULL;
    }
    k = sys->k_channels;
    if (k == 0u || k > KOSMOS_NASH_MAX_K) {
        return KOSMOS_ERR_DOMAIN;
    }

    /* 1. Validate domain & positive self-stiffness C_ii > 0 */
    for (i = 0u; i < k; ++i) {
        int64_t c_ii = sys->c_matrix[i * KOSMOS_NASH_MAX_K + i];
        if (!kosmos_i64_valid(sys->q[i]) || !kosmos_i64_valid(sys->b_witness[i])) {
            return KOSMOS_ERR_DOMAIN;
        }
        if (!kosmos_i64_valid(c_ii) || c_ii <= 0) {
            *verdict_out = KOSMOS_VERDICT_KILL;
            return KOSMOS_OK;
        }
        for (j = 0u; j < k; ++j) {
            int64_t c_ij = sys->c_matrix[i * KOSMOS_NASH_MAX_K + j];
            if (!kosmos_i64_valid(c_ij)) {
                return KOSMOS_ERR_DOMAIN;
            }
            if (sys->has_pair_forces != 0u &&
                !kosmos_i64_valid(sys->f_pair[i * KOSMOS_NASH_MAX_K + j])) {
                return KOSMOS_ERR_DOMAIN;
            }
        }
    }

    /* 2. True pairwise Newton's 3rd Law check across EVERY pair 0 <= i < j < k */
    for (i = 0u; i < k; ++i) {
        if (sys->has_pair_forces != 0u && sys->f_pair[i * KOSMOS_NASH_MAX_K + i] != 0) {
            *verdict_out = KOSMOS_VERDICT_TRAP;
            return KOSMOS_OK;
        }
        for (j = i + 1u; j < k; ++j) {
            int64_t c_ij = sys->c_matrix[i * KOSMOS_NASH_MAX_K + j];
            int64_t c_ji = sys->c_matrix[j * KOSMOS_NASH_MAX_K + i];
            if (c_ij != c_ji) {
                *verdict_out = KOSMOS_VERDICT_TRAP;
                return KOSMOS_OK;
            }
            if (sys->has_pair_forces != 0u) {
                int64_t f_ij = sys->f_pair[i * KOSMOS_NASH_MAX_K + j];
                int64_t f_ji = sys->f_pair[j * KOSMOS_NASH_MAX_K + i];
                if (f_ij != -f_ji) {
                    *verdict_out = KOSMOS_VERDICT_TRAP;
                    return KOSMOS_OK;
                }
            }
        }
    }

    /* 3. Strict discrete Nash equilibrium: 2 * |sum_j C_ij q_j - B_i| < C_ii */
    for (i = 0u; i < k; ++i) {
        kosmos_i192_t acc = { { 0u, 0u, 0u } };
        kosmos_i192_t neg_b;
        kosmos_i192_t abs_res;
        kosmos_i192_t twice_abs_res;
        kosmos_i192_t c_ii_192;

        for (j = 0u; j < k; ++j) {
            int64_t c_ij = sys->c_matrix[i * KOSMOS_NASH_MAX_K + j];
            kosmos_i192_t prod = i64_mul_to_i192(c_ij, sys->q[j]);
            i192_add(&acc, &prod);
        }
        neg_b = i64_to_i192(-sys->b_witness[i]);
        i192_add(&acc, &neg_b);

        abs_res = acc;
        if (i192_is_neg(&abs_res)) {
            kosmos_i192_neg(&abs_res, &abs_res);
        }
        twice_abs_res = abs_res;
        i192_add(&twice_abs_res, &abs_res);

        c_ii_192 = i64_to_i192(sys->c_matrix[i * KOSMOS_NASH_MAX_K + i]);
        if (!u192_lt(&twice_abs_res, &c_ii_192)) {
            *verdict_out = KOSMOS_VERDICT_HOLD;
            return KOSMOS_OK;
        }
    }

    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}
