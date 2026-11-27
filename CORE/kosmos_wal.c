/*
 * KOSMOS v0.3.1 — Cold-Boot fsync WAL, Torn-Tail Quarantine, Signed L01 & flock
 * CORE/kosmos_wal.c (0 float, 0 heap)
 *
 * Hardened against Round-2 & SPRIND review findings:
 *   1. Exclusive flock(fd, LOCK_EX) wraps the entire read-index-compute-write-fsync
 *      cycle across cold-boot, authorize, confirm_post, and signed L01 recovery.
 *   2. Syncs any newly committed intact 128B frames from the locked WAL file so
 *      logical index, head_hash, and last_nonce come from true on-disk state rather
 *      than stale in-memory counters across multi-process writers.
 *   3. Never truncates or overwrites torn tail bytes: forward-pads the torn
 *      128B physical slot with 0xEE and appends a version-2 KTRN quarantine KILL
 *      certificate whose payload_hash = SHA256(padded_torn_slot).
 *   4. Preserves sticky unresolved_latched across KILL so clearing a torn-WAL
 *      or fault KILL steps down to STATE_UNRESOLVED if a prior PRE lacked POST.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "kosmos_v02.h"
#include "kosmos_sha256.h"
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const uint8_t KWAL_MAGIC[4] = { 'K', 'W', 'A', 'L' };
static const uint8_t KTORN_TAG[4]  = { 'K', 'T', 'R', 'N' };

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

static void encode_wal_frame_ver(const kosmos_ledger_entry_t *e,
                                 uint64_t last_nonce,
                                 uint8_t version,
                                 uint8_t frame[KOSMOS_WAL_RECORD_SIZE]) {
    uint8_t seal[32];
    memset(frame, 0, KOSMOS_WAL_RECORD_SIZE);
    memcpy(&frame[0], (version == 2u) ? KTORN_TAG : KWAL_MAGIC, 4u);
    put_be32(&frame[4], (uint32_t)e->index);
    put_be64(&frame[8], e->cycle);
    put_be64(&frame[16], e->witness_id);
    put_be64(&frame[24], last_nonce);
    memcpy(&frame[32], e->payload_hash, 32u);
    memcpy(&frame[64], e->entry_hash, 32u);
    frame[96] = version;
    frame[97] = e->symbol;
    frame[98] = e->verdict;
    kosmos_sha256_oneshot(frame, 99u, seal);
    memcpy(&frame[99], seal, 29u);
}

static int open_locked_wal(const char *wal_path) {
    int fd;
    if (wal_path == NULL) {
        return -1;
    }
    fd = open(wal_path, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return -1;
    }
    if (flock(fd, LOCK_EX) != 0) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

static void close_locked_wal(int fd) {
    if (fd >= 0) {
        (void)flock(fd, LOCK_UN);
        (void)close(fd);
    }
}

static int write_fd_all_fsync(int fd, const uint8_t *buf, size_t len) {
    size_t off = 0u;
    if (fd < 0 || lseek(fd, 0, SEEK_END) < 0) {
        return -1;
    }
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n <= 0) {
            return -1;
        }
        off += (size_t)n;
    }
    if (fsync(fd) != 0) {
        return -1;
    }
    return 0;
}

static int read_exact(int fd, uint8_t *buf, size_t len) {
    size_t off = 0u;
    while (off < len) {
        ssize_t n = read(fd, buf + off, len - off);
        if (n <= 0) {
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int is_valid_kwal_frame(const uint8_t frame[KOSMOS_WAL_RECORD_SIZE], uint8_t expected_ver) {
    uint8_t seal[32];
    const uint8_t *magic = (expected_ver == 2u) ? KTORN_TAG : KWAL_MAGIC;
    if (memcmp(&frame[0], magic, 4u) != 0 || frame[96] != expected_ver) {
        return 0;
    }
    kosmos_sha256_oneshot(frame, 99u, seal);
    return (memcmp(&frame[99], seal, 29u) == 0) ? 1 : 0;
}

static void track_entry_effects(kosmos_gate_t *gate,
                                const kosmos_ledger_entry_t *e,
                                kosmos_ledger_entry_t *last_open_entry,
                                uint8_t *open_still_pending) {
    if (e->symbol == (uint8_t)KOSMOS_EVT_OPEN_AUTHORIZED) {
        *open_still_pending = 1u;
        *last_open_entry = *e;
        memcpy(gate->pending_pre_hash, e->payload_hash, 32u);
    } else if (e->symbol == (uint8_t)KOSMOS_EVT_POST_REALIZED) {
        *open_still_pending = 0u;
        gate->unresolved_latched = 0u;
        memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));
        gate->realized_count += 1u;
    } else if (e->symbol == (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED) {
        *open_still_pending = 1u;
        gate->unresolved_latched = 1u;
    } else if (e->symbol == (uint8_t)KOSMOS_EVT_HARD_KILL) {
        if (*open_still_pending != 0u) {
            gate->unresolved_latched = 1u;
        }
    } else if (e->symbol == (uint8_t)KOSMOS_EVT_L01_RECOVERY) {
        if (gate->state == (uint32_t)KOSMOS_STATE_UNRESOLVED) {
            *open_still_pending = 0u;
            gate->unresolved_latched = 0u;
            memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));
        }
    }
}

/* Under an already-held LOCK_EX fd, replay/sync intact on-disk WAL into gate so
 * gate->history.len, head_hash, last_nonce, last_cycle, and state reflect true disk state. */
