// KOSMOS v0.3 — Universal Physics & Consequence Gate (SPEC/09..12)
// RTL/kosmos_v03_physics_gate.sv : Synthesizable IEEE 1800-2012 RTL (0 float)
//
// Unifies:
//   - Complementary 4-lane dual-rail bus (0-cycle crowbar_trip on 00/11)
//   - 3D 7-point integer Laplacian stencil unit & Faraday-Lenz sign lock
//   - Newtonian local inertia (dt > 0, |F| <= F_max, doubled energy & Right to Rest)
//   - Pairwise 33-bit non-wrapping 3rd-Law check (F_AB + F_BA == 0) & strict Nash lock
//   - Minkowski lightcone (dr_sq <= ct_sq), airgap work check, entropy ceiling & Godel bound
//   - Two-cycle FRAM WAL-before-pulse & sticky unresolved_latched across KILL
//
// SPDX-License-Identifier: Apache-2.0

`timescale 1ns / 1ps
`default_nettype none

module kosmos_v03_physics_gate (
    input  wire                clk,
    input  wire                por_rst_n,
    input  wire                sys_rst_n,

    // 4-lane complementary dual-rail bus (bit0=req_open, bit1=req_post, bit2=post_ok, bit3=req_l01)
    input  wire          [3:0] rail_p,
    input  wire          [3:0] rail_n,

    // Causal horizon, airgap, and Godel witness inputs (SPEC/12)
    input  wire                airgap_sealed,
    input  wire                ancestry_valid,
    input  wire                unmerged_fork,
    input  wire                undecidable_hit,
    input  wire                has_ext_witness,
    input  wire         [31:0] dt_ticks,
    input  wire         [31:0] formal_iters,
    input  wire         [31:0] max_iters,
    input  wire         [63:0] w_lift_work,
    input  wire         [63:0] dr_sq,
    input  wire         [63:0] ct_sq,
    input  wire         [63:0] s_dot_gen,
    input  wire         [63:0] s_dot_sink,

    // Faraday-Lenz (SPEC/09), Newtonian inertia (SPEC/10), and Nash reciprocity (SPEC/11)
    input  wire  signed [31:0] dphi_b,
    input  wire  signed [31:0] emf_induced,
    input  wire  signed [31:0] emf_max,
    input  wire  signed [31:0] force_in,
    input  wire  signed [31:0] force_max,
    input  wire         [63:0] energy_rem_2x,
    input  wire         [63:0] w_min_2x,
    input  wire         [63:0] energy_cost_2x,
    input  wire  signed [31:0] f_ab,
    input  wire  signed [31:0] f_ba,
    input  wire  signed [31:0] nash_res_abs_2x,
    input  wire  signed [31:0] nash_c_ii,

    // Inline 3D 7-point integer Laplacian co-unit inputs (SPEC/09)
    input  wire  signed [31:0] f_center,
    input  wire  signed [31:0] f_xp,
    input  wire  signed [31:0] f_xm,
    input  wire  signed [31:0] f_yp,
    input  wire  signed [31:0] f_ym,
    input  wire  signed [31:0] f_zp,
    input  wire  signed [31:0] f_zm,

    // Witness & NVRAM shadow interface
    input  wire         [31:0] witness_id,
    input  wire         [31:0] auth_nonce,
    input  wire         [31:0] post_tether_tag,
    input  wire         [31:0] l01_challenge_sig,
    input  wire                nvm_in_valid,
    input  wire          [1:0] nvm_in_state,
    input  wire                nvm_in_unresolved,
    input  wire         [31:0] nvm_in_nonce,
    input  wire         [31:0] nvm_in_hist_len,
    input  wire         [31:0] nvm_in_digest,
    input  wire         [31:0] nvm_in_pending_tag,

    output logic               nvm_out_valid,
    output logic         [1:0] nvm_out_state,
    output logic               nvm_out_unresolved,
    output logic        [31:0] nvm_out_nonce,
    output logic        [31:0] nvm_out_hist_len,
    output logic        [31:0] nvm_out_digest,
    output logic        [31:0] nvm_out_pending_tag,

    // Combinational & registered outputs
    output logic signed [35:0] laplacian7_out,
    output logic         [7:0] comb_verdict,
    output logic               crowbar_trip,
    output logic               act_enable_p,
    output logic               act_enable_n,
    output logic         [1:0] lock_state,
    output logic               unresolved_latched,
    output logic         [7:0] verdict_code,
    output logic        [31:0] issued_pre_tag,
    output logic        [31:0] history_len,
    output logic        [31:0] realized_cnt,
    output logic        [31:0] history_digest
);

    localparam [1:0] STATE_HOLD       = 2'd0;
    localparam [1:0] STATE_IN_FLIGHT  = 2'd1;
    localparam [1:0] STATE_UNRESOLVED = 2'd2;
    localparam [1:0] STATE_KILL       = 2'd3;

    localparam [7:0] VERDICT_KILL     = 8'h00;
    localparam [7:0] VERDICT_HOLD     = 8'h3C;
    localparam [7:0] VERDICT_TRAP     = 8'h5A;
    localparam [7:0] VERDICT_OPEN     = 8'hA5;

    localparam signed [31:0] POISON_I32_MIN = 32'sh8000_0000;
    localparam        [31:0] L01_SECRET_KEY = 32'h4B30_354D;

    function automatic [31:0] abs32_safe(input signed [31:0] v);
        reg signed [32:0] ext;
        begin
            ext        = v;
            abs32_safe = (ext < 33'sd0) ? $unsigned(-ext) : $unsigned(ext);
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

    logic               rail_ok;
    logic               req_open;
    logic               req_post;
    logic               post_ok;
    logic               req_l01;
    logic               any_poison;
    logic               lenz_violation;
    logic        [31:0] abs_emf;
    logic        [31:0] abs_force;
    logic signed [32:0] f_pair_sum;
    logic signed [35:0] nbr_sum;
    logic        [31:0] expected_l01_sig;
    logic        [31:0] next_tag;

    always_comb begin
        rail_ok  = ((rail_p ^ rail_n) == 4'hF);
        req_open = rail_ok && ((rail_p & 4'b0001) != 4'b0000);
        req_post = rail_ok && ((rail_p & 4'b0010) != 4'b0000);
        post_ok  = rail_ok && ((rail_p & 4'b0100) != 4'b0000);
        req_l01  = rail_ok && ((rail_p & 4'b1000) != 4'b0000);

        any_poison = (dphi_b == POISON_I32_MIN) ||
                     (emf_induced == POISON_I32_MIN) ||
                     (emf_max == POISON_I32_MIN) ||
                     (force_in == POISON_I32_MIN) ||
                     (force_max == POISON_I32_MIN) ||
                     (f_ab == POISON_I32_MIN) ||
                     (f_ba == POISON_I32_MIN) ||
                     (nash_res_abs_2x == POISON_I32_MIN) ||
                     (nash_c_ii == POISON_I32_MIN);

        lenz_violation = ((emf_induced > 32'sd0) && (dphi_b > 32'sd0)) ||
                         ((emf_induced < 32'sd0) && (dphi_b < 32'sd0));

        abs_emf    = abs32_safe(emf_induced);
        abs_force  = abs32_safe(force_in);
        f_pair_sum = $signed(f_ab) + $signed(f_ba);

        // Exact 36-bit widened 7-point integer Laplacian (never overflows 32-bit inputs)
        nbr_sum        = $signed(f_xp) + $signed(f_xm) +
                         $signed(f_yp) + $signed(f_ym) +
                         $signed(f_zp) + $signed(f_zm);
        laplacian7_out = nbr_sum - (36'sd6 * $signed(f_center));

        // Bit-exact combinational priority chain matching kosmos_eval_v03_rtl_twin()
        if (!rail_ok || any_poison ||
            (dt_ticks == 32'd0) ||
            (emf_max <= 32'sd0) ||
            (force_max <= 32'sd0) ||
            (nash_c_ii <= 32'sd0) ||
            (nash_res_abs_2x < 32'sd0) ||
            lenz_violation ||
            (dr_sq > ct_sq) ||
            (s_dot_gen > s_dot_sink) ||
            (!airgap_sealed && ancestry_valid && (w_lift_work == 64'd0))) begin
            comb_verdict = VERDICT_KILL;
        end else if ((abs_emf > $unsigned(emf_max)) ||
                     (abs_force > $unsigned(force_max)) ||
                     (f_pair_sum != 33'sd0) ||
                     ((formal_iters > max_iters) && !has_ext_witness)) begin
            comb_verdict = VERDICT_TRAP;
        end else if (!ancestry_valid ||
                     unmerged_fork ||
                     (energy_rem_2x < w_min_2x) ||
                     (energy_rem_2x < energy_cost_2x) ||
                     (nash_res_abs_2x >= nash_c_ii) ||
                     (undecidable_hit && !has_ext_witness)) begin
            comb_verdict = VERDICT_HOLD;
        end else begin
            comb_verdict = VERDICT_OPEN;
        end

        expected_l01_sig = crc_step(digest_q ^ {29'd0, unresolved_latched_q, state_q},
                                    L01_SECRET_KEY ^ hist_len_q);
        next_tag         = crc_step(digest_q, witness_id ^ auth_nonce ^ dt_ticks);
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
            digest_q             <= 32'hA53C_0003;
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
                digest_q             <= 32'hA53C_0003;
            end
        end else begin
            pulse_active_q <= 1'b0;

            if (!rail_ok || any_poison ||
                ((comb_verdict == VERDICT_KILL) && (req_open || commit_armed_q))) begin
                commit_armed_q <= 1'b0;
                if ((state_q == STATE_IN_FLIGHT) || (state_q == STATE_UNRESOLVED)) begin
                    unresolved_latched_q <= 1'b1;
                end
                if (state_q != STATE_KILL) begin
                    state_q    <= STATE_KILL;
                    verdict_q  <= VERDICT_KILL;
                    hist_len_q <= hist_len_q + 32'd1;
                    digest_q   <= crc_step(digest_q, 32'hFA11_0003);
                end
            end else begin
                case (state_q)
                    STATE_KILL: begin
                        verdict_q <= VERDICT_KILL;
                        if (req_l01 && (l01_challenge_sig == expected_l01_sig)) begin
                            hist_len_q <= hist_len_q + 32'd1;
                            digest_q   <= crc_step(digest_q, l01_challenge_sig);
                            if (unresolved_latched_q) begin
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
                            commit_armed_q <= 1'b0;
                            // Recheck physics at T2 before issuing the armed pulse.
                            verdict_q <= comb_verdict;
                            if (comb_verdict == VERDICT_OPEN) begin
                                pulse_active_q <= 1'b1;
                            end else begin
                                state_q <= STATE_UNRESOLVED;
                            end
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
                                    digest_q             <= crc_step(digest_q, 32'hBAD0_0003);
                                end
                            end
                        end
                    end

                    default: begin // STATE_HOLD
                        verdict_q <= VERDICT_HOLD;
                        if (req_open) begin
                            if (auth_nonce <= last_nonce_q) begin
                                state_q    <= STATE_KILL;
                                verdict_q  <= VERDICT_KILL;
                                hist_len_q <= hist_len_q + 32'd1;
                                digest_q   <= crc_step(digest_q, auth_nonce);
                            end else if (comb_verdict == VERDICT_TRAP) begin
                                verdict_q <= VERDICT_TRAP;
                            end else if ((comb_verdict == VERDICT_OPEN) && (witness_id != 32'd0)) begin
                                // Cycle T1: FRAM pre-commit at 0.00 V before Cycle T2 pulse!
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

    assign nvm_out_valid       = 1'b1;
    assign nvm_out_state       = state_q;
    assign nvm_out_unresolved  = unresolved_latched_q || (state_q == STATE_IN_FLIGHT) || (state_q == STATE_UNRESOLVED);
    assign nvm_out_nonce       = last_nonce_q;
    assign nvm_out_hist_len    = hist_len_q;
    assign nvm_out_digest      = digest_q;
    assign nvm_out_pending_tag = pending_tag_q;

    assign crowbar_trip       = (!rail_ok) || any_poison || (state_q == STATE_KILL);
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
