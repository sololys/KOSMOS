// KOSMOS — Symplectic Witness-Gated Realization Space
// RTL/kosmos_gate.sv : Synthesizable IEEE 1800-2012 Consequence Gate (0 float)
//
// Enforces:
//   1. Hardware antisymplectic mirror J(q, p) = (q, -p) with INT32_MIN poison fence
//   2. Division-free 33-bit exact L-infinity hard & candidate envelopes
//   3. Belnap-Dunn 4-valued Chu witness reducer (00=?, 01=0, 10=1, 11=CONFLICT)
//   4. Loop-space holonomy seam check (seam_defect != 0 -> KILL)
//   5. Two-stroke realization (OPEN != REALIZED; missing/untethered POST -> UNRESOLVED)
//   6. Monotonic anti-replay nonce & append-only CRC-32 history chain
//   7. Combinational zero-clamp on actuator outputs unless verdict == OPEN (8'hA5)
//
// SPDX-License-Identifier: Apache-2.0

`timescale 1ns / 1ps
`default_nettype none

module kosmos_gate #(
    parameter [31:0] MAX_INFLIGHT_CYCLES = 32'd16
) (
    input  wire                clk,
    input  wire                rst_n,

    // Hard plant measurements & symmetric L-infinity tolerances
    input  wire  signed [31:0] hard_meas_0,
    input  wire  signed [31:0] hard_meas_1,
    input  wire  signed [31:0] hard_meas_2,
    input  wire  signed [31:0] hard_tol_0,
    input  wire  signed [31:0] hard_tol_1,
    input  wire  signed [31:0] hard_tol_2,

    // Candidate symplectic phase z = (q, p) and control u
    input  wire  signed [31:0] q_x,
    input  wire  signed [31:0] q_y,
    input  wire  signed [31:0] q_z,
    input  wire  signed [31:0] p_x,
    input  wire  signed [31:0] p_y,
    input  wire  signed [31:0] p_z,
    input  wire  signed [31:0] u_x,
    input  wire  signed [31:0] u_y,
    input  wire  signed [31:0] u_z,
    input  wire  signed [31:0] cand_pos_tol,
    input  wire  signed [31:0] cand_ctrl_tol,

    // Stroke 1: Pre-witness W_PRE, Belnap-Dunn Chu rows, and Loop seam defect
    input  wire                req_open,
    input  wire         [31:0] witness_pre_id,
    input  wire         [31:0] auth_nonce,
    input  wire  signed [31:0] seam_defect,
    input  wire          [1:0] chu_w0,
    input  wire          [1:0] chu_w1,
    input  wire          [1:0] chu_w2,
    input  wire          [1:0] chu_w3,

    // Stroke 2: Tethered Post-witness W_POST & power-cut fault line
    input  wire                req_post,
    input  wire         [31:0] witness_post_id,
    input  wire         [31:0] post_tether_tag,
    input  wire                post_outcome_ok,
    input  wire                power_cut_fault,

    // Sovereign Level-01 physical recovery
    input  wire                req_l01_recover,
    input  wire         [63:0] l01_key,

    // Combinational phase mirror outputs J(q, p) = (q, -p)
    output logic signed [31:0] mirror_q_x,
    output logic signed [31:0] mirror_q_y,
    output logic signed [31:0] mirror_q_z,
    output logic signed [31:0] mirror_p_x,
    output logic signed [31:0] mirror_p_y,
    output logic signed [31:0] mirror_p_z,
    output logic               mirror_poison,

    // Gate outputs, tethered tag, and append-only history state
    output logic         [1:0] lock_state,
    output logic         [7:0] verdict_code,
    output logic        [31:0] issued_pre_tag,
    output logic        [31:0] history_len,
    output logic        [31:0] realized_cnt,
    output logic        [31:0] history_digest,
    output logic signed [31:0] out_u_x,
    output logic signed [31:0] out_u_y,
    output logic signed [31:0] out_u_z
);

    localparam [1:0] STATE_HOLD       = 2'd0;
    localparam [1:0] STATE_IN_FLIGHT  = 2'd1;
    localparam [1:0] STATE_UNRESOLVED = 2'd2;
    localparam [1:0] STATE_KILL       = 2'd3;

    localparam [7:0] VERDICT_KILL     = 8'h00;
    localparam [7:0] VERDICT_HOLD     = 8'h3C;
    localparam [7:0] VERDICT_OPEN     = 8'hA5;

    localparam [1:0] CHU_VERIFIED     = 2'b10;
    localparam [1:0] CHU_CONFLICT     = 2'b11;

    localparam [2:0] EVT_BOOT_HOLD    = 3'd0;
    localparam [2:0] EVT_OPEN_AUTH    = 3'd3;
    localparam [2:0] EVT_POST_REAL    = 3'd4;
    localparam [2:0] EVT_UNRESOLVED   = 3'd5;
    localparam [2:0] EVT_HARD_KILL    = 3'd6;
    localparam [2:0] EVT_L01_RECOVER  = 3'd7;

    localparam signed [31:0] POISON_I32_MIN = 32'sh8000_0000;
    localparam        [63:0] L01_KEY_MAGIC  = 64'h4B4F_534D_4F53_3031;

    // 33-bit exact magnitude |v| for v != INT32_MIN
    function automatic [32:0] abs33(input signed [31:0] v);
        reg signed [32:0] ext;
        begin
            ext   = v;
            abs33 = (ext < 33'sd0) ? $unsigned(-ext) : $unsigned(ext);
        end
    endfunction

    // Deterministic 32-bit CRC-32 step mixing event symbol, verdict, and tag
    function automatic [31:0] mix_digest(
        input [31:0] prev_crc,
        input  [2:0] sym,
        input  [7:0] verd,
        input [31:0] tag_word
    );
        reg [31:0] c;
        integer k;
        begin
            c = prev_crc ^ {21'd0, sym, verd} ^ tag_word;
            for (k = 0; k < 8; k = k + 1) begin
                if ((c & 32'd1) != 32'd0)
                    c = (c >> 1) ^ 32'hEDB8_8320;
                else
                    c = (c >> 1);
            end
            mix_digest = c;
        end
    endfunction

    // Sequential registers declared prior to combinational block
    logic         [1:0] state_q;
    logic         [7:0] verdict_q;
    logic        [31:0] last_nonce_q;
    logic        [31:0] pending_tag_q;
    logic        [31:0] inflight_age_q;
    logic        [31:0] history_len_q;
    logic        [31:0] realized_cnt_q;
    logic        [31:0] digest_q;
    logic signed [31:0] latched_u_x_q;
    logic signed [31:0] latched_u_y_q;
    logic signed [31:0] latched_u_z_q;

    // Combinational checks
    logic        any_poison;
    logic        hard_inside;
    logic        cand_inside;
    logic  [1:0] chu_fold;
    logic [31:0] next_pre_tag;

    always_comb begin
        any_poison = (hard_meas_0 == POISON_I32_MIN) ||
                     (hard_meas_1 == POISON_I32_MIN) ||
                     (hard_meas_2 == POISON_I32_MIN) ||
                     (q_x == POISON_I32_MIN) || (q_y == POISON_I32_MIN) || (q_z == POISON_I32_MIN) ||
                     (p_x == POISON_I32_MIN) || (p_y == POISON_I32_MIN) || (p_z == POISON_I32_MIN) ||
                     (u_x == POISON_I32_MIN) || (u_y == POISON_I32_MIN) || (u_z == POISON_I32_MIN) ||
                     (seam_defect == POISON_I32_MIN);

        mirror_poison = any_poison;
        mirror_q_x    = any_poison ? 32'sd0 : q_x;
        mirror_q_y    = any_poison ? 32'sd0 : q_y;
        mirror_q_z    = any_poison ? 32'sd0 : q_z;
        mirror_p_x    = any_poison ? 32'sd0 : -p_x;
        mirror_p_y    = any_poison ? 32'sd0 : -p_y;
        mirror_p_z    = any_poison ? 32'sd0 : -p_z;

        hard_inside = (!any_poison) &&
                      (hard_tol_0 > 32'sd0) && (hard_tol_1 > 32'sd0) && (hard_tol_2 > 32'sd0) &&
                      (abs33(hard_meas_0) <= {1'b0, hard_tol_0}) &&
                      (abs33(hard_meas_1) <= {1'b0, hard_tol_1}) &&
                      (abs33(hard_meas_2) <= {1'b0, hard_tol_2});

        cand_inside = (!any_poison) &&
                      (cand_pos_tol > 32'sd0) && (cand_ctrl_tol > 32'sd0) &&
                      (abs33(q_x) <= {1'b0, cand_pos_tol}) &&
                      (abs33(q_y) <= {1'b0, cand_pos_tol}) &&
                      (abs33(q_z) <= {1'b0, cand_pos_tol}) &&
                      (abs33(u_x) <= {1'b0, cand_ctrl_tol}) &&
                      (abs33(u_y) <= {1'b0, cand_ctrl_tol}) &&
                      (abs33(u_z) <= {1'b0, cand_ctrl_tol});

        chu_fold     = chu_w0 | chu_w1 | chu_w2 | chu_w3;
        next_pre_tag = mix_digest(digest_q, EVT_OPEN_AUTH, VERDICT_OPEN,
                                  witness_pre_id ^ auth_nonce ^ $unsigned(u_x));
    end

    always_ff @(posedge clk) begin
        if (!rst_n) begin
            state_q        <= STATE_HOLD;
            verdict_q      <= VERDICT_HOLD;
            last_nonce_q   <= 32'd0;
            pending_tag_q  <= 32'd0;
            inflight_age_q <= 32'd0;
            history_len_q  <= 32'd1;
            realized_cnt_q <= 32'd0;
            digest_q       <= mix_digest(32'hFFFF_FFFF, EVT_BOOT_HOLD, VERDICT_HOLD, 32'd0);
            latched_u_x_q  <= 32'sd0;
            latched_u_y_q  <= 32'sd0;
            latched_u_z_q  <= 32'sd0;
        end else begin
            // Hard plant or poison breach overrides every state except already-latched KILL
            if (!hard_inside && state_q != STATE_KILL) begin
                state_q        <= STATE_KILL;
                verdict_q      <= VERDICT_KILL;
                pending_tag_q  <= 32'd0;
                inflight_age_q <= 32'd0;
                latched_u_x_q  <= 32'sd0;
                latched_u_y_q  <= 32'sd0;
                latched_u_z_q  <= 32'sd0;
                history_len_q  <= history_len_q + 32'd1;
                digest_q       <= mix_digest(digest_q, EVT_HARD_KILL, VERDICT_KILL, 32'hDEAD_0001);
            end else begin
                case (state_q)
                    STATE_KILL: begin
                        verdict_q     <= VERDICT_KILL;
                        latched_u_x_q <= 32'sd0;
                        latched_u_y_q <= 32'sd0;
                        latched_u_z_q <= 32'sd0;
                        if (req_l01_recover && (l01_key == L01_KEY_MAGIC)) begin
                            state_q       <= STATE_HOLD;
                            verdict_q     <= VERDICT_HOLD;
                            history_len_q <= history_len_q + 32'd1;
                            digest_q      <= mix_digest(digest_q, EVT_L01_RECOVER, VERDICT_HOLD, l01_key[31:0]);
                        end
                    end

                    STATE_UNRESOLVED: begin
                        verdict_q     <= VERDICT_HOLD;
                        latched_u_x_q <= 32'sd0;
                        latched_u_y_q <= 32'sd0;
                        latched_u_z_q <= 32'sd0;
                        if (req_l01_recover && (l01_key == L01_KEY_MAGIC)) begin
                            state_q       <= STATE_HOLD;
                            verdict_q     <= VERDICT_HOLD;
                            history_len_q <= history_len_q + 32'd1;
                            digest_q      <= mix_digest(digest_q, EVT_L01_RECOVER, VERDICT_HOLD, l01_key[31:0]);
                        end
                    end

                    STATE_IN_FLIGHT: begin
                        // Actuator pulse lasts strictly 1 clock cycle; then rail drops to 0 awaiting W_POST
                        latched_u_x_q <= 32'sd0;
                        latched_u_y_q <= 32'sd0;
                        latched_u_z_q <= 32'sd0;
                        verdict_q     <= VERDICT_HOLD;

                        if (power_cut_fault || (inflight_age_q >= MAX_INFLIGHT_CYCLES)) begin
                            state_q        <= STATE_UNRESOLVED;
                            pending_tag_q  <= 32'd0;
                            inflight_age_q <= 32'd0;
                            history_len_q  <= history_len_q + 32'd1;
                            digest_q       <= mix_digest(digest_q, EVT_UNRESOLVED, VERDICT_HOLD, pending_tag_q);
                        end else if (req_post) begin
                            if ((witness_post_id != 32'd0) &&
                                (post_tether_tag == pending_tag_q) &&
                                post_outcome_ok) begin
                                state_q        <= STATE_HOLD;
                                pending_tag_q  <= 32'd0;
                                inflight_age_q <= 32'd0;
                                realized_cnt_q <= realized_cnt_q + 32'd1;
                                history_len_q  <= history_len_q + 32'd1;
                                digest_q       <= mix_digest(digest_q, EVT_POST_REAL, VERDICT_HOLD, witness_post_id);
                            end else begin
                                state_q        <= STATE_UNRESOLVED;
                                pending_tag_q  <= 32'd0;
                                inflight_age_q <= 32'd0;
                                history_len_q  <= history_len_q + 32'd1;
                                digest_q       <= mix_digest(digest_q, EVT_UNRESOLVED, VERDICT_HOLD, post_tether_tag);
                            end
                        end else begin
                            inflight_age_q <= inflight_age_q + 32'd1;
                        end
                    end

                    default: begin // STATE_HOLD
                        latched_u_x_q <= 32'sd0;
                        latched_u_y_q <= 32'sd0;
                        latched_u_z_q <= 32'sd0;
                        verdict_q     <= VERDICT_HOLD;

                        if (req_open) begin
                            if ((seam_defect != 32'sd0) ||
                                (auth_nonce <= last_nonce_q) ||
                                (chu_fold == CHU_CONFLICT)) begin
                                state_q       <= STATE_KILL;
                                verdict_q     <= VERDICT_KILL;
                                history_len_q <= history_len_q + 32'd1;
                                digest_q      <= mix_digest(digest_q, EVT_HARD_KILL, VERDICT_KILL, auth_nonce);
                            end else if ((witness_pre_id != 32'd0) &&
                                         (chu_fold == CHU_VERIFIED) &&
                                         cand_inside) begin
                                state_q        <= STATE_IN_FLIGHT;
                                verdict_q      <= VERDICT_OPEN;
                                last_nonce_q   <= auth_nonce;
                                pending_tag_q  <= next_pre_tag;
                                inflight_age_q <= 32'd0;
                                latched_u_x_q  <= u_x;
                                latched_u_y_q  <= u_y;
                                latched_u_z_q  <= u_z;
                                history_len_q  <= history_len_q + 32'd1;
                                digest_q       <= next_pre_tag;
                            end
                        end
                    end
                endcase
            end
        end
    end

    assign lock_state     = state_q;
    assign verdict_code   = verdict_q;
    assign issued_pre_tag = pending_tag_q;
    assign history_len    = history_len_q;
    assign realized_cnt   = realized_cnt_q;
    assign history_digest = digest_q;

    // Hard combinational clamp: non-zero output requires active OPEN verdict in IN_FLIGHT
    assign out_u_x = ((state_q == STATE_IN_FLIGHT) && (verdict_q == VERDICT_OPEN)) ? latched_u_x_q : 32'sd0;
    assign out_u_y = ((state_q == STATE_IN_FLIGHT) && (verdict_q == VERDICT_OPEN)) ? latched_u_y_q : 32'sd0;
    assign out_u_z = ((state_q == STATE_IN_FLIGHT) && (verdict_q == VERDICT_OPEN)) ? latched_u_z_q : 32'sd0;

endmodule

`default_nettype wire