static kosmos_status_t sync_gate_from_locked_fd(kosmos_gate_t *gate,
                                                int fd,
                                                uint8_t *open_pending_out,
                                                kosmos_ledger_entry_t *last_open_out) {
    struct stat st_buf;
    size_t file_bytes;
    size_t full_blocks;
    size_t b = 0u;
    uint32_t logical_idx = 0u;
    uint8_t open_pending = 0u;
    kosmos_ledger_entry_t last_open;
    kosmos_linf_box_t saved_hard = gate->hard_box;
    kosmos_linf_box_t saved_cand = gate->cand_box;
    uint32_t saved_min_wit = gate->min_witnesses;

    memset(&last_open, 0, sizeof(last_open));
    if (fstat(fd, &st_buf) != 0 || st_buf.st_size < 0) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }
    file_bytes = (size_t)st_buf.st_size;
    if ((file_bytes % KOSMOS_WAL_RECORD_SIZE) != 0u) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }
    if (file_bytes == 0u) {
        if (open_pending_out != NULL) { *open_pending_out = 0u; }
        return KOSMOS_OK;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }

    memset(gate, 0, sizeof(*gate));
    gate->hard_box = saved_hard;
    gate->cand_box = saved_cand;
    gate->min_witnesses = saved_min_wit;
    gate->state = (uint32_t)KOSMOS_STATE_HOLD;

    full_blocks = file_bytes / KOSMOS_WAL_RECORD_SIZE;
    while (b < full_blocks) {
        uint8_t frame[KOSMOS_WAL_RECORD_SIZE];
        kosmos_ledger_entry_t *e;
        uint32_t idx_wire;
        uint64_t nonce_wire;

        if (logical_idx >= KOSMOS_HISTORY_CAP) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_CAPACITY;
        }
        if (read_exact(fd, frame, KOSMOS_WAL_RECORD_SIZE) != 0) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }
        if (!is_valid_kwal_frame(frame, 1u)) {
            if ((b + 1u) < full_blocks) {
                uint8_t cert[KOSMOS_WAL_RECORD_SIZE];
                uint8_t torn_hash[32];
                if (read_exact(fd, cert, KOSMOS_WAL_RECORD_SIZE) != 0 || !is_valid_kwal_frame(cert, 2u)) {
                    gate->state = (uint32_t)KOSMOS_STATE_KILL;
                    return KOSMOS_ERR_TAMPER;
                }
                kosmos_sha256_oneshot(frame, KOSMOS_WAL_RECORD_SIZE, torn_hash);
                if (memcmp(&cert[32], torn_hash, 32u) != 0) {
                    gate->state = (uint32_t)KOSMOS_STATE_KILL;
                    return KOSMOS_ERR_TAMPER;
                }
                memcpy(frame, cert, KOSMOS_WAL_RECORD_SIZE);
                b += 2u;
            } else {
                if (open_pending != 0u) { gate->unresolved_latched = 1u; }
                gate->state = (uint32_t)KOSMOS_STATE_KILL;
                return KOSMOS_ERR_TAMPER;
            }
        } else {
            b += 1u;
        }

        idx_wire = ((uint32_t)frame[4] << 24u) |
                   ((uint32_t)frame[5] << 16u) |
                   ((uint32_t)frame[6] << 8u) |
                   ((uint32_t)frame[7]);
        if (idx_wire != logical_idx) {
            if (open_pending != 0u) { gate->unresolved_latched = 1u; }
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }

        nonce_wire = get_be64(&frame[24]);
        if (nonce_wire < gate->last_nonce) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }
        gate->last_nonce = nonce_wire;

        e = &gate->history.entries[logical_idx];
        memset(e, 0, sizeof(*e));
        e->index = (uint64_t)logical_idx;
        e->cycle = get_be64(&frame[8]);
        e->witness_id = get_be64(&frame[16]);
        e->symbol = frame[97];
        e->verdict = frame[98];
        if (logical_idx > 0u) {
            memcpy(e->prev_hash, gate->history.entries[logical_idx - 1u].entry_hash, 32u);
        }
        memcpy(e->payload_hash, &frame[32], 32u);
        memcpy(e->entry_hash, &frame[64], 32u);

        gate->history.symbols[logical_idx] = e->symbol;
        memcpy(gate->history.head_hash, e->entry_hash, 32u);
        gate->history.len = logical_idx + 1u;
        gate->last_cycle = e->cycle;

        track_entry_effects(gate, e, &last_open, &open_pending);
        if (e->symbol == (uint8_t)KOSMOS_EVT_HARD_KILL) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
        } else if (e->symbol == (uint8_t)KOSMOS_EVT_OPEN_AUTHORIZED) {
            gate->state = (uint32_t)KOSMOS_STATE_IN_FLIGHT;
            gate->open_cycle = e->cycle;
        } else if (e->symbol == (uint8_t)KOSMOS_EVT_POST_REALIZED) {
            gate->state = (uint32_t)KOSMOS_STATE_HOLD;
            gate->open_cycle = 0u;
        } else if (e->symbol == (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED) {
            gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
        } else if (e->symbol == (uint8_t)KOSMOS_EVT_L01_RECOVERY) {
            if (gate->state == (uint32_t)KOSMOS_STATE_KILL && gate->unresolved_latched != 0u) {
                gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
            } else {
                gate->state = (uint32_t)KOSMOS_STATE_HOLD;
            }
        }
        logical_idx += 1u;
    }

    if (gate->history.len > 0u && kosmos_history_verify_chain(&gate->history) != KOSMOS_OK) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }
    if (open_pending_out != NULL) { *open_pending_out = open_pending; }
    if (last_open_out != NULL) { *last_open_out = last_open; }
    return KOSMOS_OK;
}

