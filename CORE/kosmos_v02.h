/*
 * KOSMOS v0.2 — Cold-Boot WAL, Signed L01, 1024-Channel Graph, Alexandrov ToF,
 * and 128-Bit Corner & Logical Landauer Guard (0 float, 0 heap)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KOSMOS_V02_H
#define KOSMOS_V02_H

#include "kosmos_space.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KOSMOS_CHANNEL_COUNT   1024u
#define KOSMOS_MAX_DEGREE      4u
#define KOSMOS_VERDICT_TRAP    ((uint8_t)0x5Au) /* Pairwise Hamming distance >= 4 from 0x00, 0x3C, 0xA5 */
#define KOSMOS_WAL_RECORD_SIZE 128u

/* Portable 128-bit unsigned integer (little-endian 64-bit limbs) */
typedef struct {
    uint64_t lo;
    uint64_t hi;
} kosmos_u128_t;

/* --- 1. Cold-Boot WAL & HMAC-SHA256 Signed L01 Recovery (kosmos_wal.c) --- */
void kosmos_hmac_sha256(const uint8_t *key,
                        size_t key_len,
                        const uint8_t *msg,
                        size_t msg_len,
                        uint8_t mac_out[32]);

void kosmos_l01_sign_challenge(const uint8_t key32[32],
                               const uint8_t head_hash[32],
                               uint64_t cycle,
                               uint32_t gate_state,
                               uint8_t sig_out[32]);

kosmos_status_t kosmos_gate_cold_boot(kosmos_gate_t *gate,
                                      const char *wal_path,
                                      const kosmos_linf_box_t *hard_box,
                                      const kosmos_linf_box_t *cand_box,
                                      uint32_t min_witnesses,
                                      uint64_t boot_cycle,
                                      uint8_t *had_unresolved_pre_out,
                                      kosmos_ledger_entry_t *recovered_pre_out);

kosmos_status_t kosmos_wal_authorize(kosmos_gate_t *gate,
                                     const char *wal_path,
                                     const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                     const kosmos_phase_t *cand_phase,
                                     const kosmos_ctrl_t *cand_ctrl,
                                     kosmos_witness_pre_t *w_pre,
                                     kosmos_ctrl_t *u_out,
                                     uint8_t *verdict_out);

kosmos_status_t kosmos_wal_confirm_post(kosmos_gate_t *gate,
                                        const char *wal_path,
                                        const kosmos_witness_post_t *w_post,
                                        uint8_t *verdict_out);

kosmos_status_t kosmos_wal_l01_recover_signed(kosmos_gate_t *gate,
                                              const char *wal_path,
                                              const uint8_t key32[32],
                                              const uint8_t sig32[32],
                                              uint64_t cycle,
                                              uint8_t *verdict_out);

/* --- 2. Locally Finite 1024-Channel Sluice (kosmos_channel_sluice.c) --- */
typedef struct {
    uint8_t  verdict;
    uint8_t  degree;
    uint16_t neighbors[KOSMOS_MAX_DEGREE];
    uint64_t last_nonce;
    int64_t  out_u;
    uint32_t trap_count;
    uint32_t open_count;
} kosmos_channel_slot_t;

typedef struct {
    uint8_t               global_kill;
    uint8_t               pad[7];
    int64_t               hard_limit;
    int64_t               cand_limit;
    kosmos_channel_slot_t slots[KOSMOS_CHANNEL_COUNT];
} kosmos_channel_bank_t;

kosmos_status_t kosmos_channel_bank_init(kosmos_channel_bank_t *bank,
                                         int64_t hard_limit,
                                         int64_t cand_limit);

kosmos_status_t kosmos_channel_set_neighbors(kosmos_channel_bank_t *bank,
                                             uint32_t channel_id,
                                             const uint16_t *neighbors,
                                             uint32_t degree);

kosmos_status_t kosmos_channel_step(kosmos_channel_bank_t *bank,
                                    uint32_t channel_id,
                                    int64_t hard_meas,
                                    int64_t cand_u,
                                    uint64_t auth_nonce,
                                    uint8_t chu_val,
                                    uint8_t local_anomaly,
                                    int64_t *u_out,
                                    uint8_t *verdict_out);

size_t kosmos_sizeof_channel_bank(void);

/* --- 3. Alexandrov Picosecond ToF Window (kosmos_tdc_window.c) --- */
typedef struct {
    uint64_t tau_min_ps;
    uint64_t tau_max_ps;
    uint64_t cal_uncert_ps;
} kosmos_tdc_window_t;

kosmos_status_t kosmos_tdc_validate(const kosmos_tdc_window_t *win);
kosmos_status_t kosmos_tdc_check(const kosmos_tdc_window_t *win,
                                 uint64_t meas_tof_ps,
                                 uint8_t *inside_out);
kosmos_status_t kosmos_tdc_gated_authorize(kosmos_gate_t *gate,
                                           const kosmos_tdc_window_t *win,
                                           uint64_t meas_tof_ps,
                                           const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                           const kosmos_phase_t *cand_phase,
                                           const kosmos_ctrl_t *cand_ctrl,
                                           kosmos_witness_pre_t *w_pre,
                                           kosmos_ctrl_t *u_out,
                                           uint8_t *verdict_out);

/* --- 4. 128-Bit Corner Guard & Logical Landauer Ledger (kosmos_energy_128.c) --- */
typedef struct {
    int64_t       ref[KOSMOS_DIM];
    uint64_t      weight[KOSMOS_DIM];
    kosmos_u128_t max_energy_sq;
} kosmos_corner_guard_t;

typedef struct {
    uint64_t      pruned_branches;
    uint64_t      erased_bits_min;
    uint64_t      ebit_zepto_joule_milli; /* e.g., 2871 => 2.871 zJ/bit at 300 K (logical lower bound) */
    kosmos_u128_t logical_min_milli_zj;
    kosmos_u128_t budget_milli_zj;
} kosmos_landauer_ledger_t;

kosmos_status_t kosmos_corner_guard_eval(const kosmos_corner_guard_t *guard,
                                         const int64_t vec[KOSMOS_DIM],
                                         uint8_t *inside_out,
                                         kosmos_u128_t *energy_sq_out);

kosmos_status_t kosmos_landauer_init(kosmos_landauer_ledger_t *led,
                                     uint64_t ebit_milli_zj,
                                     kosmos_u128_t budget_milli_zj);

kosmos_status_t kosmos_landauer_record_prune(kosmos_landauer_ledger_t *led,
                                             uint64_t discarded_branches,
                                             uint8_t *within_budget_out);

uint32_t kosmos_ceil_log2_u64(uint64_t n);

#ifdef __cplusplus
}
#endif

#endif /* KOSMOS_V02_H */
