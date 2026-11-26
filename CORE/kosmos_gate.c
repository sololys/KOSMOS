/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * CORE/kosmos_gate.c : Belnap-Dunn Chu Bench and Two-Stroke Omega Sluice
 *
 * Enforces:
 *   - Candidate != Consequence (pi o gamma = const inside HOLD)
 *   - OPEN != REALIZED (tethered W_PRE -> IN_FLIGHT -> W_POST)
 *   - Power-cut / missing W_POST -> UNRESOLVED_LOCK (0 mV)
 *   - One-way actuation: u_out != 0 => OPEN && inside A && Chu == VERIFIED
 *   - Subsumption: KILL > HOLD > OPEN
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_space.h"
#include "kosmos_sha256.h"
#include <string.h>

static void put_be64(uint8_t out[8], uint64_t v) {
    uint32_t i;
    for (i = 0u; i < 8u; ++i) {
        uint32_t shift = (7u - i) * 8u;
        out[i] = (uint8_t)((v >> shift) & 0xFFu);
    }
}

static void zero_ctrl(kosmos_ctrl_t *u) {
    if (u != NULL) {
        memset(u, 0, sizeof(*u));
    }
}

static void seal_pre_authorization(const kosmos_gate_t *gate,
                                   const kosmos_phase_t *cand_phase,
                                   const kosmos_ctrl_t *cand_ctrl,
                                   const kosmos_witness_pre_t *w_pre,
                                   uint8_t digest[32]) {
    kosmos_sha256_ctx_t ctx;
    uint8_t buf[8];
    uint32_t i;

    kosmos_sha256_init(&ctx);
    kosmos_sha256_update(&ctx, gate->history.head_hash, 32u);
    put_be64(buf, w_pre->witness_id);
    kosmos_sha256_update(&ctx, buf, 8u);
    put_be64(buf, w_pre->auth_nonce);
    kosmos_sha256_update(&ctx, buf, 8u);
    put_be64(buf, w_pre->cycle);
    kosmos_sha256_update(&ctx, buf, 8u);

    for (i = 0u; i < KOSMOS_DIM; ++i) {
        put_be64(buf, (uint64_t)cand_phase->q.q[i]);
        kosmos_sha256_update(&ctx, buf, 8u);
        put_be64(buf, (uint64_t)cand_phase->p.p[i]);
        kosmos_sha256_update(&ctx, buf, 8u);
    }
    for (i = 0u; i < KOSMOS_CTRL_DIM; ++i) {
        put_be64(buf, (uint64_t)cand_ctrl->u[i]);
        kosmos_sha256_update(&ctx, buf, 8u);
    }
    kosmos_sha256_final(&ctx, digest);
}

static kosmos_status_t latch_kill(kosmos_gate_t *gate,
                                  uint64_t cycle,
                                  uint64_t witness_id,
                                  uint8_t reason_tag) {
    uint8_t payload[32];
    uint64_t eff_cycle = (cycle >= gate->last_cycle) ? cycle : gate->last_cycle;

    /* If an actuation was IN_FLIGHT when KILL struck, preserve UNRESOLVED latch and pre_hash */
    if (gate->state == (uint32_t)KOSMOS_STATE_IN_FLIGHT ||
        gate->state == (uint32_t)KOSMOS_STATE_UNRESOLVED) {
        gate->unresolved_latched = 1u;
    }

    memset(payload, 0, sizeof(payload));
    payload[0] = reason_tag;
    zero_ctrl(&gate->active_u);
    if (gate->unresolved_latched == 0u) {
        memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));
    }
    gate->state = (uint32_t)KOSMOS_STATE_KILL;
    gate->last_cycle = eff_cycle;

    return kosmos_history_append(&gate->history,
                                 eff_cycle,
                                 (uint8_t)KOSMOS_EVT_HARD_KILL,
                                 KOSMOS_VERDICT_KILL,
                                 witness_id,
                                 payload);
}

kosmos_chu_val_t kosmos_chu_evaluate(const uint8_t *rows,
                                     uint32_t count,
                                     uint32_t min_verified) {
    uint8_t acc = 0u;
    uint32_t verified_cnt = 0u;
    uint32_t i;

    if (min_verified == 0u || count > KOSMOS_MAX_WITNESSES) {
        return KOSMOS_CHU_CONFLICT;
    }
    if (count == 0u || rows == NULL) {
        return KOSMOS_CHU_UNKNOWN;
    }

    for (i = 0u; i < count; ++i) {
        uint8_t v = rows[i];
        if (v > (uint8_t)KOSMOS_CHU_CONFLICT) {
            return KOSMOS_CHU_CONFLICT;
        }
        acc = (uint8_t)(acc | v);
        if (v == (uint8_t)KOSMOS_CHU_VERIFIED) {
            verified_cnt += 1u;
        }
    }

    if (acc == (uint8_t)KOSMOS_CHU_CONFLICT) {
        return KOSMOS_CHU_CONFLICT;
    }
    if ((acc & (uint8_t)KOSMOS_CHU_REFUTED) != 0u) {
        return KOSMOS_CHU_REFUTED;
    }
    if (acc == (uint8_t)KOSMOS_CHU_VERIFIED && verified_cnt >= min_verified) {
        return KOSMOS_CHU_VERIFIED;
    }
    return KOSMOS_CHU_UNKNOWN;
}

