/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * CORE/kosmos_boundary.c : Division-free, overflow-free L-infinity boxes
 *
 * Checks |x_i - r_i| <= tau_i (tau_i > 0) using offset-binary uint64_t order
 * isomorphism so no float, no truncating division, and no signed overflow
 * can ever occur.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_space.h"

static uint64_t i64_to_ordered_u64(int64_t v) {
    return ((uint64_t)v) ^ ((uint64_t)1ULL << 63u);
}

static uint64_t exact_abs_diff_i64(int64_t a, int64_t b) {
    uint64_t ua = i64_to_ordered_u64(a);
    uint64_t ub = i64_to_ordered_u64(b);
    return (ua >= ub) ? (ua - ub) : (ub - ua);
}

kosmos_status_t kosmos_box_validate(const kosmos_linf_box_t *box) {
    uint32_t i;
    if (box == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (box->count == 0u || box->count > KOSMOS_GUARD_DIM) {
        return KOSMOS_ERR_DOMAIN;
    }
    for (i = 0u; i < box->count; ++i) {
        if (!kosmos_i64_valid(box->ref[i]) || !kosmos_i64_valid(box->tol[i])) {
            return KOSMOS_ERR_DOMAIN;
        }
        if (box->tol[i] <= 0) {
            return KOSMOS_ERR_DOMAIN;
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_box_check(const kosmos_linf_box_t *box,
                                 const int64_t *values,
                                 uint32_t value_count,
                                 uint8_t *inside_out,
                                 uint32_t *breach_index_out) {
    uint32_t i;
    kosmos_status_t st;

    if (inside_out != NULL) {
        *inside_out = 0u;
    }
    if (breach_index_out != NULL) {
        *breach_index_out = 0u;
    }
    if (box == NULL || values == NULL || inside_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    st = kosmos_box_validate(box);
    if (st != KOSMOS_OK) {
        return st;
    }
    if (value_count < box->count) {
        return KOSMOS_ERR_DOMAIN;
    }

    for (i = 0u; i < box->count; ++i) {
        uint64_t dist;
        if (!kosmos_i64_valid(values[i])) {
            if (breach_index_out != NULL) {
                *breach_index_out = i;
            }
            return KOSMOS_ERR_DOMAIN;
        }
        dist = exact_abs_diff_i64(values[i], box->ref[i]);
        if (dist > (uint64_t)box->tol[i]) {
            *inside_out = 0u;
            if (breach_index_out != NULL) {
                *breach_index_out = i;
            }
            return KOSMOS_OK;
        }
    }

    *inside_out = 1u;
    return KOSMOS_OK;
}