static kosmos_status_t append_wal_entries_locked(int fd,
                                                 const kosmos_gate_t *gate,
                                                 uint32_t start_idx) {
    uint32_t i;
    for (i = start_idx; i < gate->history.len; ++i) {
        uint8_t frame[KOSMOS_WAL_RECORD_SIZE];
        encode_wal_frame_ver(&gate->history.entries[i], gate->last_nonce, 1u, frame);
        if (write_fd_all_fsync(fd, frame, sizeof(frame)) != 0) {
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
    size_t full_blocks;
    size_t rem_bytes;
    size_t b;
    uint32_t logical_idx = 0u;
    uint8_t open_pending = 0u;
    kosmos_ledger_entry_t last_open;
    kosmos_status_t st;

    memset(&last_open, 0, sizeof(last_open));
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

    memset(gate, 0, sizeof(*gate));
    gate->min_witnesses = min_witnesses;
    gate->hard_box = *hard_box;
    gate->cand_box = *cand_box;

    /* Acquire exclusive file lock across cold-boot creation / replay / torn quarantine */
    fd = open_locked_wal(wal_path);
    if (fd < 0) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_CAPACITY;
    }

    if (fstat(fd, &st_buf) != 0 || st_buf.st_size < 0) {
        close_locked_wal(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }

    file_bytes = (size_t)st_buf.st_size;
    if (file_bytes == 0u) {
        st = kosmos_gate_init(gate, hard_box, cand_box, min_witnesses, boot_cycle);
        if (st == KOSMOS_OK) {
            st = append_wal_entries_locked(fd, gate, 0u);
        }
        close_locked_wal(fd);
        return st;
    }

    full_blocks = file_bytes / KOSMOS_WAL_RECORD_SIZE;
    rem_bytes = file_bytes % KOSMOS_WAL_RECORD_SIZE;

    b = 0u;
    while (b < full_blocks) {
        uint8_t frame[KOSMOS_WAL_RECORD_SIZE];
        kosmos_ledger_entry_t *e;
        uint32_t idx_wire;
        uint64_t nonce_wire;

        if (logical_idx >= KOSMOS_HISTORY_CAP) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_CAPACITY;
        }
        if (read_exact(fd, frame, KOSMOS_WAL_RECORD_SIZE) != 0) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }

        if (!is_valid_kwal_frame(frame, 1u)) {
            if ((b + 1u) < full_blocks) {
                uint8_t cert[KOSMOS_WAL_RECORD_SIZE];
                uint8_t torn_hash[32];
                if (read_exact(fd, cert, KOSMOS_WAL_RECORD_SIZE) != 0 || !is_valid_kwal_frame(cert, 2u)) {
                    close_locked_wal(fd);
                    gate->state = (uint32_t)KOSMOS_STATE_KILL;
                    return KOSMOS_ERR_TAMPER;
                }
                kosmos_sha256_oneshot(frame, KOSMOS_WAL_RECORD_SIZE, torn_hash);
                if (memcmp(&cert[32], torn_hash, 32u) != 0) {
                    close_locked_wal(fd);
                    gate->state = (uint32_t)KOSMOS_STATE_KILL;
                    return KOSMOS_ERR_TAMPER;
                }
                memcpy(frame, cert, KOSMOS_WAL_RECORD_SIZE);
                b += 2u;
            } else {
                close_locked_wal(fd);
                if (open_pending != 0u) {
                    gate->unresolved_latched = 1u;
                }
                gate->state = (uint32_t)KOSMOS_STATE_KILL;
                return KOSMOS_ERR_TAMPER;
            }
        } else {
            b += 1u;
        }

        idx_wire = ((uint32_t)frame[4] << 24u) |
                   ((uint32_t)frame[5] << 16u) |
                   ((uint32_t)frame[6] << 8u) |
                   ((uint32_t)frame[7]);
        if (idx_wire != logical_idx) {
            close_locked_wal(fd);
            if (open_pending != 0u) {
                gate->unresolved_latched = 1u;
            }
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }

        nonce_wire = get_be64(&frame[24]);
        if (nonce_wire < gate->last_nonce) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }
        gate->last_nonce = nonce_wire;

        e = &gate->history.entries[logical_idx];
        memset(e, 0, sizeof(*e));
        e->index = (uint64_t)logical_idx;
        e->cycle = get_be64(&frame[8]);
        e->witness_id = get_be64(&frame[16]);
        e->symbol = frame[97];
        e->verdict = frame[98];
        if (logical_idx > 0u) {
            memcpy(e->prev_hash, gate->history.entries[logical_idx - 1u].entry_hash, 32u);
        }
        memcpy(e->payload_hash, &frame[32], 32u);
        memcpy(e->entry_hash, &frame[64], 32u);

        gate->history.symbols[logical_idx] = e->symbol;
        memcpy(gate->history.head_hash, e->entry_hash, 32u);
        gate->history.len = logical_idx + 1u;
        gate->last_cycle = e->cycle;

        track_entry_effects(gate, e, &last_open, &open_pending);
        if (e->symbol == (uint8_t)KOSMOS_EVT_HARD_KILL) {
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
        } else if (e->symbol == (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED) {
            gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
        } else if (e->symbol == (uint8_t)KOSMOS_EVT_L01_RECOVERY) {
            if (gate->state == (uint32_t)KOSMOS_STATE_KILL && gate->unresolved_latched != 0u) {
                gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
            } else {
                gate->state = (uint32_t)KOSMOS_STATE_HOLD;
            }
        }
        logical_idx += 1u;
    }

    if (gate->history.len > 0u && kosmos_history_verify_chain(&gate->history) != KOSMOS_OK) {
        close_locked_wal(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }

    /* Handle torn partial tail (1..127 bytes) atomically under LOCK_EX */
    if (rem_bytes != 0u) {
        uint8_t torn_slot[KOSMOS_WAL_RECORD_SIZE];
        uint8_t torn_hash[32];
        uint8_t cert_frame[KOSMOS_WAL_RECORD_SIZE];
        uint8_t append_buf[2u * KOSMOS_WAL_RECORD_SIZE];
        size_t pad_len = KOSMOS_WAL_RECORD_SIZE - rem_bytes;
        uint64_t eff_boot = (boot_cycle > gate->last_cycle) ? boot_cycle : (gate->last_cycle + 1u);

        memset(torn_slot, 0xEE, sizeof(torn_slot));
        if (read_exact(fd, torn_slot, rem_bytes) != 0) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            return KOSMOS_ERR_TAMPER;
        }

        if (open_pending != 0u) {
            gate->unresolved_latched = 1u;
            if (had_unresolved_pre_out != NULL) {
                *had_unresolved_pre_out = 1u;
            }
            if (recovered_pre_out != NULL) {
                *recovered_pre_out = last_open;
            }
        }

        kosmos_sha256_oneshot(torn_slot, KOSMOS_WAL_RECORD_SIZE, torn_hash);
        if (kosmos_history_append(&gate->history,
                                  eff_boot,
                                  (uint8_t)KOSMOS_EVT_HARD_KILL,
                                  KOSMOS_VERDICT_KILL,
                                  0x544F524E57414C21ULL,
                                  torn_hash) == KOSMOS_OK) {
            encode_wal_frame_ver(&gate->history.entries[gate->history.len - 1u],
                                 gate->last_nonce,
                                 2u,
                                 cert_frame);
            memcpy(&append_buf[0], &torn_slot[rem_bytes], pad_len);
            memcpy(&append_buf[pad_len], cert_frame, KOSMOS_WAL_RECORD_SIZE);
            (void)write_fd_all_fsync(fd, append_buf, pad_len + KOSMOS_WAL_RECORD_SIZE);
        }

        close_locked_wal(fd);
        gate->last_cycle = eff_boot;
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_TAMPER;
    }

    /* Intact aligned WAL — inspect terminal entry while still holding LOCK_EX */
    {
        const kosmos_ledger_entry_t *last = &gate->history.entries[gate->history.len - 1u];
        uint64_t eff_boot = (boot_cycle > gate->last_cycle) ? boot_cycle : (gate->last_cycle + 1u);

        if (open_pending != 0u) {
            gate->unresolved_latched = 1u;
            if (had_unresolved_pre_out != NULL) {
                *had_unresolved_pre_out = 1u;
            }
            if (recovered_pre_out != NULL) {
                *recovered_pre_out = last_open;
            }
        }

        if (last->symbol == (uint8_t)KOSMOS_EVT_HARD_KILL) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            gate->last_cycle = eff_boot;
            return KOSMOS_OK;
        }
        if (last->symbol == (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED ||
            (last->symbol == (uint8_t)KOSMOS_EVT_L01_RECOVERY && gate->unresolved_latched != 0u)) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
            gate->last_cycle = eff_boot;
            return KOSMOS_OK;
        }
        if (last->symbol == (uint8_t)KOSMOS_EVT_OPEN_AUTHORIZED) {
            uint32_t prev_len = gate->history.len;
            gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
            gate->unresolved_latched = 1u;
            gate->last_cycle = eff_boot;
            st = kosmos_history_append(&gate->history,
                                       eff_boot,
                                       (uint8_t)KOSMOS_EVT_OUTCOME_UNRESOLVED,
                                       KOSMOS_VERDICT_HOLD,
                                       last->witness_id,
                                       last->payload_hash);
            if (st != KOSMOS_OK) {
                close_locked_wal(fd);
                gate->state = (uint32_t)KOSMOS_STATE_KILL;
                return st;
            }
            st = append_wal_entries_locked(fd, gate, prev_len);
            close_locked_wal(fd);
            return st;
        }

        gate->state = (uint32_t)KOSMOS_STATE_HOLD;
        gate->last_cycle = eff_boot;
    }

    close_locked_wal(fd);
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
    int fd;
    uint32_t prev_len;
    kosmos_status_t sync_st;
    kosmos_status_t st;
    kosmos_status_t wst;

    if (gate == NULL || wal_path == NULL) {
        return KOSMOS_ERR_NULL;
    }
    fd = open_locked_wal(wal_path);
    if (fd < 0) {
        if (u_out != NULL) { memset(u_out, 0, sizeof(*u_out)); }
        if (verdict_out != NULL) { *verdict_out = KOSMOS_VERDICT_KILL; }
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_CAPACITY;
    }

    /* Sync true on-disk logical index & head_hash under LOCK_EX before computing entry */
    sync_st = sync_gate_from_locked_fd(gate, fd, NULL, NULL);
    if (sync_st != KOSMOS_OK) {
        close_locked_wal(fd);
        if (u_out != NULL) { memset(u_out, 0, sizeof(*u_out)); }
        if (verdict_out != NULL) { *verdict_out = KOSMOS_VERDICT_KILL; }
        return sync_st;
    }

    prev_len = gate->history.len;
    st = kosmos_gate_authorize(gate, hard_meas, cand_phase, cand_ctrl, w_pre, u_out, verdict_out);

    if (gate->history.len > prev_len) {
        wst = append_wal_entries_locked(fd, gate, prev_len);
        if (wst != KOSMOS_OK) {
            close_locked_wal(fd);
            if (u_out != NULL) { memset(u_out, 0, sizeof(*u_out)); }
            memset(&gate->active_u, 0, sizeof(gate->active_u));
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            if (verdict_out != NULL) { *verdict_out = KOSMOS_VERDICT_KILL; }
            return wst;
        }
    }
    close_locked_wal(fd);
    return st;
}

