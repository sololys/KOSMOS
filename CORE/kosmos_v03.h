/*
 * KOSMOS v0.3 — Universal Physical Law Modules (0 float, 0 heap, 0 extensions)
 * CORE/kosmos_v03.h
 *
 * Covers:
 *   - SPEC/09 : 3D Cellular Exterior Calculus (grad, curl, div, laplacian7) & Lenz
 *   - SPEC/10 : Newtonian Local Inertia, Doubled Kinetic Work & Right to Rest
 *   - SPEC/11 : Pairwise 3rd-Law Reciprocity & Strict Discrete Nash Equilibrium
 *   - SPEC/12 : Relativistic Lightcone, Airgap, Entropy Sink & Godel Witness Gate
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KOSMOS_V03_H
#define KOSMOS_V03_H

#include "kosmos_v02.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KOSMOS_GRID_N      4u
#define KOSMOS_GRID_CELLS  (KOSMOS_GRID_N * KOSMOS_GRID_N * KOSMOS_GRID_N)
#define KOSMOS_NASH_MAX_K  8u

/* --- SPEC/09: 3D Periodic Cellular Cochain Grids (4x4x4 = 64 cells) --- */
typedef struct {
    int64_t v[KOSMOS_GRID_CELLS];
} kosmos_scalar_grid_t;

typedef struct {
    int64_t x[KOSMOS_GRID_CELLS];
    int64_t y[KOSMOS_GRID_CELLS];
    int64_t z[KOSMOS_GRID_CELLS];
} kosmos_vec_grid_t;

kosmos_status_t kosmos_field_grad(const kosmos_scalar_grid_t *f,
                                  kosmos_vec_grid_t *grad_out);

kosmos_status_t kosmos_field_curl(const kosmos_vec_grid_t *edge,
                                  kosmos_vec_grid_t *curl_out);

kosmos_status_t kosmos_field_div(const kosmos_vec_grid_t *face,
                                 kosmos_scalar_grid_t *div_out);

kosmos_status_t kosmos_field_laplacian7(const kosmos_scalar_grid_t *f,
                                        kosmos_scalar_grid_t *lap_out);

kosmos_status_t kosmos_eval_faraday_lenz(int64_t dphi_b,
                                         int64_t emf_induced,
                                         int64_t emf_max,
                                         uint8_t *verdict_out);

/* --- SPEC/10: Newtonian Local Inertia & Right to Rest --- */
typedef struct {
    int64_t       mass[KOSMOS_DIM];
    int64_t       force_max[KOSMOS_DIM];
    kosmos_u128_t energy_rem;
    kosmos_u128_t w_min;
    kosmos_u128_t entropy_toll;
} kosmos_inertia_cfg_t;

kosmos_status_t kosmos_eval_newtonian_step(const kosmos_inertia_cfg_t *cfg,
                                           const kosmos_point3_t *q_prev2,
                                           const kosmos_point3_t *q_prev1,
                                           const kosmos_point3_t *q_curr,
                                           uint64_t dt_ticks,
                                           uint8_t *verdict_out);

/* --- SPEC/11: Pairwise Reciprocity & Strict Discrete Nash Equilibrium --- */
typedef struct {
    uint32_t k_channels;
    uint32_t has_pair_forces;
    int64_t  q[KOSMOS_NASH_MAX_K];
    int64_t  b_witness[KOSMOS_NASH_MAX_K];
    int64_t  c_matrix[KOSMOS_NASH_MAX_K * KOSMOS_NASH_MAX_K];
    int64_t  f_pair[KOSMOS_NASH_MAX_K * KOSMOS_NASH_MAX_K];
} kosmos_nash_system_t;

kosmos_status_t kosmos_eval_nash_equilibrium(const kosmos_nash_system_t *sys,
                                             uint8_t *verdict_out);

/* --- SPEC/12: Relativistic Lightcone, Airgap, Entropy & Godel Boundary --- */
kosmos_status_t kosmos_eval_lightcone_airgap(int64_t dx,
                                             int64_t dy,
                                             int64_t dz,
                                             uint64_t dt_ticks,
                                             uint64_t c_per_tick,
                                             uint8_t airgap_sealed,
                                             uint8_t ancestry_receipt_valid,
                                             uint64_t w_lift_work,
                                             uint8_t unmerged_fork,
                                             uint8_t *verdict_out);

kosmos_status_t kosmos_eval_entropy_ceiling(kosmos_u128_t s_dot_gen,
                                            kosmos_u128_t s_dot_sink,
                                            uint8_t *verdict_out);

kosmos_status_t kosmos_eval_godel_witness(uint64_t formal_iters,
                                          uint64_t max_iters,
                                          uint8_t undecidable_hit,
                                          uint8_t has_ext_witness,
                                          uint8_t *verdict_out);

/* Bit-exact software twin of RTL/kosmos_v03_physics_gate.sv combinational core */
typedef struct {
    uint8_t  rail_p;
    uint8_t  rail_n;
    uint8_t  airgap_sealed;
    uint8_t  ancestry_valid;
    uint8_t  unmerged_fork;
    uint8_t  undecidable_hit;
    uint8_t  has_ext_witness;
    uint8_t  pad0;
    uint32_t dt_ticks;
    uint32_t formal_iters;
    uint32_t max_iters;
    uint32_t pad1;
    int32_t  dphi_b;
    int32_t  emf_induced;
    int32_t  emf_max;
    int32_t  force_in;
    int32_t  force_max;
    int32_t  f_ab;
    int32_t  f_ba;
    int32_t  nash_res_abs_2x;
    int32_t  nash_c_ii;
    int32_t  pad2;
    uint64_t w_lift_work;
    uint64_t dr_sq;
    uint64_t ct_sq;
    uint64_t energy_rem_2x;
    uint64_t w_min_2x;
    uint64_t energy_cost_2x;
    uint64_t s_dot_gen;
    uint64_t s_dot_sink;
} kosmos_v03_rtl_vec_t;

uint8_t kosmos_eval_v03_rtl_twin(const kosmos_v03_rtl_vec_t *in);
size_t  kosmos_sizeof_v03_rtl_vec(void);

#ifdef __cplusplus
}
#endif

#endif /* KOSMOS_V03_H */