kosmos_status_t kosmos_gate_init(kosmos_gate_t *gate,
                                 const kosmos_linf_box_t *hard_box,
                                 const kosmos_linf_box_t *cand_box,
                                 uint32_t min_witnesses,
                                 uint64_t boot_cycle) {
    kosmos_status_t st;

    if (gate == NULL || hard_box == NULL || cand_box == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (min_witnesses == 0u || min_witnesses > KOSMOS_MAX_WITNESSES) {
        return KOSMOS_ERR_DOMAIN;
    }
    st = kosmos_box_validate(hard_box);
    if (st != KOSMOS_OK) {
        return st;
    }
    st = kosmos_box_validate(cand_box);
    if (st != KOSMOS_OK) {
        return st;
    }

    memset(gate, 0, sizeof(*gate));
    gate->state = (uint32_t)KOSMOS_STATE_HOLD;
    gate->min_witnesses = min_witnesses;
    gate->last_cycle = boot_cycle;
    gate->last_nonce = 0u;
    gate->open_cycle = 0u;
    gate->realized_count = 0u;
    gate->hard_box = *hard_box;
    gate->cand_box = *cand_box;

    return kosmos_history_init(&gate->history, boot_cycle);
}

kosmos_status_t kosmos_gate_mirror_chamber(kosmos_gate_t *gate,
                                           kosmos_phase_t *candidate_inout) {
    kosmos_status_t st;
    if (gate == NULL || candidate_inout == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (gate->state != (uint32_t)KOSMOS_STATE_HOLD) {
        return KOSMOS_ERR_LOCKED;
    }
    st = kosmos_mirror_apply(candidate_inout, candidate_inout);
    if (st != KOSMOS_OK) {
        (void)latch_kill(gate, gate->last_cycle, 0u, 0xE1u);
        return st;
    }
    /* Inside HOLD, continuous/involution transforms preserve historical anchor h */
    return KOSMOS_OK;
}

static void pack_cand_vector(const kosmos_phase_t *z,
                             const kosmos_ctrl_t *u,
                             int64_t out[KOSMOS_GUARD_DIM]) {
    /* Maps 3D position + 3D control magnitude/channels or 6D phase:
     * Channels 0..2 = q[0..2], Channels 3..5 = u[0..2] */
    out[0] = z->q.q[0];
    out[1] = z->q.q[1];
    out[2] = z->q.q[2];
    out[3] = u->u[0];
    out[4] = u->u[1];
    out[5] = u->u[2];
}

kosmos_status_t kosmos_gate_step_hold(kosmos_gate_t *gate,
                                      const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                      const kosmos_phase_t *cand_phase,
                                      const kosmos_ctrl_t *cand_ctrl,
                                      uint64_t cycle,
                                      kosmos_ctrl_t *u_out,
                                      uint8_t *verdict_out) {
    uint8_t hard_ok = 0u;
    kosmos_status_t st;

    zero_ctrl(u_out);
    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
    }
    if (gate == NULL || hard_meas == NULL || cand_phase == NULL ||
        cand_ctrl == NULL || u_out == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (gate->state == (uint32_t)KOSMOS_STATE_KILL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_LOCKED;
    }
    if (cycle <= gate->last_cycle) {
        (void)latch_kill(gate, gate->last_cycle, 0u, 0xE2u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_CLOCK;
    }
    gate->last_cycle = cycle;

    st = kosmos_box_check(&gate->hard_box, hard_meas, KOSMOS_GUARD_DIM, &hard_ok, NULL);
    if (st != KOSMOS_OK || hard_ok == 0u ||
        !kosmos_phase_valid(cand_phase) || !kosmos_ctrl_valid(cand_ctrl)) {
        (void)latch_kill(gate, cycle, 0u, 0xE3u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return (st != KOSMOS_OK) ? st : KOSMOS_OK;
    }

    if (gate->state != (uint32_t)KOSMOS_STATE_HOLD) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_LOCKED;
    }

    /* Even when inside hard_box and cand_box, elapsed clock cycles NEVER open HOLD */
    *verdict_out = KOSMOS_VERDICT_HOLD;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_gate_authorize(kosmos_gate_t *gate,
                                      const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                      const kosmos_phase_t *cand_phase,
                                      const kosmos_ctrl_t *cand_ctrl,
                                      kosmos_witness_pre_t *w_pre,
                                      kosmos_ctrl_t *u_out,
                                      uint8_t *verdict_out) {
    uint8_t hard_ok = 0u;
    uint8_t cand_ok = 0u;
    int64_t cand_vec[KOSMOS_GUARD_DIM];
    kosmos_chu_val_t chu;
    kosmos_status_t st;

    zero_ctrl(u_out);
    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
    }
    if (gate == NULL || hard_meas == NULL || cand_phase == NULL ||
        cand_ctrl == NULL || w_pre == NULL || u_out == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    if (gate->state == (uint32_t)KOSMOS_STATE_KILL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_LOCKED;
    }
    if (gate->state == (uint32_t)KOSMOS_STATE_IN_FLIGHT ||
        gate->state == (uint32_t)KOSMOS_STATE_UNRESOLVED) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_LOCKED;
    }

    if (w_pre->cycle <= gate->last_cycle) {
        (void)latch_kill(gate, gate->last_cycle, w_pre->witness_id, 0xE4u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_CLOCK;
    }
    gate->last_cycle = w_pre->cycle;

    /* 1. Hard plant L-infinity safety wall & symmetric integer validity */
    st = kosmos_box_check(&gate->hard_box, hard_meas, KOSMOS_GUARD_DIM, &hard_ok, NULL);
    if (st != KOSMOS_OK || hard_ok == 0u ||
        !kosmos_phase_valid(cand_phase) || !kosmos_ctrl_valid(cand_ctrl) ||
        !kosmos_i64_valid(w_pre->seam_defect)) {
        (void)latch_kill(gate, w_pre->cycle, w_pre->witness_id, 0xE5u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return (st != KOSMOS_OK) ? st : KOSMOS_OK;
    }

    /* 2. Loop-space holonomy seam check: non-closing loop triggers KILL */
    if (w_pre->seam_defect != 0) {
        (void)latch_kill(gate, w_pre->cycle, w_pre->witness_id, 0xE6u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_OK;
    }

    /* 3. Anti-replay nonce check (reused or regressed authorization -> KILL) */
    if (w_pre->auth_nonce <= gate->last_nonce) {
        (void)latch_kill(gate, w_pre->cycle, w_pre->witness_id, 0xE7u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_REPLAY;
    }

    /* 4. Belnap-Dunn 4-valued Chu bench */
    chu = kosmos_chu_evaluate(w_pre->chu_rows, w_pre->witness_count, gate->min_witnesses);
    if (chu == KOSMOS_CHU_CONFLICT) {
        (void)latch_kill(gate, w_pre->cycle, w_pre->witness_id, 0xE8u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_OK;
    }
    if (w_pre->witness_id == 0u || chu != KOSMOS_CHU_VERIFIED) {
        /* Insufficient or refuted candidate witness stays safely in HOLD (0 mV) */
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }

    /* 5. Candidate envelope check (out-of-bounds proposal rejected into HOLD, not KILL) */
    pack_cand_vector(cand_phase, cand_ctrl, cand_vec);
    st = kosmos_box_check(&gate->cand_box, cand_vec, KOSMOS_GUARD_DIM, &cand_ok, NULL);
    if (st != KOSMOS_OK) {
        (void)latch_kill(gate, w_pre->cycle, w_pre->witness_id, 0xE9u);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return st;
    }
    if (cand_ok == 0u) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }

    /* 6. Stroke 1 passed: seal W_PRE, enter IN_FLIGHT, release u_out (OPEN != REALIZED) */
    gate->last_nonce = w_pre->auth_nonce;
    gate->open_cycle = w_pre->cycle;
    seal_pre_authorization(gate, cand_phase, cand_ctrl, w_pre, gate->pending_pre_hash);
    memcpy(w_pre->pre_hash, gate->pending_pre_hash, 32u);

    st = kosmos_history_append(&gate->history,
                               w_pre->cycle,
                               (uint8_t)KOSMOS_EVT_OPEN_AUTHORIZED,
                               KOSMOS_VERDICT_OPEN,
                               w_pre->witness_id,
                               gate->pending_pre_hash);
    if (st != KOSMOS_OK) {
        (void)latch_kill(gate, w_pre->cycle, w_pre->witness_id, 0xEAu);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return st;
    }

    gate->state = (uint32_t)KOSMOS_STATE_IN_FLIGHT;
    gate->active_u = *cand_ctrl;
    *u_out = *cand_ctrl;
    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_gate_confirm_post(kosmos_gate_t *gate,
                                         const kosmos_witness_post_t *w_post,
                                         uint8_t *verdict_out) {
    kosmos_status_t st;

    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_HOLD;
    }
    if (gate == NULL || w_post == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (gate->state == (uint32_t)KOSMOS_STATE_KILL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_LOCKED;
    }
    if (gate->state != (uint32_t)KOSMOS_STATE_IN_FLIGHT) {
        return KOSMOS_ERR_LOCKED;
    }

    zero_ctrl(&gate->active_u);

    if (w_post->cycle < gate->open_cycle || w_post->cycle < gate->last_cycle) {
        (void)latch_kill(gate, gate->last_cycle, w_post->witness_id, 0xEBu);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_CLOCK;
    }
    gate->last_cycle = w_post->cycle;

    if (w_post->witness_id == 0u ||
        memcmp(w_post->tether_pre_hash, gate->pending_pre_hash, 32u) != 0 ||
        w_post->outcome_ok != 1u) {
        /* Untethered or failed POST locks gate into UNRESOLVED (never erases attempt) */
        gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
        gate->unresolved_latched = 1u;
        (void)kosmos_history_append(&gate->history,
                                    w_post->cycle,
                                    (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED,
                                    KOSMOS_VERDICT_HOLD,
                                    w_post->witness_id,
                                    gate->pending_pre_hash);
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_TETHER;
    }

    st = kosmos_history_append(&gate->history,
                               w_post->cycle,
                               (uint8_t)KOSMOS_EVT_POST_REALIZED,
                               KOSMOS_VERDICT_HOLD,
                               w_post->witness_id,
                               gate->pending_pre_hash);
    memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));
    gate->unresolved_latched = 0u;
    if (st != KOSMOS_OK) {
        (void)latch_kill(gate, w_post->cycle, w_post->witness_id, 0xECu);
        *verdict_out = KOSMOS_VERDICT_KILL;
        return st;
    }

    gate->realized_count += 1u;
    gate->state = (uint32_t)KOSMOS_STATE_HOLD;
    *verdict_out = KOSMOS_VERDICT_HOLD;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_gate_power_loss(kosmos_gate_t *gate,
                                       uint64_t recovery_boot_cycle,
                                       uint8_t *verdict_out) {
    uint64_t eff_cycle;
    if (gate == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    zero_ctrl(&gate->active_u);
    eff_cycle = (recovery_boot_cycle > gate->last_cycle)
                    ? recovery_boot_cycle
                    : (gate->last_cycle + 1u);
    gate->last_cycle = eff_cycle;

    if (gate->state == (uint32_t)KOSMOS_STATE_KILL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_OK;
    }
    if (gate->state == (uint32_t)KOSMOS_STATE_IN_FLIGHT) {
        gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
        gate->unresolved_latched = 1u;
        (void)kosmos_history_append(&gate->history,
                                    eff_cycle,
                                    (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED,
                                    KOSMOS_VERDICT_HOLD,
                                    0u,
                                    gate->pending_pre_hash);
    }
    *verdict_out = KOSMOS_VERDICT_HOLD;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_gate_l01_recover(kosmos_gate_t *gate,
                                        uint64_t l01_key,
                                        uint64_t cycle,
                                        uint8_t *verdict_out) {
    uint8_t payload[32];
    kosmos_status_t st;

    if (gate == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (l01_key != KOSMOS_L01_KEY_MAGIC) {
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_AUTH;
    }
    if (cycle <= gate->last_cycle) {
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_CLOCK;
    }

    zero_ctrl(&gate->active_u);
    memset(payload, 0, sizeof(payload));
    put_be64(payload, l01_key);

    st = kosmos_history_append(&gate->history,
                               cycle,
                               (uint8_t)KOSMOS_EVT_L01_RECOVERY,
                               KOSMOS_VERDICT_HOLD,
                               l01_key,
                               payload);
    if (st != KOSMOS_OK) {
        *verdict_out = KOSMOS_VERDICT_KILL;
        return st;
    }

    gate->last_cycle = cycle;
    if (gate->state == (uint32_t)KOSMOS_STATE_KILL && gate->unresolved_latched != 0u) {
        /* Clearing KILL when an earlier PRE was unresolved steps down to UNRESOLVED, never straight to HOLD */
        gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
    } else {
        gate->unresolved_latched = 0u;
        memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));
        gate->state = (uint32_t)KOSMOS_STATE_HOLD;
    }
    *verdict_out = KOSMOS_VERDICT_HOLD;
    return KOSMOS_OK;
}

size_t kosmos_sizeof_gate(void) {
    return sizeof(kosmos_gate_t);
}

size_t kosmos_sizeof_history(void) {
    return sizeof(kosmos_history_t);
}
