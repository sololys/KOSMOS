/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * CORE/kosmos_space.h : Canonical types and C11 contracts (0 float, 0 heap)
 *
 * K = (E, B, omega, J, A, C_4, Omega)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KOSMOS_SPACE_H
#define KOSMOS_SPACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Spatial, control, guard, witness, and history dimensions */
#define KOSMOS_DIM            3u
#define KOSMOS_CTRL_DIM       3u
#define KOSMOS_GUARD_DIM      6u
#define KOSMOS_MAX_WITNESSES  8u
#define KOSMOS_ALPHABET_SIZE  8u
#define KOSMOS_HISTORY_CAP    256u

/* Symmetric phase lattice [-KOSMOS_PHASE_MAX, +KOSMOS_PHASE_MAX].
 * Two's-complement INT64_MIN (-2^63) has no positive partner and is fenced out
 * as KOSMOS_POISON_MIN so J(q, p) = (q, -p) is a strict, UB-free involution. */
#define KOSMOS_PHASE_MAX      INT64_MAX
#define KOSMOS_POISON_MIN     INT64_MIN

/* Hamming-separated verdict wire codes (pairwise distance >= 4) */
#define KOSMOS_VERDICT_KILL   ((uint8_t)0x00u)
#define KOSMOS_VERDICT_HOLD   ((uint8_t)0x3Cu)
#define KOSMOS_VERDICT_OPEN   ((uint8_t)0xA5u)

/* Sovereign Level-01 physical recovery token ("KOSMOS01") */
#define KOSMOS_L01_KEY_MAGIC  ((uint64_t)0x4B4F534D4F533031ULL)

/* Return status codes */
typedef enum {
    KOSMOS_OK             =  0,
    KOSMOS_ERR_NULL       = -1,
    KOSMOS_ERR_DOMAIN     = -2,
    KOSMOS_ERR_OVERFLOW   = -3,
    KOSMOS_ERR_LOCKED     = -4,
    KOSMOS_ERR_REPLAY     = -5,
    KOSMOS_ERR_CLOCK      = -6,
    KOSMOS_ERR_TETHER     = -7,
    KOSMOS_ERR_CAPACITY   = -8,
    KOSMOS_ERR_TAMPER     = -9,
    KOSMOS_ERR_AUTH       = -10
} kosmos_status_t;

/* Belnap-Dunn 4-valued Chu evaluation lattice FOUR = {?, 0, 1, CONFLICT} */
typedef enum {
    KOSMOS_CHU_UNKNOWN    = 0, /* 0b00 : insufficient evidence -> HOLD     */
    KOSMOS_CHU_REFUTED    = 1, /* 0b01 : candidate refuted     -> HOLD     */
    KOSMOS_CHU_VERIFIED   = 2, /* 0b10 : verified              -> OPEN-ok  */
    KOSMOS_CHU_CONFLICT   = 3  /* 0b11 : witness contradiction -> KILL     */
} kosmos_chu_val_t;

/* Discrete event alphabet A = {0..7} for B = A* */
typedef enum {
    KOSMOS_EVT_BOOT_HOLD          = 0,
    KOSMOS_EVT_MIRROR_INVOLUTION  = 1,
    KOSMOS_EVT_CANDIDATE_HOLD     = 2,
    KOSMOS_EVT_OPEN_AUTHORIZED    = 3,
    KOSMOS_EVT_POST_REALIZED      = 4,
    KOSMOS_EVT_OUTCOME_UNRESOLVED = 5,
    KOSMOS_EVT_HARD_KILL          = 6,
    KOSMOS_EVT_L01_RECOVERY       = 7
} kosmos_event_sym_t;

/* Internal Omega sluice lock state */
typedef enum {
    KOSMOS_STATE_HOLD       = 0,
    KOSMOS_STATE_IN_FLIGHT  = 1,
    KOSMOS_STATE_UNRESOLVED = 2,
    KOSMOS_STATE_KILL       = 3
} kosmos_lock_state_t;

/* 3D Affine base point q in Q (no addition of two points allowed) */
typedef struct {
    int64_t q[KOSMOS_DIM];
} kosmos_point3_t;

