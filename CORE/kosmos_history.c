/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * CORE/kosmos_history.c : Append-only prefix history B = A* and Cantor space S
 *
 * Enforces h_{n+1} = h_n || e_n, SHA-256 chain integrity, zero-float Cantor
 * ultrametric comparison via common-prefix depth, and cylinder narrowing
 * C_{h || e} subset C_h.
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

static void compute_entry_hash(const kosmos_ledger_entry_t *e, uint8_t digest[32]) {
    kosmos_sha256_ctx_t ctx;
    uint8_t buf[8];

    kosmos_sha256_init(&ctx);
    kosmos_sha256_update(&ctx, e->prev_hash, 32u);
    put_be64(buf, e->index);
    kosmos_sha256_update(&ctx, buf, 8u);
    put_be64(buf, e->cycle);
    kosmos_sha256_update(&ctx, buf, 8u);
    kosmos_sha256_update(&ctx, &e->symbol, 1u);
    kosmos_sha256_update(&ctx, &e->verdict, 1u);
    put_be64(buf, e->witness_id);
    kosmos_sha256_update(&ctx, buf, 8u);
    kosmos_sha256_update(&ctx, e->payload_hash, 32u);
    kosmos_sha256_final(&ctx, digest);
}

kosmos_status_t kosmos_history_init(kosmos_history_t *hist, uint64_t boot_cycle) {
    uint8_t zero_payload[32];
    if (hist == NULL) {
        return KOSMOS_ERR_NULL;
    }
    memset(hist, 0, sizeof(*hist));
    memset(zero_payload, 0, sizeof(zero_payload));
    return kosmos_history_append(hist,
                                 boot_cycle,
                                 (uint8_t)KOSMOS_EVT_BOOT_HOLD,
                                 KOSMOS_VERDICT_HOLD,
                                 0u,
                                 zero_payload);
}

kosmos_status_t kosmos_history_append(kosmos_history_t *hist,
                                      uint64_t cycle,
                                      uint8_t symbol,
                                      uint8_t verdict,
                                      uint64_t witness_id,
                                      const uint8_t payload_hash[32]) {
    kosmos_ledger_entry_t *entry;
    uint32_t idx;

    if (hist == NULL || payload_hash == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (symbol >= KOSMOS_ALPHABET_SIZE) {
        return KOSMOS_ERR_DOMAIN;
    }
    if (verdict != KOSMOS_VERDICT_KILL &&
        verdict != KOSMOS_VERDICT_HOLD &&
        verdict != KOSMOS_VERDICT_OPEN) {
        return KOSMOS_ERR_DOMAIN;
    }
    if (hist->len >= KOSMOS_HISTORY_CAP) {
        return KOSMOS_ERR_CAPACITY;
    }
    if (hist->len > 0u && cycle < hist->entries[hist->len - 1u].cycle) {
        return KOSMOS_ERR_CLOCK;
    }

    idx = hist->len;
    entry = &hist->entries[idx];
    memset(entry, 0, sizeof(*entry));

    entry->index = (uint64_t)idx;
    entry->cycle = cycle;
    entry->witness_id = witness_id;
    entry->symbol = symbol;
    entry->verdict = verdict;
    memcpy(entry->prev_hash, hist->head_hash, 32u);
    memcpy(entry->payload_hash, payload_hash, 32u);
    compute_entry_hash(entry, entry->entry_hash);

    hist->symbols[idx] = symbol;
    memcpy(hist->head_hash, entry->entry_hash, 32u);
    hist->len = idx + 1u;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_history_verify_chain(const kosmos_history_t *hist) {
    uint8_t expected_prev[32];
    uint8_t computed[32];
    uint32_t i;

    if (hist == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (hist->len == 0u || hist->len > KOSMOS_HISTORY_CAP) {
        return KOSMOS_ERR_TAMPER;
    }

    memset(expected_prev, 0, sizeof(expected_prev));

    for (i = 0u; i < hist->len; ++i) {
        const kosmos_ledger_entry_t *e = &hist->entries[i];
        if (e->index != (uint64_t)i) {
            return KOSMOS_ERR_TAMPER;
        }
        if (e->symbol >= KOSMOS_ALPHABET_SIZE || hist->symbols[i] != e->symbol) {
            return KOSMOS_ERR_TAMPER;
        }
        if (i > 0u && e->cycle < hist->entries[i - 1u].cycle) {
            return KOSMOS_ERR_TAMPER;
        }
        if (memcmp(e->prev_hash, expected_prev, 32u) != 0) {
            return KOSMOS_ERR_TAMPER;
        }
        compute_entry_hash(e, computed);
        if (memcmp(e->entry_hash, computed, 32u) != 0) {
            return KOSMOS_ERR_TAMPER;
        }
        memcpy(expected_prev, computed, 32u);
    }

    if (memcmp(hist->head_hash, expected_prev, 32u) != 0) {
        return KOSMOS_ERR_TAMPER;
    }
    return KOSMOS_OK;
}

int kosmos_prefix_leq(const uint8_t *prefix,
                      size_t prefix_len,
                      const uint8_t *seq,
                      size_t seq_len) {
    size_t i;
    if ((prefix == NULL && prefix_len > 0u) || (seq == NULL && seq_len > 0u)) {
        return 0;
    }
    if (prefix_len > seq_len) {
        return 0;
    }
    for (i = 0u; i < prefix_len; ++i) {
        if (prefix[i] != seq[i]) {
            return 0;
        }
    }
    return 1;
}

size_t kosmos_cantor_common_prefix(const uint8_t *s,
                                   size_t s_len,
                                   const uint8_t *t,
                                   size_t t_len) {
    size_t limit = (s_len < t_len) ? s_len : t_len;
    size_t i;
    if (s == NULL || t == NULL) {
        return 0u;
    }
    for (i = 0u; i < limit; ++i) {
        if (s[i] != t[i]) {
            return i;
        }
    }
    return limit;
}

int kosmos_cantor_closer(const uint8_t *ref_seq,
                         size_t ref_len,
                         const uint8_t *cand_a,
                         size_t a_len,
                         const uint8_t *cand_b,
                         size_t b_len) {
    /* d_S(ref, a) = 2^{-l(ref, a)} < d_S(ref, b) = 2^{-l(ref, b)}
     * iff integer common prefix length l(ref, a) > l(ref, b). */
    size_t la = kosmos_cantor_common_prefix(ref_seq, ref_len, cand_a, a_len);
    size_t lb = kosmos_cantor_common_prefix(ref_seq, ref_len, cand_b, b_len);
    return (la > lb) ? 1 : 0;
}

int kosmos_cantor_in_cylinder(const kosmos_history_t *hist,
                              const uint8_t *future_seq,
                              size_t seq_len) {
    if (hist == NULL) {
        return 0;
    }
    return kosmos_prefix_leq(hist->symbols, (size_t)hist->len, future_seq, seq_len);
}
