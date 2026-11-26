/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * CORE/kosmos_sha256.h : Zero-dependency FIPS 180-4 SHA-256 (0 float)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KOSMOS_SHA256_H
#define KOSMOS_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t  buffer[64];
    uint32_t buflen;
} kosmos_sha256_ctx_t;

void kosmos_sha256_init(kosmos_sha256_ctx_t *ctx);
void kosmos_sha256_update(kosmos_sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void kosmos_sha256_final(kosmos_sha256_ctx_t *ctx, uint8_t digest[32]);
void kosmos_sha256_oneshot(const uint8_t *data, size_t len, uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif /* KOSMOS_SHA256_H */
