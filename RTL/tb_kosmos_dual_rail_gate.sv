// KOSMOS v0.2.1 — Testbench for Dual-Rail, WAL-Before-Pulse, and Sticky-Unresolved
// RTL/tb_kosmos_dual_rail_gate.sv
//
// Exercises all original + Round-2 adversarial hardware invariants:
//   1. Alexandrov ToF 133_425 ps vs 133_426 ps + echo inside window without Chu witness
//   2. Corner trap (80, 80, 10) vs L-inf 100 and 100^2 = 10_000
//   3. WAL-Before-Pulse (T1 FRAM commit at 0 V, T2 pulse): power cut MID-PULSE wakes in
//      STATE_UNRESOLVED with preserved tag and nonce (Round-2 Brudd 3 regression!)
//   4. Simultaneous 00/11 dual-rail fault + RESET_BY_L01 while in STATE_UNRESOLVED:
//      0-cycle crowbar_trip AND FRAM preserves fram_unresolved + fram_pending_tag across KILL;
//      clearing KILL returns to STATE_UNRESOLVED, never straight to STATE_HOLD (Round-2 Brudd 2!)
//
// SPDX-License-Identifier: Apache-2.0

`timescale 1ns / 1ps

module tb_kosmos_dual_rail_gate;

    reg         clk;
    reg         por_rst_n;
    reg         sys_rst_n;

    reg   [3:0] cmd_rail_p;
    reg   [3:0] cmd_rail_n;

    reg  [31:0] tof_meas_ps;
    reg  [31:0] tof_min_ps;
    reg  [31:0] tof_max_ps;
    reg  [31:0] tof_uncert_ps;

    reg  signed [15:0] vec_x, vec_y, vec_z;
    reg         [15:0] linf_limit;
    reg         [63:0] energy_sq_max;

    reg  [31:0] witness_id;
    reg  [31:0] auth_nonce;
    reg   [1:0] chu_w0, chu_w1;
    reg  [31:0] post_tether_tag;
    reg  [31:0] l01_challenge_sig;

    // Simulated non-volatile FRAM cell surviving sys_rst_n power loss
    reg         fram_valid;
    reg   [1:0] fram_state;
    reg         fram_unresolved;
    reg  [31:0] fram_nonce;
    reg  [31:0] fram_hist_len;
    reg  [31:0] fram_digest;
    reg  [31:0] fram_pending_tag;

    wire        nvm_out_valid;
    wire  [1:0] nvm_out_state;
    wire        nvm_out_unresolved;
    wire [31:0] nvm_out_nonce;
    wire [31:0] nvm_out_hist_len;
    wire [31:0] nvm_out_digest;
    wire [31:0] nvm_out_pending_tag;

    wire        crowbar_trip;
    wire        act_enable_p;
    wire        act_enable_n;
    wire  [1:0] lock_state;
    wire        unresolved_latched;
    wire  [7:0] verdict_code;
    wire [31:0] issued_pre_tag;
    wire [31:0] history_len;
    wire [31:0] realized_cnt;
    wire [31:0] history_digest;

    kosmos_dual_rail_gate dut (
        .clk                 (clk),
        .por_rst_n           (por_rst_n),
        .sys_rst_n           (sys_rst_n),
        .cmd_rail_p          (cmd_rail_p),
        .cmd_rail_n          (cmd_rail_n),
        .tof_meas_ps         (tof_meas_ps),
        .tof_min_ps          (tof_min_ps),
        .tof_max_ps          (tof_max_ps),
        .tof_uncert_ps       (tof_uncert_ps),
        .vec_x               (vec_x),
        .vec_y               (vec_y),
        .vec_z               (vec_z),
        .linf_limit          (linf_limit),
        .energy_sq_max       (energy_sq_max),
        .witness_id          (witness_id),
        .auth_nonce          (auth_nonce),
        .chu_w0              (chu_w0),
        .chu_w1              (chu_w1),
        .post_tether_tag     (post_tether_tag),
        .l01_challenge_sig   (l01_challenge_sig),
        .nvm_in_valid        (fram_valid),
        .nvm_in_state        (fram_state),
        .nvm_in_unresolved   (fram_unresolved),
        .nvm_in_nonce        (fram_nonce),
        .nvm_in_hist_len     (fram_hist_len),
        .nvm_in_digest       (fram_digest),
        .nvm_in_pending_tag  (fram_pending_tag),
        .nvm_out_valid       (nvm_out_valid),
        .nvm_out_state       (nvm_out_state),
        .nvm_out_unresolved  (nvm_out_unresolved),
        .nvm_out_nonce       (nvm_out_nonce),
        .nvm_out_hist_len    (nvm_out_hist_len),
        .nvm_out_digest      (nvm_out_digest),
        .nvm_out_pending_tag (nvm_out_pending_tag),
        .crowbar_trip        (crowbar_trip),
        .act_enable_p        (act_enable_p),
        .act_enable_n        (act_enable_n),
        .lock_state          (lock_state),
        .unresolved_latched  (unresolved_latched),
        .verdict_code        (verdict_code),
        .issued_pre_tag      (issued_pre_tag),
        .history_len         (history_len),
        .realized_cnt        (realized_cnt),
        .history_digest      (history_digest)
    );

    always #5 clk = ~clk;

    always @(posedge clk) begin
        if (!por_rst_n) begin
            fram_valid       <= 1'b0;
            fram_state       <= 2'd0;
            fram_unresolved  <= 1'b0;
            fram_nonce       <= 32'd0;
            fram_hist_len    <= 32'd0;
            fram_digest      <= 32'd0;
            fram_pending_tag <= 32'd0;
        end else if (sys_rst_n) begin
            fram_valid       <= nvm_out_valid;
            fram_state       <= nvm_out_state;
            fram_unresolved  <= nvm_out_unresolved;
            fram_nonce       <= nvm_out_nonce;
            fram_hist_len    <= nvm_out_hist_len;
            fram_digest      <= nvm_out_digest;
            fram_pending_tag <= nvm_out_pending_tag;
        end
    end

    function automatic [31:0] compute_l01_sig(
        input [31:0] dig,
        input        unres_bit,
        input  [1:0] st,
        input [31:0] hlen
    );
        reg [31:0] c;
        integer k;
        begin
            c = (dig ^ {29'd0, unres_bit, st}) ^ (32'h4B30_354D ^ hlen);
            for (k = 0; k < 8; k = k + 1) begin
                if ((c & 32'd1) != 32'd0)
                    c = (c >> 1) ^ 32'hEDB8_8320;
                else
                    c = (c >> 1);
            end
            compute_l01_sig = c;
        end
    endfunction

    task automatic set_cmd(input [3:0] p);
        begin
            cmd_rail_p = p;
            cmd_rail_n = ~p;
        end
    endtask

    task automatic fail_now(input [255:0] msg);
        begin
            $fatal(1, "[DUAL-RAIL FAIL] %0s", msg);
        end
    endtask

    reg [31:0] saved_pre_tag;
    reg [31:0] sig_kill_stepdown;
    reg [31:0] sig_unres_clear;
    reg [31:0] len_at_cut;

    initial begin
        clk               = 1'b0;
        por_rst_n         = 1'b0;
        sys_rst_n         = 1'b0;
        set_cmd(4'b0000);
        tof_min_ps        = 32'd133000;
        tof_max_ps        = 32'd133425;
        tof_uncert_ps     = 32'd0;
        tof_meas_ps       = 32'd133425;
        vec_x             = 16'sd50;
        vec_y             = 16'sd50;
        vec_z             = 16'sd10;
        linf_limit        = 16'd100;
        energy_sq_max     = 64'd10000;
        witness_id        = 32'h1234_5678;
        auth_nonce        = 32'd1;
        chu_w0            = 2'b10;
        chu_w1            = 2'b10;
        post_tether_tag   = 32'd0;
        l01_challenge_sig = 32'd0;

        repeat (2) @(posedge clk);
        #1;
        por_rst_n = 1'b1;
        sys_rst_n = 1'b1;
        @(posedge clk);
        #1;

        // 1. ToF boundary (133_426 ps rejected) & echo without Chu witness rejected
        tof_meas_ps = 32'd133426;
        set_cmd(4'b0001);
        @(posedge clk);
        #1;
        if (verdict_code !== 8'h5A || act_enable_p !== 1'b0 || act_enable_n !== 1'b1)
            fail_now("133_426 ps outside ToF window was not trapped");

        tof_meas_ps = 32'd133425;
        chu_w0      = 2'b00;
        chu_w1      = 2'b00;
        @(posedge clk);
        #1;
        if (verdict_code !== 8'h3C || act_enable_p !== 1'b0)
            fail_now("Echo inside ToF window without Chu witness was not held");

        // 2. Corner trap (80, 80, 10) vs 100^2 = 10_000
        chu_w0 = 2'b10;
        chu_w1 = 2'b10;
        vec_x  = 16'sd80;
        vec_y  = 16'sd80;
        vec_z  = 16'sd10;
        @(posedge clk);
        #1;
        if (verdict_code !== 8'h5A || act_enable_p !== 1'b0)
            fail_now("Corner vector (80,80,10) slipped past 100^2 guard");

        // 3. WAL-Before-Pulse + Mid-Pulse Power Cut (Round-2 Brudd 3 regression!)
        vec_x = 16'sd50;
        vec_y = 16'sd50;
        vec_z = 16'sd10;
        @(posedge clk); // Cycle T1: pre-commit to FRAM, act_enable_p MUST still be 0!
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd1 || act_enable_p !== 1'b0 || verdict_code !== 8'h3C)
            fail_now("Cycle T1 violated WAL-Before-Pulse (energized rail before FRAM latch)");
        saved_pre_tag = issued_pre_tag;

        @(posedge clk); // Cycle T2: FRAM is now latched AND act_enable_p == 1 fires!
        #1;
        if (act_enable_p !== 1'b1 || act_enable_n !== 1'b0 || verdict_code !== 8'hA5 ||
            fram_state !== 2'd1 || fram_unresolved !== 1'b1 || fram_pending_tag !== saved_pre_tag)
            fail_now("Cycle T2 failed to latch FRAM simultaneously with pulse");
        len_at_cut = history_len;

        // Cut power MID-PULSE right inside Cycle T2!
        sys_rst_n = 1'b0;
        @(posedge clk);
        #1;
        sys_rst_n = 1'b1;
        @(posedge clk); // FRAM syncs post-boot STATE_UNRESOLVED
        #1;

        if (lock_state !== 2'd2 || unresolved_latched !== 1'b1 ||
            issued_pre_tag !== saved_pre_tag || act_enable_p !== 1'b0 ||
            history_len !== (len_at_cut + 32'd1))
            fail_now("Mid-pulse power cut failed to wake in STATE_UNRESOLVED with tag intact");

        // 4. Simultaneous Dual-Rail 00/11 Fault + Valid L01 Reset while in STATE_UNRESOLVED (Round-2 Brudd 2!)
        sig_unres_clear   = compute_l01_sig(history_digest, unresolved_latched, lock_state, history_len);
        l01_challenge_sig = sig_unres_clear;
        cmd_rail_p        = 4'b1000; // req_l01 = 1
        cmd_rail_n        = 4'b0110; // bit 0 has 00 fault!
        #1; // Check 0-cycle combinational crowbar!
        if (crowbar_trip !== 1'b1 || verdict_code !== 8'h00 || act_enable_p !== 1'b0)
            fail_now("Simultaneous L01 + 00-fault failed 0-cycle crowbar_trip");

        @(posedge clk); // Latches STATE_KILL
        #1;
        set_cmd(4'b0000);
        @(posedge clk); // FRAM updates
        #1;

        // FRAM MUST still preserve fram_unresolved == 1 and fram_pending_tag == saved_pre_tag!
        if (lock_state !== 2'd3 || fram_unresolved !== 1'b1 || fram_pending_tag !== saved_pre_tag)
            fail_now("Dual-rail fault wiped fram_unresolved or fram_pending_tag in FRAM");

        // L01 Recovery #1 from STATE_KILL MUST step down to STATE_UNRESOLVED (2), NOT STATE_HOLD (0)!
        sig_kill_stepdown = compute_l01_sig(history_digest, unresolved_latched, lock_state, history_len);
        l01_challenge_sig = sig_kill_stepdown;
        set_cmd(4'b1000);
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd2 || unresolved_latched !== 1'b1 || issued_pre_tag !== saved_pre_tag)
            fail_now("Clearing KILL bypassed STATE_UNRESOLVED instead of stepping down");

        // Replaying sig_kill_stepdown MUST fail; dedicated L01 Recovery #2 clears STATE_UNRESOLVED -> STATE_HOLD
        set_cmd(4'b1000);
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd2)
            fail_now("Replayed KILL signature wrongly cleared STATE_UNRESOLVED");

        sig_unres_clear   = compute_l01_sig(history_digest, unresolved_latched, lock_state, history_len);
        l01_challenge_sig = sig_unres_clear;
        set_cmd(4'b1000);
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd0 || unresolved_latched !== 1'b0 || issued_pre_tag !== 32'd0)
            fail_now("Dedicated second L01 signature failed to clear STATE_UNRESOLVED to STATE_HOLD");

        $display("[PASS] RTL kosmos_dual_rail_gate v0.2.1: WAL-before-pulse, sticky FRAM UNRESOLVED across KILL, ToF, corner, and 0-cycle crowbar verified (hist=%0d).",
                 history_len);
        $finish(0);
    end

endmodule