/* 3D Euclidean displacement vector dq = q2 - q1 in V */
typedef struct {
    int64_t dq[KOSMOS_DIM];
} kosmos_vec3_t;

/* 3D Cotangent momentum / tension fiber p in T*_q Q */
typedef struct {
    int64_t p[KOSMOS_DIM];
} kosmos_mom3_t;

/* 6D Symplectic phase state z = (q, p) in M = T*Q */
typedef struct {
    kosmos_point3_t q;
    kosmos_mom3_t   p;
} kosmos_phase_t;

/* Control actuation vector u in U = (Z_M^3, ||.||_inf) */
typedef struct {
    int64_t u[KOSMOS_CTRL_DIM];
} kosmos_ctrl_t;

/* Exact 192-bit two's-complement signed integer (little-endian 64-bit limbs)
 * Holds exact symplectic 2-form omega(dz1, dz2) across all [-M, +M]^6 inputs
 * with zero overflow and zero compiler extensions. */
typedef struct {
    uint64_t limb[3];
} kosmos_i192_t;

/* Division-free L-infinity box A(r, tau) : |x_i - r_i| <= tau_i, tau_i > 0 */
typedef struct {
    uint32_t count;
    uint32_t reserved;
    int64_t  ref[KOSMOS_GUARD_DIM];
    int64_t  tol[KOSMOS_GUARD_DIM];
} kosmos_linf_box_t;

/* Pre-realization witness package W_PRE */
typedef struct {
    uint64_t witness_id;
    uint64_t auth_nonce;
    uint64_t cycle;
    int64_t  seam_defect;
    uint32_t witness_count;
    uint8_t  chu_rows[KOSMOS_MAX_WITNESSES];
    uint8_t  pre_hash[32];
    uint8_t  pad[4];
} kosmos_witness_pre_t;

/* Post-realization tethered witness package W_POST */
typedef struct {
    uint64_t witness_id;
    uint64_t cycle;
    uint8_t  tether_pre_hash[32];
    uint8_t  outcome_ok;
    uint8_t  pad[7];
} kosmos_witness_post_t;

/* Single sealed entry in append-only prefix history B = A* */
typedef struct {
    uint64_t index;
    uint64_t cycle;
    uint64_t witness_id;
    uint8_t  symbol;
    uint8_t  verdict;
    uint8_t  pad[6];
    uint8_t  prev_hash[32];
    uint8_t  payload_hash[32];
    uint8_t  entry_hash[32];
} kosmos_ledger_entry_t;

/* Append-only discrete history B = A* */
typedef struct {
    uint32_t              len;
    uint32_t              reserved;
    uint8_t               symbols[KOSMOS_HISTORY_CAP];
    uint8_t               head_hash[32];
    kosmos_ledger_entry_t entries[KOSMOS_HISTORY_CAP];
} kosmos_history_t;

/* Unified Kosmos Omega Gate instance */
typedef struct {
    uint32_t          state;
    uint32_t          min_witnesses;
    uint64_t          last_cycle;
    uint64_t          last_nonce;
    uint64_t          open_cycle;
    uint64_t          realized_count;
    uint8_t           pending_pre_hash[32];
    kosmos_linf_box_t hard_box;
    kosmos_linf_box_t cand_box;
    kosmos_ctrl_t     active_u;
    kosmos_history_t  history;
} kosmos_gate_t;

/* --- Mirror & Symplectic Body (kosmos_mirror.c) --- */
int             kosmos_i64_valid(int64_t v);
int             kosmos_phase_valid(const kosmos_phase_t *z);
int             kosmos_ctrl_valid(const kosmos_ctrl_t *u);
kosmos_status_t kosmos_affine_diff(const kosmos_point3_t *q2,
                                   const kosmos_point3_t *q1,
                                   kosmos_vec3_t *dq_out);
kosmos_status_t kosmos_affine_translate(const kosmos_point3_t *q,
                                        const kosmos_vec3_t *dq,
                                        kosmos_point3_t *q_out);
kosmos_status_t kosmos_mirror_apply(const kosmos_phase_t *in,
                                    kosmos_phase_t *out);
