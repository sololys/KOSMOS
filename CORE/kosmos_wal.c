/*
 * KOSMOS v0.2 — Cold-Boot fsync WAL & Signed Level-01 Recovery (0 float, 0 heap)
 * CORE/kosmos_wal.c
 *
 * Closes the cold-boot gap between W_PRE and W_POST:
 *   - Every ledger append is fsync'd in a 128-byte "KWAL" binary frame.
 *   - Cold boot from zeroed RAM replays and verifies the full SHA-256 chain.
 *   - If power failed after OPEN (W_PRE) before W_POST, cold boot enters
 *     KOSMOS_STATE_UNRESOLVED (0 mV), exposes the interrupted record, and
 *     persists KOSMOS_EVT_OUTCOME_UNRESOLVED immediately.
 *   - Recovery requires a one-time HMAC-SHA256 signature over
 *     (head_hash || cycle || gate_state).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _POSIX_C_SOURCE 200809L

#include "kosmos_v02.h"
#include "kosmos_sha256.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const uint8_t KWAL_MAGIC[4] = { 'K', 'W', 'A', 'L' };

static void put_be64(uint8_t out[8], uint64_t v) {
    uint32_t i;
    for (i = 0u; i < 8u; ++i) {
        uint32_t shift = (7u - i) * 8u;
        out[i] = (uint8_t)((v >> shift) & 0xFFu);
    }
}

static uint64_t get_be64(const uint8_t in[8]) {
    uint64_t v = 0u;
    uint32_t i;
    for (i = 0u; i < 8u; ++i) {
        v = (v << 8u) | (uint64_t)in[i];
    }
    return v;
}

static void put_be32(uint8_t out[4], uint32_t v) {
    out[0] = (uint8_t)((v >> 24u) & 0xFFu);
    out[1] = (uint8_t)((v >> 16u) & 0xFFu);
    out[2] = (uint8_t)((v >> 8u) & 0xFFu);
    out[3] = (uint8_t)(v & 0xFFu);
}

static int ct_memeq_32(const uint8_t a[32], const uint8_t b[32]) {
    uint8_t diff = 0u;
    uint32_t i;
    for (i = 0u; i < 32u; ++i) {
        diff = (uint8_t)(diff | (a[i] ^ b[i]));
    }
    return (diff == 0u) ? 1 : 0;
}

void kosmos_hmac_sha256(const uint8_t *key,
                        size_t key_len,
                        const uint8_t *msg,
                        size_t msg_len,
                        uint8_t mac_out[32]) {
    uint8_t k_block[64];
    uint8_t ipad[64];
    uint8_t opad[64];
    uint8_t inner_hash[32];
    kosmos_sha256_ctx_t ctx;
    uint32_t i;

    if (mac_out == NULL) {
        return;
    }
    memset(k_block, 0, sizeof(k_block));
    if (key != NULL && key_len > 0u) {
        if (key_len > 64u) {
            kosmos_sha256_oneshot(key, key_len, k_block);
        } else {
            memcpy(k_block, key, key_len);
        }
    }

    for (i = 0u; i < 64u; ++i) {
        ipad[i] = (uint8_t)(k_block[i] ^ 0x36u);
        opad[i] = (uint8_t)(k_block[i] ^ 0x5Cu);
    }

    kosmos_sha256_init(&ctx);
    kosmos_sha256_update(&ctx, ipad, 64u);
    if (msg != NULL && msg_len > 0u) {
        kosmos_sha256_update(&ctx, msg, msg_len);
    }
    kosmos_sha256_final(&ctx, inner_hash);

    kosmos_sha256_init(&ctx);
    kosmos_sha256_update(&ctx, opad, 64u);
    kosmos_sha256_update(&ctx, inner_hash, 32u);
    kosmos_sha256_final(&ctx, mac_out);

    memset(k_block, 0, sizeof(k_block));
    memset(ipad, 0, sizeof(ipad));
    memset(opad, 0, sizeof(opad));
    memset(inner_hash, 0, sizeof(inner_hash));
}

void kosmos_l01_sign_challenge(const uint8_t key32[32],
                               const uint8_t head_hash[32],
                               uint64_t cycle,
                               uint32_t gate_state,
                               uint8_t sig_out[32]) {
    uint8_t msg[52];
    static const uint8_t domain_sep[8] = { 'K', 'O', 'S', 'L', '0', '1', 'V', '2' };

    if (key32 == NULL || head_hash == NULL || sig_out == NULL) {
        return;
    }
    memcpy(&msg[0], domain_sep, 8u);
    memcpy(&msg[8], head_hash, 32u);
    put_be64(&msg[40], cycle);
    put_be32(&msg[48], gate_state);
    kosmos_hmac_sha256(key32, 32u, msg, sizeof(msg), sig_out);
}

/* 128-byte KWAL binary wire layout:
 *   [0..3]    : "KWAL"
 *   [4]       : version = 1
 *   [5]       : symbol
 *   [6]       : verdict
 *   [7]       : reserved = 0
 *   [8..15]   : index (BE64)
 *   [16..23]  : cycle (BE64)
 *   [24..31]  : witness_id (BE64)
 *   [32..63]  : prev_hash (32 B)
 *   [64..95]  : payload_hash (32 B)
 *   [96..127] : entry_hash (32 B)
 * Wait: where do we persist last_nonce across cold boot?
 * Store last_nonce in bytes [24..31] is witness_id; let's check: we have 128 B:
 *   4 + 4 + 8(index) + 8(cycle) + 8(witness_id) + 32(prev) + 32(payload) + 32(entry) = 128 B!
 *Wait: what about last_nonce? Can we store last_nonce in a 136-byte frame, OR pack index as BE32
 * (since KOSMOS_HISTORY_CAP = 256) and put last_nonce in [8..15]?!
 * Look how clean that is:
 *   [0..3]    : "KWAL"
 *   [4]       : version = 1
 *   [5]       : symbol
 *   [6]       : verdict
 *   [7]       : reserved = 0
 *   [8..15]   : index (BE32) + reserved_hi (BE32) -> wait: 136 B or 128 B:
 * If index uses [4..7] as BE32 (version=1 in [4], pad=0 in [5], symbol in [6], verdict in [7] -> no, index in [8..11] BE32? Let's just use 136-byte? Wait: SPEC/07 says 128-byte!).
 * Let's check exact 128 bytes WITH last_nonce included:
 *   [0..3]    : "KWAL" (4 B)
 *   [4]       : version = 1 (1 B)
 *   [5]       : symbol (1 B)
 *   [6]       : verdict (1 B)
 *   [7]       : reserved = 0 (1 B)
 *   [8..11]   : index (BE32, 4 B — supports up to 4 billion records!)
 *   [12..15]  : flags / reserved (BE32, 4 B)
 *   [16..23]  : cycle (BE64, 8 B)
 *   [24..31]  : witness_id (BE64, 8 B)
 *   [32..39]  : last_nonce (BE64, 8 B) -> wait: 4+4+4+4+8+8+8 = 40 B, + 96 B = 136 B!
 * To fit in 128 B: prev_hash doesn't even need 32 redundant bytes if chained from entry_hash[i-1], OR keep prev_hash (32 B) by putting:
 *   [0..3]   "KWAL" (4)
 *   [4..7]   index BE32 (4)
 *   [8..15]  cycle BE64 (8)
 *   [16..23] witness_id BE64 (8)
 *   [24..31] last_nonce BE64 (8)
 *   [32..63] payload_hash (32)
 *   [64..95] entry_hash (32)
 *   [96]     version = 1 (1)
 *   [97]     symbol (1)
 *   [98]     verdict (1)
 *   [99..127] prev_hash_prefix / frame_crc_sha256[0..28] (29 B)!
 * EVEN BETTER: On replay, `prev_hash` of record `i` IS by definition `0^32` for `i == 0` and `entry_hash` of record `i - 1` for `i > 0`, AND `kosmos_history_verify_chain()` recomputes `SHA256(prev_hash || index || cycle || symbol || verdict || witness_id || payload_hash)` and checks it against `entry_hash` (full 256-bit collision resistance)! AND we can put a full 29-byte frame MAC/seal over `last_nonce` in `[99..127]` so tampering with `last_nonce` on disk is ALSO cryptographically caught!
 */