kosmos_status_t kosmos_wal_confirm_post(kosmos_gate_t *gate,
                                        const char *wal_path,
                                        const kosmos_witness_post_t *w_post,
                                        uint8_t *verdict_out) {
    int fd;
    uint32_t prev_len;
    kosmos_status_t sync_st;
    kosmos_status_t st;
    kosmos_status_t wst;

    if (gate == NULL || wal_path == NULL) {
        return KOSMOS_ERR_NULL;
    }
    fd = open_locked_wal(wal_path);
    if (fd < 0) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        if (verdict_out != NULL) { *verdict_out = KOSMOS_VERDICT_KILL; }
        return KOSMOS_ERR_CAPACITY;
    }

    sync_st = sync_gate_from_locked_fd(gate, fd, NULL, NULL);
    if (sync_st != KOSMOS_OK) {
        close_locked_wal(fd);
        if (verdict_out != NULL) { *verdict_out = KOSMOS_VERDICT_KILL; }
        return sync_st;
    }

    prev_len = gate->history.len;
    st = kosmos_gate_confirm_post(gate, w_post, verdict_out);

    if (gate->history.len > prev_len) {
        wst = append_wal_entries_locked(fd, gate, prev_len);
        if (wst != KOSMOS_OK) {
            close_locked_wal(fd);
            gate->state = (uint32_t)KOSMOS_STATE_KILL;
            if (verdict_out != NULL) { *verdict_out = KOSMOS_VERDICT_KILL; }
            return wst;
        }
    }
    close_locked_wal(fd);
    return st;
}

