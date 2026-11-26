/*
 * KOSMOS v0.2 — 128-Bit Exact Corner Guard & Logical Landauer Branch Ledger
 * CORE/kosmos_energy_128.c (0 float, 0 square root, 0 compiler extensions)
 *
 * Catches Euclidean/quadratic corner overloads such as (80, 80, 10) vs 100^2
 * that slip inside L-infinity <= 100, and tracks the logical information-
 * theoretic Landauer lower bound (explicitly distinct from physical calorimetry).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_v02.h"
#include <string.h>

static uint64_t ordered_u64(int64_t v) {
    return ((uint64_t)v) ^ ((uint64_t)1ULL << 63u);
}

static uint64_t exact_dist_u64(int64_t a, int64_t b) {
    uint64_t ua = ordered_u64(a);
    uint64_t ub = ordered_u64(b);
    return (ua >= ub) ? (ua - ub) : (ub - ua);
}

static kosmos_u128_t u64_mul_u128(uint64_t a, uint64_t b) {
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

static int u128_add_checked(kosmos_u128_t a, kosmos_u128_t b, kosmos_u128_t *out) {
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

/* Multiply u128 by u64 with exact 128-bit overflow detection */
static int u128_mul_u64_checked(kosmos_u128_t a, uint64_t w, kosmos_u128_t *out) {
    kosmos_u128_t p_lo;
    kosmos_u128_t p_hi;
    uint64_t hi_sum;

    if (w == 0u) {
        out->lo = 0u;
        out->hi = 0u;
        return 1;
    }
    p_lo = u64_mul_u128(a.lo, w);
    p_hi = u64_mul_u128(a.hi, w);
    if (p_hi.hi != 0u) {
        return 0;
    }
    hi_sum = p_lo.hi + p_hi.lo;
    if (hi_sum < p_lo.hi) {
        return 0;
    }
    out->lo = p_lo.lo;
    out->hi = hi_sum;
    return 1;
}

static int u128_leq(kosmos_u128_t a, kosmos_u128_t b) {
    if (a.hi != b.hi) {
        return (a.hi < b.hi) ? 1 : 0;
    }
    return (a.lo <= b.lo) ? 1 : 0;
}

kosmos_status_t kosmos_corner_guard_eval(const kosmos_corner_guard_t *guard,
                                         const int64_t vec[KOSMOS_DIM],
                                         uint8_t *inside_out,
                                         kosmos_u128_t *energy_sq_out) {
    kosmos_u128_t acc = { 0u, 0u };
    uint32_t i;

    if (inside_out != NULL) {
        *inside_out = 0u;
    }
    if (energy_sq_out != NULL) {
        energy_sq_out->lo = 0u;
        energy_sq_out->hi = 0u;
    }
    if (guard == NULL || vec == NULL || inside_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    for (i = 0u; i < KOSMOS_DIM; ++i) {
        uint64_t d;
        kosmos_u128_t d_sq;
        kosmos_u128_t weighted;

        if (!kosmos_i64_valid(guard->ref[i]) || !kosmos_i64_valid(vec[i]) || guard->weight[i] == 0u) {
            return KOSMOS_ERR_DOMAIN;
        }
        d = exact_dist_u64(vec[i], guard->ref[i]);
        d_sq = u64_mul_u128(d, d);
        if (!u128_mul_u64_checked(d_sq, guard->weight[i], &weighted)) {
            return KOSMOS_ERR_OVERFLOW;
        }
        if (!u128_add_checked(acc, weighted, &acc)) {
            return KOSMOS_ERR_OVERFLOW;
        }
    }

    if (energy_sq_out != NULL) {
        *energy_sq_out = acc;
    }
    *inside_out = (uint8_t)u128_leq(acc, guard->max_energy_sq);
    return KOSMOS_OK;
}

uint32_t kosmos_ceil_log2_u64(uint64_t n) {
    uint64_t v;
    uint32_t bits = 0u;
    if (n <= 1u) {
        return 0u;
    }
    v = n - 1u;
    while (v > 0u) {
        v >>= 1u;
        bits += 1u;
    }
    return bits;
}

kosmos_status_t kosmos_landauer_init(kosmos_landauer_ledger_t *led,
                                     uint64_t ebit_milli_zj,
                                     kosmos_u128_t budget_milli_zj) {
    if (led == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (ebit_milli_zj == 0u) {
        return KOSMOS_ERR_DOMAIN;
    }
    memset(led, 0, sizeof(*led));
    led->ebit_zepto_joule_milli = ebit_milli_zj;
    led->budget_milli_zj = budget_milli_zj;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_landauer_record_prune(kosmos_landauer_ledger_t *led,
                                             uint64_t discarded_branches,
                                             uint8_t *within_budget_out) {
    uint64_t bits_added;
    kosmos_u128_t delta_e;
    kosmos_u128_t next_e;

    if (within_budget_out != NULL) {
        *within_budget_out = 0u;
    }
    if (led == NULL || within_budget_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (discarded_branches == 0u) {
        *within_budget_out = (uint8_t)u128_leq(led->logical_min_milli_zj, led->budget_milli_zj);
        return KOSMOS_OK;
    }
    if (discarded_branches > (UINT64_MAX - led->pruned_branches)) {
        return KOSMOS_ERR_OVERFLOW;
    }

    /* Choosing 1 survivor out of (1 + discarded_branches) branches erases at least
     * ceil(log2(1 + discarded_branches)) logical bits of branch ambiguity. */
    bits_added = (discarded_branches == UINT64_MAX)
                     ? 64u
                     : (uint64_t)kosmos_ceil_log2_u64(discarded_branches + 1u);
    if (bits_added > (UINT64_MAX - led->erased_bits_min)) {
        return KOSMOS_ERR_OVERFLOW;
    }

    delta_e = u64_mul_u128(bits_added, led->ebit_zepto_joule_milli);
    if (!u128_add_checked(led->logical_min_milli_zj, delta_e, &next_e)) {
        return KOSMOS_ERR_OVERFLOW;
    }

    led->pruned_branches += discarded_branches;
    led->erased_bits_min += bits_added;
    led->logical_min_milli_zj = next_e;
    *within_budget_out = (uint8_t)u128_leq(led->logical_min_milli_zj, led->budget_milli_zj);
    return KOSMOS_OK;
}
