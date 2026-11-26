/*
 * KOSMOS v0.2 — Locally Finite 1024-Channel Graph & O(1) TRAP Quarantine
 * CORE/kosmos_channel_sluice.c (0 float, 0 heap)
 *
 * Enforces deg(k) <= 4 and per-channel TRAP (0x5A) quarantine so a fault in
 * channel k isolates k at 0 mV without perturbing neighbors or non-neighbors,
 * while a global plant/conflict breach trips global_kill across all channels.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_v02.h"
#include <string.h>

static uint64_t ordered_u64(int64_t v) {
    return ((uint64_t)v) ^ ((uint64_t)1ULL << 63u);
}

static uint64_t abs_i64_exact(int64_t v) {
    uint64_t uv = ordered_u64(v);
    uint64_t uz = ordered_u64(0);
    return (uv >= uz) ? (uv - uz) : (uz - uv);
}

kosmos_status_t kosmos_channel_bank_init(kosmos_channel_bank_t *bank,
                                         int64_t hard_limit,
                                         int64_t cand_limit) {
    uint32_t k;
    if (bank == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (!kosmos_i64_valid(hard_limit) || !kosmos_i64_valid(cand_limit) ||
        hard_limit <= 0 || cand_limit <= 0) {
        return KOSMOS_ERR_DOMAIN;
    }

    memset(bank, 0, sizeof(*bank));
    bank->hard_limit = hard_limit;
    bank->cand_limit = cand_limit;
    for (k = 0u; k < KOSMOS_CHANNEL_COUNT; ++k) {
        bank->slots[k].verdict = KOSMOS_VERDICT_HOLD;
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_channel_set_neighbors(kosmos_channel_bank_t *bank,
                                             uint32_t channel_id,
                                             const uint16_t *neighbors,
                                             uint32_t degree) {
    uint32_t i;
    uint32_t j;

    if (bank == NULL || (neighbors == NULL && degree > 0u)) {
        return KOSMOS_ERR_NULL;
    }
    if (channel_id >= KOSMOS_CHANNEL_COUNT || degree > KOSMOS_MAX_DEGREE) {
        return KOSMOS_ERR_DOMAIN;
    }

    for (i = 0u; i < degree; ++i) {
        if (neighbors[i] >= KOSMOS_CHANNEL_COUNT || neighbors[i] == (uint16_t)channel_id) {
            return KOSMOS_ERR_DOMAIN;
        }
        for (j = i + 1u; j < degree; ++j) {
            if (neighbors[i] == neighbors[j]) {
                return KOSMOS_ERR_DOMAIN;
            }
        }
    }

    bank->slots[channel_id].degree = (uint8_t)degree;
    for (i = 0u; i < KOSMOS_MAX_DEGREE; ++i) {
        bank->slots[channel_id].neighbors[i] = (i < degree) ? neighbors[i] : 0u;
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_channel_step(kosmos_channel_bank_t *bank,
                                    uint32_t channel_id,
                                    int64_t hard_meas,
                                    int64_t cand_u,
                                    uint64_t auth_nonce,
                                    uint8_t chu_val,
                                    uint8_t local_anomaly,
                                    int64_t *u_out,
                                    uint8_t *verdict_out) {
    kosmos_channel_slot_t *slot;

    if (u_out != NULL) {
        *u_out = 0;
    }
    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
    }
    if (bank == NULL || u_out == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    /* Out-of-bounds channel ID trips sticky global KILL */
    if (channel_id >= KOSMOS_CHANNEL_COUNT) {
        bank->global_kill = 1u;
        return KOSMOS_ERR_DOMAIN;
    }
    if (bank->global_kill != 0u) {
        bank->slots[channel_id].out_u = 0;
        bank->slots[channel_id].verdict = KOSMOS_VERDICT_KILL;
        return KOSMOS_ERR_LOCKED;
    }

    slot = &bank->slots[channel_id];

    /* Global plant safety wall or witness contradiction -> global KILL */
    if (!kosmos_i64_valid(hard_meas) || !kosmos_i64_valid(cand_u) ||
        abs_i64_exact(hard_meas) > (uint64_t)bank->hard_limit ||
        chu_val >= (uint8_t)KOSMOS_CHU_CONFLICT) {
        bank->global_kill = 1u;
        slot->out_u = 0;
        slot->verdict = KOSMOS_VERDICT_KILL;
        *verdict_out = KOSMOS_VERDICT_KILL;
        return KOSMOS_OK;
    }

    /* Sticky per-channel TRAP quarantine: isolated to this channel alone */
    if (slot->verdict == KOSMOS_VERDICT_TRAP) {
        slot->out_u = 0;
        *verdict_out = KOSMOS_VERDICT_TRAP;
        return KOSMOS_ERR_LOCKED;
    }

    /* Local anomaly, local replay, or local candidate breach -> quarantine ONLY this channel */
    if (local_anomaly != 0u ||
        auth_nonce <= slot->last_nonce ||
        abs_i64_exact(cand_u) > (uint64_t)bank->cand_limit) {
        slot->verdict = KOSMOS_VERDICT_TRAP;
        slot->out_u = 0;
        slot->trap_count += 1u;
        *verdict_out = KOSMOS_VERDICT_TRAP;
        return KOSMOS_OK;
    }

    if (chu_val != (uint8_t)KOSMOS_CHU_VERIFIED) {
        slot->verdict = KOSMOS_VERDICT_HOLD;
        slot->out_u = 0;
        *verdict_out = KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }

    slot->last_nonce = auth_nonce;
    slot->verdict = KOSMOS_VERDICT_OPEN;
    slot->out_u = cand_u;
    slot->open_count += 1u;
    *u_out = cand_u;
    *verdict_out = KOSMOS_VERDICT_OPEN;
    return KOSMOS_OK;
}

size_t kosmos_sizeof_channel_bank(void) {
    return sizeof(kosmos_channel_bank_t);
}
