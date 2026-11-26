/*
 * KOSMOS v0.3 — 3D Cellular Exterior Calculus & Faraday-Lenz Sign Gate
 * CORE/kosmos_cellular_field.c (0 float, 0 heap, 0 compiler extensions)
 *
 * Implements staggered coboundary operators D0 (grad), D1 (curl), D2 (div),
 * and the 7-point integer Laplacian on a 3D cellular grid so that:
 *   - curl(grad(f)) == 0 identically (exact integer cancellation)
 *   - div(curl(e))  == 0 identically (exact integer cancellation)
 *   - checkerboard oscillations (+A, -A) are caught without blind spots
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_v03.h"
#include <string.h>

static uint32_t grid_idx(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t xm = x % KOSMOS_GRID_N;
    uint32_t ym = y % KOSMOS_GRID_N;
    uint32_t zm = z % KOSMOS_GRID_N;
    return (xm * KOSMOS_GRID_N + ym) * KOSMOS_GRID_N + zm;
}

static int checked_add_i64(int64_t a, int64_t b, int64_t *out) {
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

static int checked_sub_i64(int64_t a, int64_t b, int64_t *out) {
    if (!kosmos_i64_valid(a) || !kosmos_i64_valid(b)) {
        return 0;
    }
    return checked_add_i64(a, -b, out);
}

kosmos_status_t kosmos_field_grad(const kosmos_scalar_grid_t *f,
                                  kosmos_vec_grid_t *grad_out) {
    uint32_t x, y, z;
    if (f == NULL || grad_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    memset(grad_out, 0, sizeof(*grad_out));

    for (x = 0u; x < KOSMOS_GRID_N; ++x) {
        for (y = 0u; y < KOSMOS_GRID_N; ++y) {
            for (z = 0u; z < KOSMOS_GRID_N; ++z) {
                uint32_t c  = grid_idx(x, y, z);
                uint32_t xp = grid_idx(x + 1u, y, z);
                uint32_t yp = grid_idx(x, y + 1u, z);
                uint32_t zp = grid_idx(x, y, z + 1u);

                if (!checked_sub_i64(f->v[xp], f->v[c], &grad_out->x[c]) ||
                    !checked_sub_i64(f->v[yp], f->v[c], &grad_out->y[c]) ||
                    !checked_sub_i64(f->v[zp], f->v[c], &grad_out->z[c])) {
                    memset(grad_out, 0, sizeof(*grad_out));
                    return KOSMOS_ERR_OVERFLOW;
                }
            }
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_field_curl(const kosmos_vec_grid_t *edge,
                                  kosmos_vec_grid_t *curl_out) {
    uint32_t x, y, z;
    if (edge == NULL || curl_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    memset(curl_out, 0, sizeof(*curl_out));

    for (x = 0u; x < KOSMOS_GRID_N; ++x) {
        for (y = 0u; y < KOSMOS_GRID_N; ++y) {
            for (z = 0u; z < KOSMOS_GRID_N; ++z) {
                uint32_t c  = grid_idx(x, y, z);
                uint32_t xp = grid_idx(x + 1u, y, z);
                uint32_t yp = grid_idx(x, y + 1u, z);
                uint32_t zp = grid_idx(x, y, z + 1u);
                int64_t dz_dy, dy_dz, dx_dz, dz_dx, dy_dx, dx_dy;

                if (!checked_sub_i64(edge->z[yp], edge->z[c], &dz_dy) ||
                    !checked_sub_i64(edge->y[zp], edge->y[c], &dy_dz) ||
                    !checked_sub_i64(dz_dy, dy_dz, &curl_out->x[c]) ||
                    !checked_sub_i64(edge->x[zp], edge->x[c], &dx_dz) ||
                    !checked_sub_i64(edge->z[xp], edge->z[c], &dz_dx) ||
                    !checked_sub_i64(dx_dz, dz_dx, &curl_out->y[c]) ||
                    !checked_sub_i64(edge->y[xp], edge->y[c], &dy_dx) ||
                    !checked_sub_i64(edge->x[yp], edge->x[c], &dx_dy) ||
                    !checked_sub_i64(dy_dx, dx_dy, &curl_out->z[c])) {
                    memset(curl_out, 0, sizeof(*curl_out));
                    return KOSMOS_ERR_OVERFLOW;
                }
            }
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_field_div(const kosmos_vec_grid_t *face,
                                 kosmos_scalar_grid_t *div_out) {
    uint32_t x, y, z;
    if (face == NULL || div_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    memset(div_out, 0, sizeof(*div_out));

    for (x = 0u; x < KOSMOS_GRID_N; ++x) {
        for (y = 0u; y < KOSMOS_GRID_N; ++y) {
            for (z = 0u; z < KOSMOS_GRID_N; ++z) {
                uint32_t c  = grid_idx(x, y, z);
                uint32_t xp = grid_idx(x + 1u, y, z);
                uint32_t yp = grid_idx(x, y + 1u, z);
                uint32_t zp = grid_idx(x, y, z + 1u);
                int64_t dbx, dby, dbz, sxy;

                if (!checked_sub_i64(face->x[xp], face->x[c], &dbx) ||
                    !checked_sub_i64(face->y[yp], face->y[c], &dby) ||
                    !checked_sub_i64(face->z[zp], face->z[c], &dbz) ||
                    !checked_add_i64(dbx, dby, &sxy) ||
                    !checked_add_i64(sxy, dbz, &div_out->v[c])) {
                    memset(div_out, 0, sizeof(*div_out));
                    return KOSMOS_ERR_OVERFLOW;
                }
            }
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_field_laplacian7(const kosmos_scalar_grid_t *f,
                                        kosmos_scalar_grid_t *lap_out) {
    uint32_t x, y, z;
    const int64_t safe_center_max = KOSMOS_PHASE_MAX / 6;

    if (f == NULL || lap_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    memset(lap_out, 0, sizeof(*lap_out));

    for (x = 0u; x < KOSMOS_GRID_N; ++x) {
        for (y = 0u; y < KOSMOS_GRID_N; ++y) {
            for (z = 0u; z < KOSMOS_GRID_N; ++z) {
                uint32_t c  = grid_idx(x, y, z);
                uint32_t xp = grid_idx(x + 1u, y, z);
                uint32_t xm = grid_idx(x + KOSMOS_GRID_N - 1u, y, z);
                uint32_t yp = grid_idx(x, y + 1u, z);
                uint32_t ym = grid_idx(x, y + KOSMOS_GRID_N - 1u, z);
                uint32_t zp = grid_idx(x, y, z + 1u);
                uint32_t zm = grid_idx(x, y, z + KOSMOS_GRID_N - 1u);
                int64_t fc = f->v[c];
                int64_t s1, s2, s3, s4, s5, six_fc;

                if (!kosmos_i64_valid(fc) || fc > safe_center_max || fc < -safe_center_max) {
                    memset(lap_out, 0, sizeof(*lap_out));
                    return KOSMOS_ERR_OVERFLOW;
                }
                six_fc = 6 * fc;
                if (!checked_add_i64(f->v[xp], f->v[xm], &s1) ||
                    !checked_add_i64(s1, f->v[yp], &s2) ||
                    !checked_add_i64(s2, f->v[ym], &s3) ||
                    !checked_add_i64(s3, f->v[zp], &s4) ||
                    !checked_add_i64(s4, f->v[zm], &s5) ||
                    !checked_sub_i64(s5, six_fc, &lap_out->v[c])) {
                    memset(lap_out, 0, sizeof(*lap_out));
                    return KOSMOS_ERR_OVERFLOW;
                }
            }
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_eval_faraday_lenz(int64_t dphi_b,
                                         int64_t emf_induced,
                                         int64_t emf_max,
                                         uint8_t *verdict_out) {
    uint64_t abs_emf;
    if (verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (!kosmos_i64_valid(dphi_b) || !kosmos_i64_valid(emf_induced) ||
        !kosmos_i64_valid(emf_max) || emf_max <= 0) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_DOMAIN;
    }

    /* Lenz opposition law: induced EMF must never reinforce dPhi_B (E * dPhi_B <= 0) */
    if ((emf_induced > 0 && dphi_b > 0) || (emf_induced < 0 && dphi_b < 0)) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_OK;
    }

    abs_emf = (emf_induced < 0) ? (uint64_t)(-emf_induced) : (uint64_t)emf_induced;
    if (abs_emf > (uint64_t)emf_max) {
        *verdict_out = KOSMOS_VERDICT_TRAP;
        return KOSMOS_OK;
    }

    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}
