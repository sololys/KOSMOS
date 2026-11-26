/*
 * KOSMOS — Symplectic Witness-Gated Realization Space
 * TESTS/test_core_c11.c : Native C11 ASan/UBSan test suite
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_space.h"
#include "kosmos_sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ASSERT_TRUE(cond, msg)                                                   \
    do {                                                                         \
        if (!(cond)) {                                                           \
            fprintf(stderr, "[C11 FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg);  \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

static void test_sha256_fips_vectors(void) {
    static const uint8_t expected_empty[32] = {
        0xe3u, 0xb0u, 0xc4u, 0x42u, 0x98u, 0xfcu, 0x1cu, 0x14u,
        0x9au, 0xfbu, 0xf4u, 0xc8u, 0x99u, 0x6fu, 0xb9u, 0x24u,
        0x27u, 0xaeu, 0x41u, 0xe4u, 0x64u, 0x9bu, 0x93u, 0x4cu,
        0xa4u, 0x95u, 0x99u, 0x1bu, 0x78u, 0x52u, 0xb8u, 0x55u
    };
    static const uint8_t expected_abc[32] = {
        0xbau, 0x78u, 0x16u, 0xbfu, 0x8fu, 0x01u, 0xcfu, 0xeau,
        0x41u, 0x41u, 0x40u, 0xdeu, 0x5du, 0xaeu, 0x22u, 0x23u,
        0xb0u, 0x03u, 0x61u, 0xa3u, 0x96u, 0x17u, 0x7au, 0x9cu,
        0xb4u, 0x10u, 0xffu, 0x61u, 0xf2u, 0x00u, 0x15u, 0xadu
    };
    uint8_t digest[32];
    const uint8_t abc[3] = { 'a', 'b', 'c' };

    kosmos_sha256_oneshot(NULL, 0u, digest);
    ASSERT_TRUE(memcmp(digest, expected_empty, 32u) == 0, "SHA-256 empty vector mismatch");

    kosmos_sha256_oneshot(abc, 3u, digest);
    ASSERT_TRUE(memcmp(digest, expected_abc, 32u) == 0, "SHA-256 'abc' vector mismatch");
}

static void test_affine_and_antisymplectic_mirror(void) {
    kosmos_point3_t q1 = { { 1000LL, -2500LL, 9223372036854775000LL } };
    kosmos_point3_t q2 = { { 1450LL, -2000LL, 9223372036854775807LL } };
    kosmos_vec3_t dq;
    kosmos_point3_t q_rebuilt;

    kosmos_phase_t z = {
        { { -42LL, 0LL, KOSMOS_PHASE_MAX } },
        { { KOSMOS_PHASE_MAX, -KOSMOS_PHASE_MAX, 137LL } }
    };
    kosmos_phase_t z_flip;
    kosmos_phase_t z_back;
    kosmos_phase_t z_poison;

    kosmos_vec3_t dq1 = { { KOSMOS_PHASE_MAX, -1234567890123LL, 77LL } };
    kosmos_mom3_t dp1 = { { -9876543210987LL, KOSMOS_PHASE_MAX, -55LL } };
    kosmos_vec3_t dq2 = { { 3333333333333LL, 4444444444444LL, -KOSMOS_PHASE_MAX } };
    kosmos_mom3_t dp2 = { { -1111111111111LL, 2222222222222LL, KOSMOS_PHASE_MAX } };
    kosmos_mom3_t neg_dp1 = { { -dp1.p[0], -dp1.p[1], -dp1.p[2] } };
    kosmos_mom3_t neg_dp2 = { { -dp2.p[0], -dp2.p[1], -dp2.p[2] } };
    kosmos_i192_t w_orig;
    kosmos_i192_t w_pullback;
    kosmos_i192_t neg_w_orig;

    ASSERT_TRUE(kosmos_affine_diff(&q2, &q1, &dq) == KOSMOS_OK, "affine_diff failed");
    ASSERT_TRUE(dq.dq[0] == 450LL && dq.dq[1] == 500LL && dq.dq[2] == 807LL, "dq mismatch");
    ASSERT_TRUE(kosmos_affine_translate(&q1, &dq, &q_rebuilt) == KOSMOS_OK, "translate failed");
    ASSERT_TRUE(memcmp(&q_rebuilt, &q2, sizeof(q2)) == 0, "q1 + (q2 - q1) != q2");

    /* Shift both points by an arbitrary offset: displacement dq is invariant */
    {
        kosmos_vec3_t shift = { { -500000LL, 900000LL, -1000LL } };
        kosmos_point3_t s1, s2;
        kosmos_vec3_t dq_shifted;
        ASSERT_TRUE(kosmos_affine_translate(&q1, &shift, &s1) == KOSMOS_OK, "shift s1");
        ASSERT_TRUE(kosmos_affine_translate(&q2, &shift, &s2) == KOSMOS_OK, "shift s2");
        ASSERT_TRUE(kosmos_affine_diff(&s2, &s1, &dq_shifted) == KOSMOS_OK, "diff shifted");
        ASSERT_TRUE(memcmp(&dq, &dq_shifted, sizeof(dq)) == 0, "Affine origin invariance broken");
    }

    /* Involution J(J(z)) == z */
    ASSERT_TRUE(kosmos_mirror_apply(&z, &z_flip) == KOSMOS_OK, "mirror 1 failed");
    ASSERT_TRUE(z_flip.p.p[0] == -KOSMOS_PHASE_MAX && z_flip.p.p[1] == KOSMOS_PHASE_MAX, "negation failed");
    ASSERT_TRUE(kosmos_mirror_apply(&z_flip, &z_back) == KOSMOS_OK, "mirror 2 failed");
    ASSERT_TRUE(memcmp(&z, &z_back, sizeof(z)) == 0, "J(J(z)) != z");

    /* Two's-complement INT64_MIN poison must be rejected fail-closed */
    z_poison = z;
    z_poison.p.p[1] = KOSMOS_POISON_MIN;
    ASSERT_TRUE(kosmos_mirror_apply(&z_poison, &z_flip) == KOSMOS_ERR_DOMAIN,
                "INT64_MIN poison was not rejected");

    /* Exact 192-bit antisymplectic pullback J* omega == -omega at INT64_MAX scale */
    ASSERT_TRUE(kosmos_symplectic_form(&dq1, &dp1, &dq2, &dp2, &w_orig) == KOSMOS_OK, "omega orig");
    ASSERT_TRUE(kosmos_symplectic_form(&dq1, &neg_dp1, &dq2, &neg_dp2, &w_pullback) == KOSMOS_OK,
                "omega pullback");
    kosmos_i192_neg(&w_orig, &neg_w_orig);
    ASSERT_TRUE(!kosmos_i192_is_zero(&w_orig), "omega unexpectedly zero");
    ASSERT_TRUE(kosmos_i192_eq(&w_pullback, &neg_w_orig), "J* omega != -omega");
}

