// KOSMOS v0.2.1 — Dual-Rail, WAL-Before-Pulse, Sticky-Unresolved Gate
// RTL/kosmos_dual_rail_gate.sv : Synthesizable IEEE 1800-2012 RTL (0 float)
//
// Hardened against Round-2 adversarial findings:
//   1. Complementary 4-lane dual-rail bus ((rail_p ^ rail_n) == 4'hF; 00/11 -> 0-cycle crowbar_trip)
//   2. Two-cycle WAL-Before-Pulse: Cycle T1 commits IN_FLIGHT + tag + nonce to FRAM at 0.00 V;
//      Cycle T2 fires the 1-cycle actuator pulse ONLY after FRAM is latched.
//   3. Sticky unresolved_latched_q + preserved pending_tag_q across STATE_KILL so a dual-rail
//      fault never wipes STATE_UNRESOLVED or its forensic tag from FRAM; clearing KILL steps
//      down to STATE_UNRESOLVED first when an earlier PRE lacked POST.
//   4. Calibrated Alexandrov picosecond ToF window & 64-bit corner guard ((80,80,10) > 100^2).
//
// SPDX-License-Identifier: Apache-2.0

`timescale 1ns / 1ps
`default_nettype none

module kosmos_dual_rail_gate (
    input  wire         clk,
    input  wire         por_rst_n,
    input  wire         sys_rst_n,

    // 4-bit complementary dual-rail command bus:
    //   bit 0 = req_open, bit 1 = req_post, bit 2 = post_ok, bit 3 = req_l01
    input  wire   [3:0] cmd_rail_p,
    input  wire   [3:0] cmd_rail_n,

    // Alexandrov picosecond Time-of-Flight inputs
    input  wire  [31:0] tof_meas_ps,
    input  wire  [31:0] tof_min_ps,
    input  wire  [31:0] tof_max_ps,
    input  wire  [31:0] tof_uncert_ps,

    // 16-bit signed 3D vector & 64-bit exact quadratic energy ceiling
    input  wire  signed [15:0] vec_x,
    input  wire  signed [15:0] vec_y,
    input  wire  signed [15:0] vec_z,
    input  wire         [15:0] linf_limit,
    input  wire         [63:0] energy_sq_max,

    // Witness & challenge-bound L01 recovery inputs
    input  wire  [31:0] witness_id,
    input  wire  [31:0] auth_nonce,
    input  wire   [1:0] chu_w0,
    input  wire   [1:0] chu_w1,
    input  wire  [31:0] post_tether_tag,
    input  wire  [31:0] l01_challenge_sig,

    // Non-volatile FRAM/MRAM shadow readback inputs
    input  wire         nvm_in_valid,
    input  wire   [1:0] nvm_in_state,
    input  wire         nvm_in_unresolved,
    input  wire  [31:0] nvm_in_nonce,
    input  wire  [31:0] nvm_in_hist_len,
    input  wire  [31:0] nvm_in_digest,
    input  wire  [31:0] nvm_in_pending_tag,

    // Non-volatile shadow write outputs
    output logic        nvm_out_valid,
    output logic  [1:0] nvm_out_state,
    output logic        nvm_out_unresolved,
    output logic [31:0] nvm_out_nonce,
    output logic [31:0] nvm_out_hist_len,
    output logic [31:0] nvm_out_digest,
    output logic [31:0] nvm_out_pending_tag,

    // Dual-rail actuator & crowbar outputs
    output logic        crowbar_trip,
    output logic        act_enable_p,
    output logic        act_enable_n,
    output logic  [1:0] lock_state,
    output logic        unresolved_latched,
    output logic  [7:0] verdict_code,
    output logic [31:0] issued_pre_tag,
    output logic [31:0] history_len,
    output logic [31:0] realized_cnt,
    output logic [31:0] history_digest
);

    localparam [1:0] STATE_HOLD       = 2'd0;
    localparam [1:0] STATE_IN_FLIGHT  = 2'd1;
    localparam [1:0] STATE_UNRESOLVED = 2'd2;
    localparam [1:0] STATE_KILL       = 2'd3;

    localparam [7:0] VERDICT_KILL     = 8'h00;
    localparam [7:0] VERDICT_HOLD     = 8'h3C;
    localparam [7:0] VERDICT_TRAP     = 8'h5A;
    localparam [7:0] VERDICT_OPEN     = 8'hA5;

    localparam [1:0] CHU_VERIFIED     = 2'b10;
    localparam [1:0] CHU_CONFLICT     = 2'b11;

    localparam signed [15:0] POISON_I16_MIN = 16'sh8000;
    localparam        [31:0] L01_SECRET_KEY = 32'h4B30_354D;

    function automatic [15:0] abs16(input signed [15:0] v);
        reg signed [16:0] ext;
        begin
            ext   = v;
            abs16 = (ext < 17'sd0) ? $unsigned(-ext) : $unsigned(ext);
        end
    endfunction

    function automatic [31:0] crc_step(input [31:0] prev, input [31:0] word);
        reg [31:0] c;
        integer k;
        begin
            c = prev ^ word;
            for (k = 0; k < 8; k = k + 1) begin
                if ((c & 32'd1) != 32'd0)
                    c = (c >> 1) ^ 32'hEDB8_8320;
                else
                    c = (c >> 1);
            end
            crc_step = c;
        end
    endfunction

    logic  [1:0] state_q;
    logic        unresolved_latched_q;
    logic        commit_armed_q;
    logic        pulse_active_q;
    logic  [7:0] verdict_q;
    logic [31:0] last_nonce_q;
    logic [31:0] pending_tag_q;
    logic [31:0] hist_len_q;
    logic [31:0] realized_q;
    logic [31:0] digest_q;

    logic        rail_ok;
    logic        req_open;
    logic        req_post;
    logic        post_ok;
    logic        req_l01;
    logic        poison_vec;
    logic        linf_ok;
    logic [63:0] sq_sum;
    logic        corner_ok;
    logic        tof_win_valid;
    logic [32:0] eff_tof_min;
    logic [32:0] eff_tof_max;
    logic        tof_ok;
    logic  [1:0] chu_fold;
    logic [31:0] expected_l01_sig;
    logic [31:0] next_tag;

    always_comb begin
        rail_ok  = ((cmd_rail_p ^ cmd_rail_n) == 4'hF);
        req_open = rail_ok && ((cmd_rail_p & 4'b0001) != 4'b0000);
        req_post = rail_ok && ((cmd_rail_p & 4'b0010) != 4'b0000);
        post_ok  = rail_ok && ((cmd_rail_p & 4'b0100) != 4'b0000);
        req_l01  = rail_ok && ((cmd_rail_p & 4'b1000) != 4'b0000);

        poison_vec = (vec_x == POISON_I16_MIN) ||
                     (vec_y == POISON_I16_MIN) ||
                     (vec_z == POISON_I16_MIN);

        linf_ok = (!poison_vec) && (linf_limit > 16'd0) &&
                  (abs16(vec_x) <= linf_limit) &&
                  (abs16(vec_y) <= linf_limit) &&
                  (abs16(vec_z) <= linf_limit);

        sq_sum = ({48'd0, abs16(vec_x)} * {48'd0, abs16(vec_x)}) +
                 ({48'd0, abs16(vec_y)} * {48'd0, abs16(vec_y)}) +
                 ({48'd0, abs16(vec_z)} * {48'd0, abs16(vec_z)});
        corner_ok = (!poison_vec) && (sq_sum <= energy_sq_max);

        eff_tof_min   = {1'b0, tof_min_ps} + {1'b0, tof_uncert_ps};
        eff_tof_max   = (tof_max_ps >= tof_uncert_ps)
                            ? {1'b0, (tof_max_ps - tof_uncert_ps)}
                            : 33'd0;
        tof_win_valid = (tof_max_ps >= tof_uncert_ps) && (eff_tof_min <= eff_tof_max);
        tof_ok        = tof_win_valid &&
                        ({1'b0, tof_meas_ps} >= eff_tof_min) &&
                        ({1'b0, tof_meas_ps} <= eff_tof_max);

        chu_fold         = chu_w0 | chu_w1;
        expected_l01_sig = crc_step(digest_q ^ {29'd0, unresolved_latched_q, state_q},
                                    L01_SECRET_KEY ^ hist_len_q);
        next_tag         = crc_step(digest_q, witness_id ^ auth_nonce ^ tof_meas_ps);
    end

    always_ff @(posedge clk) begin
        if (!por_rst_n) begin
            state_q              <= STATE_HOLD;
            unresolved_latched_q <= 1'b0;
            commit_armed_q       <= 1'b0;
            pulse_active_q       <= 1'b0;
            verdict_q            <= VERDICT_HOLD;
            last_nonce_q         <= 32'd0;
            pending_tag_q        <= 32'd0;
            hist_len_q           <= 32'd1;
            realized_q           <= 32'd0;
            digest_q             <= 32'hA53C_0001;
        end else if (!sys_rst_n) begin
            commit_armed_q <= 1'b0;
            pulse_active_q <= 1'b0;
            if (nvm_in_valid) begin
                last_nonce_q  <= nvm_in_nonce;
                pending_tag_q <= nvm_in_pending_tag;
                if (nvm_in_state == STATE_KILL) begin
                    state_q              <= STATE_KILL;
                    unresolved_latched_q <= nvm_in_unresolved;
                    verdict_q            <= VERDICT_KILL;
                    hist_len_q           <= nvm_in_hist_len;
                    digest_q             <= nvm_in_digest;
                end else if ((nvm_in_state == STATE_IN_FLIGHT) ||
                             (nvm_in_state == STATE_UNRESOLVED) ||
                             nvm_in_unresolved) begin
                    state_q              <= STATE_UNRESOLVED;
                    unresolved_latched_q <= 1'b1;
                    verdict_q            <= VERDICT_HOLD;
                    hist_len_q           <= (nvm_in_state == STATE_IN_FLIGHT)
                                                ? (nvm_in_hist_len + 32'd1)
                                                : nvm_in_hist_len;
                    digest_q             <= (nvm_in_state == STATE_IN_FLIGHT)
                                                ? crc_step(nvm_in_digest, 32'hDEAD_BEEF ^ nvm_in_pending_tag)
                                                : nvm_in_digest;
                end else begin
                    state_q              <= STATE_HOLD;
                    unresolved_latched_q <= 1'b0;
                    pending_tag_q        <= 32'd0;
                    verdict_q            <= VERDICT_HOLD;
                    hist_len_q           <= nvm_in_hist_len;
                    digest_q             <= nvm_in_digest;
                end
            end else begin
                state_q              <= STATE_HOLD;
                unresolved_latched_q <= 1'b0;
                verdict_q            <= VERDICT_HOLD;
                last_nonce_q         <= 32'd0;
                pending_tag_q        <= 32'd0;
                hist_len_q           <= 32'd1;
                digest_q             <= 32'hA53C_0001;
            end
        end else begin
            pulse_active_q <= 1'b0;

            // Dual-rail symmetry breach or poison vector -> immediate latched KILL
            // WITHOUT wiping unresolved_latched_q or pending_tag_q!
            if (!rail_ok || poison_vec || (chu_fold == CHU_CONFLICT) ||
                (commit_armed_q && (!linf_ok || !corner_ok || !tof_ok || (chu_fold != CHU_VERIFIED)))) begin
                commit_armed_q <= 1'b0;
                pulse_active_q <= 1'b0;
                if ((state_q == STATE_IN_FLIGHT) || (state_q == STATE_UNRESOLVED)) begin
                    unresolved_latched_q <= 1'b1;
                end
                if (state_q != STATE_KILL) begin
                    state_q    <= STATE_KILL;
                    verdict_q  <= VERDICT_KILL;
                    hist_len_q <= hist_len_q + 32'd1;
                    digest_q   <= crc_step(digest_q, 32'hFA11_0011);
                end
            end else begin
                case (state_q)
                    STATE_KILL: begin
                        verdict_q <= VERDICT_KILL;
                        if (req_l01 && (l01_challenge_sig == expected_l01_sig)) begin
                            hist_len_q <= hist_len_q + 32'd1;
                            digest_q   <= crc_step(digest_q, l01_challenge_sig);
                            if (unresolved_latched_q) begin
                                // Step down from KILL -> STATE_UNRESOLVED, keeping pending_tag_q intact!
                                state_q   <= STATE_UNRESOLVED;
                                verdict_q <= VERDICT_HOLD;
                            end else begin
                                state_q       <= STATE_HOLD;
                                pending_tag_q <= 32'd0;
                                verdict_q     <= VERDICT_HOLD;
                            end
                        end
                    end

                    STATE_UNRESOLVED: begin
                        verdict_q <= VERDICT_HOLD;
                        if (req_l01 && (l01_challenge_sig == expected_l01_sig)) begin
                            state_q              <= STATE_HOLD;
                            unresolved_latched_q <= 1'b0;
                            pending_tag_q        <= 32'd0;
                            verdict_q            <= VERDICT_HOLD;
                            hist_len_q           <= hist_len_q + 32'd1;
                            digest_q             <= crc_step(digest_q, l01_challenge_sig);
                        end
                    end

                    STATE_IN_FLIGHT: begin
                        if (commit_armed_q) begin
                            // Cycle T2: FRAM latches STATE_IN_FLIGHT on this clock edge; now fire 1-cycle pulse!
                            commit_armed_q <= 1'b0;
                            pulse_active_q <= 1'b1;
                            verdict_q      <= VERDICT_OPEN;
                        end else begin
                            verdict_q <= VERDICT_HOLD;
                            if (req_post) begin
                                if ((witness_id != 32'd0) &&
                                    (post_tether_tag == pending_tag_q) &&
                                    post_ok) begin
                                    state_q              <= STATE_HOLD;
                                    unresolved_latched_q <= 1'b0;
                                    pending_tag_q        <= 32'd0;
                                    realized_q           <= realized_q + 32'd1;
                                    hist_len_q           <= hist_len_q + 32'd1;
                                    digest_q             <= crc_step(digest_q, witness_id ^ post_tether_tag);
                                end else begin
                                    state_q              <= STATE_UNRESOLVED;
                                    unresolved_latched_q <= 1'b1;
                                    hist_len_q           <= hist_len_q + 32'd1;
                                    digest_q             <= crc_step(digest_q, 32'hBAD0_9057);
                                end
                            end
                        end
                    end

                    default: begin // STATE_HOLD
                        verdict_q <= VERDICT_HOLD;
                        if (req_open) begin
                            if ((auth_nonce <= last_nonce_q) || (chu_fold == CHU_CONFLICT)) begin
                                state_q    <= STATE_KILL;
                                verdict_q  <= VERDICT_KILL;
                                hist_len_q <= hist_len_q + 32'd1;
                                digest_q   <= crc_step(digest_q, auth_nonce);
                            end else if (!tof_ok || !linf_ok || !corner_ok) begin
                                verdict_q <= VERDICT_TRAP;
                            end else if ((witness_id != 32'd0) && (chu_fold == CHU_VERIFIED)) begin
                                // Cycle T1 (WAL-Before-Pulse): present IN_FLIGHT + tag to FRAM at 0.00 V!
                                state_q              <= STATE_IN_FLIGHT;
                                unresolved_latched_q <= 1'b1;
                                commit_armed_q       <= 1'b1;
                                pulse_active_q       <= 1'b0;
                                verdict_q            <= VERDICT_HOLD;
                                last_nonce_q         <= auth_nonce;
                                pending_tag_q        <= next_tag;
                                hist_len_q           <= hist_len_q + 32'd1;
                                digest_q             <= next_tag;
                            end
                        end
                    end
                endcase
            end
        end
    end

    // Non-volatile FRAM shadow writeback bus
    assign nvm_out_valid       = 1'b1;
    assign nvm_out_state       = state_q;
    assign nvm_out_unresolved  = unresolved_latched_q || (state_q == STATE_IN_FLIGHT) || (state_q == STATE_UNRESOLVED);
    assign nvm_out_nonce       = last_nonce_q;
    assign nvm_out_hist_len    = hist_len_q;
    assign nvm_out_digest      = digest_q;
    assign nvm_out_pending_tag = pending_tag_q;

    // 0-cycle combinational crowbar trip on dual-rail fault, poison, or latched KILL
    assign crowbar_trip       = (!rail_ok) || poison_vec || (chu_fold == CHU_CONFLICT) || (state_q == STATE_KILL) ||
                                ((commit_armed_q || pulse_active_q) && (!linf_ok || !corner_ok || !tof_ok || (chu_fold != CHU_VERIFIED)));
    assign act_enable_p       = (!crowbar_trip) && pulse_active_q && (state_q == STATE_IN_FLIGHT) && (verdict_q == VERDICT_OPEN);
    assign act_enable_n       = ~act_enable_p;

    assign lock_state         = state_q;
    assign unresolved_latched = unresolved_latched_q;
    assign verdict_code       = crowbar_trip ? VERDICT_KILL : verdict_q;
    assign issued_pre_tag     = pending_tag_q;
    assign history_len        = hist_len_q;
    assign realized_cnt       = realized_q;
    assign history_digest     = digest_q;

endmodule

`default_nettype wire