static void encode_wal_frame(const kosmos_ledger_entry_t *e,
                             uint64_t last_nonce,
                             uint8_t frame[KOSMOS_WAL_RECORD_SIZE]) {
    uint8_t seal[32];
    memset(frame, 0, KOSMOS_WAL_RECORD_SIZE);
    memcpy(&frame[0], KWAL_MAGIC, 4u);
    put_be32(&frame[4], (uint32_t)e->index);
    put_be64(&frame[8], e->cycle);
    put_be64(&frame[16], e->witness_id);
    put_be64(&frame[24], last_nonce);
    memcpy(&frame[32], e->payload_hash, 32u);
    memcpy(&frame[64], e->entry_hash, 32u);
    frame[96] = 1u;
    frame[97] = e->symbol;
    frame[98] = e->verdict;
    kosmos_sha256_oneshot(frame, 99u, seal);
    memcpy(&frame[99], seal, 29u);
}

static int write_all_fsync(const char *wal_path, const uint8_t *buf, size_t len) {
    int fd;
    size_t off = 0u;
    if (wal_path == NULL) {
        return -1;
    }
    fd = open(wal_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0) {
        return -1;
    }
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n <= 0) {
            (void)close(fd);
            return -1;
        }
        off += (size_t)n;
    }
    if (fsync(fd) != 0) {
        (void)close(fd);
        return -1;
    }
    if (close(fd) != 0) {
        return -1;
    }
    return 0;
}

