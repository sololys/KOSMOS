/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * CORE/kosmos_mirror.c : Affine 3D floor Q, Symplectic T*Q, and Mirror J
 *
 * Enforces J(q, p) = (q, -p), J^2 = I, and J* omega = -omega in exact 192-bit
 * signed integer arithmetic with KOSMOS_POISON_MIN (INT64_MIN) fenced out.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_space.h"
#include <string.h>

int kosmos_i64_valid(int64_t v) {
    return (v != KOSMOS_POISON_MIN) ? 1 : 0;
}

int kosmos_phase_valid(const kosmos_phase_t *z) {
    uint32_t i;
    if (z == NULL) {
        return 0;
    }
    for (i = 0u; i < KOSMOS_DIM; ++i) {
        if (!kosmos_i64_valid(z->q.q[i]) || !kosmos_i64_valid(z->p.p[i])) {
            return 0;
        }
    }
    return 1;
}

int kosmos_ctrl_valid(const kosmos_ctrl_t *u) {
    uint32_t i;
    if (u == NULL) {
        return 0;
    }
    for (i = 0u; i < KOSMOS_CTRL_DIM; ++i) {
        if (!kosmos_i64_valid(u->u[i])) {
            return 0;
        }
    }
    return 1;
}

static int safe_add_i64(int64_t a, int64_t b, int64_t *out) {
    if (!kosmos_i64_valid(a) || !kosmos_i64_valid(b)) {
        return 0;
    }
    if (b > 0 && a > (KOSMOS_PHASE_MAX - b)) {
        return 0;
    }
    if (b < 0 && a < (-KOSMOS_PHASE_MAX - b)) {
        return 0;
    }
    *out = a + b;
    return 1;
}

kosmos_status_t kosmos_affine_diff(const kosmos_point3_t *q2,
                                   const kosmos_point3_t *q1,
                                   kosmos_vec3_t *dq_out) {
    uint32_t i;
    if (q2 == NULL || q1 == NULL || dq_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    for (i = 0u; i < KOSMOS_DIM; ++i) {
        if (!kosmos_i64_valid(q2->q[i]) || !kosmos_i64_valid(q1->q[i])) {
            memset(dq_out, 0, sizeof(*dq_out));
            return KOSMOS_ERR_DOMAIN;
        }
        /* Safe negation -q1->q[i] is guaranteed since q1->q[i] != INT64_MIN */
        if (!safe_add_i64(q2->q[i], -q1->q[i], &dq_out->dq[i])) {
            memset(dq_out, 0, sizeof(*dq_out));
            return KOSMOS_ERR_OVERFLOW;
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_affine_translate(const kosmos_point3_t *q,
                                        const kosmos_vec3_t *dq,
                                        kosmos_point3_t *q_out) {
    uint32_t i;
    if (q == NULL || dq == NULL || q_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    for (i = 0u; i < KOSMOS_DIM; ++i) {
        if (!kosmos_i64_valid(q->q[i]) || !kosmos_i64_valid(dq->dq[i])) {
            memset(q_out, 0, sizeof(*q_out));
            return KOSMOS_ERR_DOMAIN;
        }
        if (!safe_add_i64(q->q[i], dq->dq[i], &q_out->q[i])) {
            memset(q_out, 0, sizeof(*q_out));
            return KOSMOS_ERR_OVERFLOW;
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_mirror_apply(const kosmos_phase_t *in,
                                    kosmos_phase_t *out) {
    uint32_t i;
    kosmos_phase_t tmp;

    if (in == NULL || out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (!kosmos_phase_valid(in)) {
        memset(out, 0, sizeof(*out));
        return KOSMOS_ERR_DOMAIN;
    }

    for (i = 0u; i < KOSMOS_DIM; ++i) {
        tmp.q.q[i] = in->q.q[i];
        tmp.p.p[i] = (in->p.p[i] == 0) ? 0 : -in->p.p[i];
    }

    *out = tmp;
    return KOSMOS_OK;
}

/* --- Portable 192-bit signed arithmetic for symplectic 2-form omega --- */

static void u64_mul_u128(uint64_t a, uint64_t b, uint64_t *lo_out, uint64_t *hi_out) {
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

static void i192_add_inplace(kosmos_i192_t *acc, const kosmos_i192_t *rhs) {
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

void kosmos_i192_neg(const kosmos_i192_t *in, kosmos_i192_t *out) {
    kosmos_i192_t inv;
    kosmos_i192_t one;
    if (in == NULL || out == NULL) {
        return;
    }
    inv.limb[0] = ~in->limb[0];
    inv.limb[1] = ~in->limb[1];
    inv.limb[2] = ~in->limb[2];
    one.limb[0] = 1u;
    one.limb[1] = 0u;
    one.limb[2] = 0u;
    i192_add_inplace(&inv, &one);
    *out = inv;
}

int kosmos_i192_eq(const kosmos_i192_t *a, const kosmos_i192_t *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }
    return (a->limb[0] == b->limb[0] &&
            a->limb[1] == b->limb[1] &&
            a->limb[2] == b->limb[2]) ? 1 : 0;
}

int kosmos_i192_is_zero(const kosmos_i192_t *a) {
    if (a == NULL) {
        return 0;
    }
    return (a->limb[0] == 0u && a->limb[1] == 0u && a->limb[2] == 0u) ? 1 : 0;
}

static kosmos_i192_t i64_mul_i192(int64_t a, int64_t b) {
    int neg = ((a < 0) ? 1 : 0) ^ ((b < 0) ? 1 : 0);
    uint64_t ua = (a < 0) ? (uint64_t)(-a) : (uint64_t)a;
    uint64_t ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;
    kosmos_i192_t res;

    u64_mul_u128(ua, ub, &res.limb[0], &res.limb[1]);
    res.limb[2] = 0u;
    if (neg && !kosmos_i192_is_zero(&res)) {
        kosmos_i192_neg(&res, &res);
    }
    return res;
}

kosmos_status_t kosmos_symplectic_form(const kosmos_vec3_t *dq1,
                                       const kosmos_mom3_t *dp1,
                                       const kosmos_vec3_t *dq2,
                                       const kosmos_mom3_t *dp2,
                                       kosmos_i192_t *omega_out) {
    uint32_t i;
    kosmos_i192_t acc;

    if (dq1 == NULL || dp1 == NULL || dq2 == NULL || dp2 == NULL || omega_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    acc.limb[0] = 0u;
    acc.limb[1] = 0u;
    acc.limb[2] = 0u;

    for (i = 0u; i < KOSMOS_DIM; ++i) {
        kosmos_i192_t term_pos;
        kosmos_i192_t term_neg;
        if (!kosmos_i64_valid(dq1->dq[i]) || !kosmos_i64_valid(dp1->p[i]) ||
            !kosmos_i64_valid(dq2->dq[i]) || !kosmos_i64_valid(dp2->p[i])) {
            memset(omega_out, 0, sizeof(*omega_out));
            return KOSMOS_ERR_DOMAIN;
        }
        term_pos = i64_mul_i192(dq1->dq[i], dp2->p[i]);
        term_neg = i64_mul_i192(dp1->p[i], dq2->dq[i]);
        kosmos_i192_neg(&term_neg, &term_neg);
        i192_add_inplace(&acc, &term_pos);
        i192_add_inplace(&acc, &term_neg);
    }

    *omega_out = acc;
    return KOSMOS_OK;
}