kosmos_status_t kosmos_wal_l01_recover_signed(kosmos_gate_t *gate,
                                              const char *wal_path,
                                              const uint8_t key32[32],
                                              const uint8_t sig32[32],
                                              uint64_t cycle,
                                              uint8_t *verdict_out) {
    int fd;
    uint8_t expected_sig[32];
    uint32_t prev_len;
    kosmos_status_t sync_st;
    kosmos_status_t st;
    kosmos_status_t wst;

    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
    }
    if (gate == NULL || wal_path == NULL || key32 == NULL || sig32 == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    fd = open_locked_wal(wal_path);
    if (fd < 0) {
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        return KOSMOS_ERR_CAPACITY;
    }

    sync_st = sync_gate_from_locked_fd(gate, fd, NULL, NULL);
    if (sync_st != KOSMOS_OK) {
        close_locked_wal(fd);
        return sync_st;
    }

    if (cycle <= gate->last_cycle) {
        close_locked_wal(fd);
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_CLOCK;
    }

    kosmos_l01_sign_challenge(key32, gate->history.head_hash, cycle, gate->state, expected_sig);
    if (!ct_memeq_32(expected_sig, sig32)) {
        memset(expected_sig, 0, sizeof(expected_sig));
        close_locked_wal(fd);
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_ERR_AUTH;
    }
    memset(expected_sig, 0, sizeof(expected_sig));

    prev_len = gate->history.len;
    memset(&gate->active_u, 0, sizeof(gate->active_u));

    st = kosmos_history_append(&gate->history,
                               cycle,
                               (uint8_t)KOSMOS_EVT_L01_RECOVERY,
                               KOSMOS_VERDICT_HOLD,
                               0x4C30315349474E44ULL,
                               sig32);
    if (st != KOSMOS_OK) {
        close_locked_wal(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        *verdict_out = KOSMOS_VERDICT_KILL;
        return st;
    }

    wst = append_wal_entries_locked(fd, gate, prev_len);
    if (wst != KOSMOS_OK) {
        close_locked_wal(fd);
        gate->state = (uint32_t)KOSMOS_STATE_KILL;
        *verdict_out = KOSMOS_VERDICT_KILL;
        return wst;
    }

    gate->last_cycle = cycle;
    if (gate->state == (uint32_t)KOSMOS_STATE_KILL && gate->unresolved_latched != 0u) {
        gate->state = (uint32_t)KOSMOS_STATE_UNRESOLVED;
    } else {
        gate->unresolved_latched = 0u;
        memset(gate->pending_pre_hash, 0, sizeof(gate->pending_pre_hash));
        gate->state = (uint32_t)KOSMOS_STATE_HOLD;
    }
    *verdict_out = KOSMOS_VERDICT_HOLD;
    close_locked_wal(fd);
    return KOSMOS_OK;
}