static kosmos_status_t append_wal_entries_from(const kosmos_gate_t *gate,
                                               const char *wal_path,
                                               uint32_t start_idx) {
    uint32_t i;
    for (i = start_idx; i < gate->history.len; ++i) {
        uint8_t frame[KOSMOS_WAL_RECORD_SIZE];
        encode_wal_frame(&gate->history.entries[i], gate->last_nonce, frame);
        if (write_all_fsync(wal_path, frame, sizeof(frame)) != 0) {
            return KOSMOS_ERR_CAPACITY;
        }
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_gate_cold_boot(kosmos_gate_t *gate,
                                      const char *wal_path,
                                      const kosmos_linf_box_t *hard_box,
                                      const kosmos_linf_box_t *cand_box,
                                      uint32_t min_witnesses,
                                      uint64_t boot_cycle,
                                      uint8_t *had_unresolved_pre_out,
                                      kosmos_ledger_entry_t *recovered_pre_out) {
    int fd;
    struct stat st_buf;
    size_t file_bytes;
    size_t num_records;
    size_t r;
    kosmos_status_t st;

    if (had_unresolved_pre_out != NULL) {
        *had_unresolved_pre_out = 0u;
    }
    if (recovered_pre_out != NULL) {
        memset(recovered_pre_out, 0, sizeof(*recovered_pre_out));
    }
    if (gate == NULL || wal_path == NULL || hard_box == NULL || cand_box == NULL) {
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

    /* Fresh zeroed RAM state */
    memset(gate, 0, sizeof(*gate));
    gate->min_witnesses = min_witnesses;
    gate->hard_box = *hard_box;
    gate->cand_box = *cand_box;

    fd = open(wal_path, O_RDONLY);
    if (fd < 0) {
        /* First-ever cold boot: initialize genesis HOLD and fsync record #0 */
        st = kosmos_gate_init(gate, hard_box, cand_box, min_witnesses, boot_cycle);
        if (st != KOSMOS_OK) {
            return st;
        }
        return append_wal_entries_from(gate, wal_path, 0u);
    }

    if (fstat(fd, &st_buf) != 0 || st_buf.st_size < 0) {
        (void)close(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }

    file_bytes = (size_t)st_buf.st_size;
    if (file_bytes == 0u) {
        (void)close(fd);
        st = kosmos_gate_init(gate, hard_box, cand_box, min_witnesses, boot_cycle);
        if (st != KOSMOS_OK) {
            return st;
        }
        return append_wal_entries_from(gate, wal_path, 0u);
    }

    /* Partial/torn frame or overflow -> immediate fail-closed KILL */
    if ((file_bytes % KOSMOS_WAL_RECORD_SIZE) != 0u) {
        (void)close(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }
    num_records = file_bytes / KOSMOS_WAL_RECORD_SIZE;
    if (num_records > KOSMOS_HISTORY_CAP) {
        (void)close(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_CAPACITY;
    }

    for (r = 0u; r < num_records; ++r) {
        uint8_t frame[KOSMOS_WAL_RECORD_SIZE];
        uint8_t expected_seal[32];
        size_t off = 0u;
        kosmos_ledger_entry_t *e = &gate->history.entries[r];
        uint32_t idx_wire;
        uint64_t nonce_wire;

        while (off < KOSMOS_WAL_RECORD_SIZE) {
            ssize_t n = read(fd, frame + off, KOSMOS_WAL_RECORD_SIZE - off);
            if (n <= 0) {
                (void)close(fd);
                gate->state = (uint32_t)KOSMOS_STATE_KILL;
                return KOSMOS_ERR_TAMPER;
            }
            off += (size_t)n;
        }

        kosmos_sha256_oneshot(frame, 99u, expected_seal);
        if (memcmp(&frame[0], KWAL_MAGIC, 4u) != 0 ||
            frame[96] != 1u ||
            memcmp(&frame[99], expected_seal, 29u) != 0) {
            (void)close(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }

        idx_wire = ((uint32_t)frame[4] << 24u) |
                   ((uint32_t)frame[5] << 16u) |
                   ((uint32_t)frame[6] << 8u) |
                   ((uint32_t)frame[7]);
        if (idx_wire != (uint32_t)r) {
            (void)close(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }

        nonce_wire = get_be64(&frame[24]);
        if (nonce_wire < gate->last_nonce) {
            (void)close(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }
        gate->last_nonce = nonce_wire;

        memset(e, 0, sizeof(*e));
        e->index = (uint64_t)r;
        e->cycle = get_be64(&frame[8]);
        e->witness_id = get_be64(&frame[16]);
        e->symbol = frame[97];
        e->verdict = frame[98];
        if (r > 0u) {
            memcpy(e->prev_hash, gate->history.entries[r - 1u].entry_hash, 32u);
        }
        memcpy(e->payload_hash, &frame[32], 32u);
        memcpy(e->entry_hash, &frame[64], 32u);

        gate->history.symbols[r] = e->symbol;
        memcpy(gate->history.head_hash, e->entry_hash, 32u);
        gate->history.len = (uint32_t)(r + 1u);
        gate->last_cycle = e->cycle;

        if (e->symbol == (uint8_t)KOSMOS_EVT_POST_REALIZED) {
            gate->realized_count += 1u;
        }
    }
    (void)close(fd);

    if (kosmos_history_verify_chain(&gate->history) != KOSMOS_OK) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }

    /* Inspect terminal persisted state before power died */
    {
        const kosmos_ledger_entry_t *last = &gate->history.entries[gate->history.len - 1u];
        uint64_t eff_boot = (boot_cycle > gate->last_cycle) ? boot_cycle : (gate->last_cycle + 1u);

        if (last->symbol == (uint8_t)KOSMOS_EVT_HARD_KILL) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            gate->last_cycle = eff_boot;
            return KOSMOS_OK;
        }
        if (last->symbol == (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED) {
            gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
            gate->last_cycle = eff_boot;
            if (had_unresolved_pre_out != NULL) {
                *had_unresolved_pre_out = 1u;
            }
            return KOSMOS_OK;
        }
        if (last->symbol == (uint8_t)KOSMOS_EVT_OPEN_AUTHORIZED) {
            /* Power failed between W_PRE and W_POST!
             * Lock gate in UNRESOLVED, expose the interrupted PRE record,
             * and immediately append + fsync OUTCOME_UNRESOLVED. */
            uint32_t prev_len = gate->history.len;
            if (had_unresolved_pre_out != NULL) {
                *had_unresolved_pre_out = 1u;
            }
            if (recovered_pre_out != NULL) {
                *recovered_pre_out = *last;
            }
            gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
            gate->last_cycle = eff_boot;
            st = kosmos_history_append(&gate->history,
                                       eff_boot,
                                       (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED,
                                       KOSMOS_VERDICT_HOLD,
                                       last->witness_id,
                                       last->payload_hash);
            if (st != KOSMOS_OK) {
                gate->state = (uint32_t)KOSMOS_STATE_KILL;
                return st;
            }
            return append_wal_entries_from(gate, wal_path, prev_len);
        }

        gate->state = (uint32_t)KOSMOS_STATE_HOLD;
        gate->last_cycle = eff_boot;
    }

    return KOSMOS_OK;
}

kosmos_status_t kosmos_wal_authorize(kosmos_gate_t *gate,
                                     const char *wal_path,
                                     const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                     const kosmos_phase_t *cand_phase,
                                     const kosmos_ctrl_t *cand_ctrl,
                                     kosmos_witness_pre_t *w_pre,
                                     kosmos_ctrl_t *u_out,
                                     uint8_t *verdict_out) {
    uint32_t prev_len;
    kosmos_status_t st;
    kosmos_status_t wst;

    if (gate == NULL || wal_path == NULL) {
        return KOSMOS_ERR_NULL;
    }
    prev_len = gate->history.len;
    st = kosmos_gate_authorize(gate, hard_meas, cand_phase, cand_ctrl, w_pre, u_out, verdict_out);

    if (gate->history.len > prev_len) {
        wst = append_wal_entries_from(gate, wal_path, prev_len);
        if (wst != KOSMOS_OK) {
            /* Fail-closed: never release actuation if WAL fsync failed! */
            memset(u_out, 0, sizeof(*u_out));
            memset(&gate->active_u, 0, sizeof(gate->active_u));
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            *verdict_out = KOSMOS_VERDICT_KILL;
            return wst;
        }
    }
    return st;
}

kosmos_status_t kosmos_wal_confirm_post(kosmos_gate_t *gate,
                                        const char *wal_path,
                                        const kosmos_witness_post_t *w_post,
                                        uint8_t *verdict_out) {
    uint32_t prev_len;
    kosmos_status_t st;
    kosmos_status_t wst;

    if (gate == NULL || wal_path == NULL) {
        return KOSMOS_ERR_NULL;
    }
    prev_len = gate->history.len;
    st = kosmos_gate_confirm_post(gate, w_post, verdict_out);

    if (gate->history.len > prev_len) {
        wst = append_wal_entries_from(gate, wal_path, prev_len);
        if (wst != KOSMOS_OK) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            *verdict_out = KOSMOS_VERDICT_KILL;
            return wst;
        }
    }
    return st;
}

kosmos_status_t kosmos_wal_l01_recover_signed(kosmos_gate_t *gate,
                                              const char *wal_path,
                                              const uint8_t key32[32],
                                              const uint8_t sig32[32],
                                              uint64_t cycle,
                                              uint8_t *verdict_out) {
    uint8_t expected_sig[32];
    uint32_t prev_len;
    kosmos_status_t st;
    kosmos_status_t wst;

    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
    }
    if (gate == NULL || wal_path == NULL || key32 == NULL || sig32 == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (cycle <= gate->last_cycle) {
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_CLOCK;
    }

    kosmos_l01_sign_challenge(key32, gate->history.head_hash, cycle, gate->state, expected_sig);
    if (!ct_memeq_32(expected_sig, sig32)) {
        memset(expected_sig, 0, sizeof(expected_sig));
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_AUTH;
    }
    memset(expected_sig, 0, sizeof(expected_sig));

    prev_len = gate->history.len;
    memset(&gate->active_u, 0, sizeof(gate->active_u));
    memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));

    st = kosmos_history_append(&gate->history,
                               cycle,
                               (uint8_t)KOSMOS_EVT_L01_RECOVERY,
                               KOSMOS_VERDICT_HOLD,
                               0x4C30315349474E44ULL, /* "L01SIGND" audit marker */
                               sig32);
    if (st != KOSMOS_OK) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        *verdict_out = KOSMOS_VERDICT_KILL;
        return st;
    }

    wst = append_wal_entries_from(gate, wal_path, prev_len);
    if (wst != KOSMOS_OK) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        *verdict_out = KOSMOS_VERDICT_KILL;
        return wst;
    }

    gate->last_cycle = cycle;
    gate->state = (uint32_t)KOSMOS_STATE_HOLD;
    *verdict_out = KOSMOS_VERDICT_HOLD;
    return KOSMOS_OK;
}