kosmos_status_t kosmos_symplectic_form(const kosmos_vec3_t *dq1,
                                       const kosmos_mom3_t *dp1,
                                       const kosmos_vec3_t *dq2,
                                       const kosmos_mom3_t *dp2,
                                       kosmos_i192_t *omega_out);
void            kosmos_i192_neg(const kosmos_i192_t *in, kosmos_i192_t *out);
int             kosmos_i192_eq(const kosmos_i192_t *a, const kosmos_i192_t *b);
int             kosmos_i192_is_zero(const kosmos_i192_t *a);

/* --- Boundary L-infinity Envelopes (kosmos_boundary.c) --- */
kosmos_status_t kosmos_box_validate(const kosmos_linf_box_t *box);
kosmos_status_t kosmos_box_check(const kosmos_linf_box_t *box,
                                 const int64_t *values,
                                 uint32_t value_count,
                                 uint8_t *inside_out,
                                 uint32_t *breach_index_out);

/* --- Discrete History & Cantor Cylinders (kosmos_history.c) --- */
kosmos_status_t kosmos_history_init(kosmos_history_t *hist, uint64_t boot_cycle);
kosmos_status_t kosmos_history_append(kosmos_history_t *hist,
                                      uint64_t cycle,
                                      uint8_t symbol,
                                      uint8_t verdict,
                                      uint64_t witness_id,
                                      const uint8_t payload_hash[32]);
kosmos_status_t kosmos_history_verify_chain(const kosmos_history_t *hist);
int             kosmos_prefix_leq(const uint8_t *prefix,
                                  size_t prefix_len,
                                  const uint8_t *seq,
                                  size_t seq_len);
size_t          kosmos_cantor_common_prefix(const uint8_t *s,
                                            size_t s_len,
                                            const uint8_t *t,
                                            size_t t_len);
int             kosmos_cantor_closer(const uint8_t *ref_seq,
                                     size_t ref_len,
                                     const uint8_t *cand_a,
                                     size_t a_len,
                                     const uint8_t *cand_b,
                                     size_t b_len);
int             kosmos_cantor_in_cylinder(const kosmos_history_t *hist,
                                          const uint8_t *future_seq,
                                          size_t seq_len);

/* --- Chu Witness & Omega Gate (kosmos_gate.c) --- */
kosmos_chu_val_t kosmos_chu_evaluate(const uint8_t *rows,
                                     uint32_t count,
                                     uint32_t min_verified);
kosmos_status_t  kosmos_gate_init(kosmos_gate_t *gate,
                                  const kosmos_linf_box_t *hard_box,
                                  const kosmos_linf_box_t *cand_box,
                                  uint32_t min_witnesses,
                                  uint64_t boot_cycle);
kosmos_status_t  kosmos_gate_mirror_chamber(kosmos_gate_t *gate,
                                            kosmos_phase_t *candidate_inout);
kosmos_status_t  kosmos_gate_step_hold(kosmos_gate_t *gate,
                                       const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                       const kosmos_phase_t *cand_phase,
                                       const kosmos_ctrl_t *cand_ctrl,
                                       uint64_t cycle,
                                       kosmos_ctrl_t *u_out,
                                       uint8_t *verdict_out);
kosmos_status_t  kosmos_gate_authorize(kosmos_gate_t *gate,
                                       const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                       const kosmos_phase_t *cand_phase,
                                       const kosmos_ctrl_t *cand_ctrl,
                                       kosmos_witness_pre_t *w_pre,
                                       kosmos_ctrl_t *u_out,
                                       uint8_t *verdict_out);
kosmos_status_t  kosmos_gate_confirm_post(kosmos_gate_t *gate,
                                          const kosmos_witness_post_t *w_post,
                                          uint8_t *verdict_out);
kosmos_status_t  kosmos_gate_power_loss(kosmos_gate_t *gate,
                                        uint64_t recovery_boot_cycle,
                                        uint8_t *verdict_out);
kosmos_status_t  kosmos_gate_l01_recover(kosmos_gate_t *gate,
                                         uint64_t l01_key,
                                         uint64_t cycle,
                                         uint8_t *verdict_out);

size_t           kosmos_sizeof_gate(void);
size_t           kosmos_sizeof_history(void);

#ifdef __cplusplus
}
#endif

#endif /* KOSMOS_SPACE_H */