static void test_end_to_end_gate_invariants(void) {
    kosmos_linf_box_t hard_box;
    kosmos_linf_box_t cand_box;
    kosmos_gate_t gate;
    int64_t hard_ok_meas[KOSMOS_GUARD_DIM] = { 0, 0, 0, 0, 0, 0 };
    kosmos_phase_t cand_z = { { { 10, -20, 30 } }, { { 100, -200, 300 } } };
    kosmos_ctrl_t cand_u = { { 15, -10, 5 } };
    kosmos_ctrl_t u_out;
    uint8_t verdict = 0u;
    kosmos_witness_pre_t w_pre;
    kosmos_witness_post_t w_post;
    uint32_t i;

    memset(&hard_box, 0, sizeof(hard_box));
    memset(&cand_box, 0, sizeof(cand_box));
    hard_box.count = 6u;
    cand_box.count = 6u;
    for (i = 0u; i < 6u; ++i) {
        hard_box.ref[i] = 0;
        hard_box.tol[i] = 1000;
        cand_box.ref[i] = 0;
        cand_box.tol[i] = 100;
    }
    /* Deliberately set tol[3] = 10 while cand_u.u[0] = 15 to catch truncating 15/10 == 1 bug */
    cand_box.tol[3] = 10;

    ASSERT_TRUE(kosmos_gate_init(&gate, &hard_box, &cand_box, 2u, 100u) == KOSMOS_OK, "gate init");
    ASSERT_TRUE(gate.history.len == 1u, "genesis history len != 1");

    /* Mirror flips inside HOLD never change history prefix h */
    ASSERT_TRUE(kosmos_gate_mirror_chamber(&gate, &cand_z) == KOSMOS_OK, "mirror chamber 1");
    ASSERT_TRUE(kosmos_gate_mirror_chamber(&gate, &cand_z) == KOSMOS_OK, "mirror chamber 2");
    ASSERT_TRUE(gate.history.len == 1u, "mirror chamber mutated history");

    /* Candidate u[0]=15 exceeds tol[3]=10 (ratio 1.5): rejected to HOLD (not KILL!) */
    memset(&w_pre, 0, sizeof(w_pre));
    w_pre.witness_id = 0x101u;
    w_pre.auth_nonce = 1u;
    w_pre.cycle = 101u;
    w_pre.seam_defect = 0;
    w_pre.witness_count = 2u;
    w_pre.chu_rows[0] = (uint8_t)KOSMOS_CHU_VERIFIED;
    w_pre.chu_rows[1] = (uint8_t)KOSMOS_CHU_VERIFIED;

    ASSERT_TRUE(kosmos_gate_authorize(&gate, hard_ok_meas, &cand_z, &cand_u, &w_pre, &u_out, &verdict) == KOSMOS_OK,
                "cand exceed call");
    ASSERT_TRUE(verdict == KOSMOS_VERDICT_HOLD && u_out.u[0] == 0,
                "15 vs 10 candidate breach did not stay in HOLD with u=0");
    ASSERT_TRUE(gate.state == (uint32_t)KOSMOS_STATE_HOLD, "candidate breach wrongly tripped KILL");

    /* Bring candidate inside cand_box (u[0] = 9 <= 10) and authorize Stroke 1 */
    cand_u.u[0] = 9;
    w_pre.auth_nonce = 2u;
    w_pre.cycle = 102u;
    ASSERT_TRUE(kosmos_gate_authorize(&gate, hard_ok_meas, &cand_z, &cand_u, &w_pre, &u_out, &verdict) == KOSMOS_OK,
                "valid authorize");
    ASSERT_TRUE(verdict == KOSMOS_VERDICT_OPEN && u_out.u[0] == 9, "OPEN failed");
    ASSERT_TRUE(gate.realized_count == 0u && gate.state == (uint32_t)KOSMOS_STATE_IN_FLIGHT,
                "OPEN prematurely marked REALIZED");

    /* Complete Stroke 2 with tethered W_POST */
    memset(&w_post, 0, sizeof(w_post));
    w_post.witness_id = 0x202u;
    w_post.cycle = 103u;
    memcpy(w_post.tether_pre_hash, w_pre.pre_hash, 32u);
    w_post.outcome_ok = 1u;
    ASSERT_TRUE(kosmos_gate_confirm_post(&gate, &w_post, &verdict) == KOSMOS_OK, "confirm post");
    ASSERT_TRUE(gate.realized_count == 1u && gate.state == (uint32_t)KOSMOS_STATE_HOLD,
                "W_POST failed to finalize realization");
    ASSERT_TRUE(kosmos_history_verify_chain(&gate.history) == KOSMOS_OK, "SHA-256 chain invalid");
}

int main(void) {
    test_sha256_fips_vectors();
    test_affine_and_antisymplectic_mirror();
    test_end_to_end_gate_invariants();
    printf("[PASS] C11 core + ASan/UBSan suite verified.\n");
    return 0;
}
