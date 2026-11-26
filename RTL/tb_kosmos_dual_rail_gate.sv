// KOSMOS v0.2 — Testbench for Dual-Rail, Cold-Boot, ToF, and Corner Guard
// RTL/tb_kosmos_dual_rail_gate.sv
//
// Exercises:
//   1. Cold-boot power cut between PRE and POST -> wakes in UNRESOLVED, blocks OPEN,
//      preserves unresolved tag, and requires non-replayable challenge-bound L01 sig
//   2. Alexandrov ToF 133_425 ps vs 133_426 ps + echo inside window without Chu witness
//   3. Corner trap (80, 80, 10) vs L-inf 100 and 100^2 = 10_000
//   4. 0-cycle combinational crowbar_trip on dual-rail 00 / 11 symmetry fault
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
    reg  [31:0] fram_nonce;
    reg  [31:0] fram_hist_len;
    reg  [31:0] fram_digest;
    reg  [31:0] fram_pending_tag;

    wire        nvm_out_valid;
    wire  [1:0] nvm_out_state;
    wire [31:0] nvm_out_nonce;
    wire [31:0] nvm_out_hist_len;
    wire [31:0] nvm_out_digest;
    wire [31:0] nvm_out_pending_tag;

    wire        crowbar_trip;
    wire        act_enable_p;
    wire        act_enable_n;
    wire  [1:0] lock_state;
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
        .nvm_in_nonce        (fram_nonce),
        .nvm_in_hist_len     (fram_hist_len),
        .nvm_in_digest       (fram_digest),
        .nvm_in_pending_tag  (fram_pending_tag),
        .nvm_out_valid       (nvm_out_valid),
        .nvm_out_state       (nvm_out_state),
        .nvm_out_nonce       (nvm_out_nonce),
        .nvm_out_hist_len    (nvm_out_hist_len),
        .nvm_out_digest      (nvm_out_digest),
        .nvm_out_pending_tag (nvm_out_pending_tag),
        .crowbar_trip        (crowbar_trip),
        .act_enable_p        (act_enable_p),
        .act_enable_n        (act_enable_n),
        .lock_state          (lock_state),
        .verdict_code        (verdict_code),
        .issued_pre_tag      (issued_pre_tag),
        .history_len         (history_len),
        .realized_cnt        (realized_cnt),
        .history_digest      (history_digest)
    );

    always #5 clk = ~clk;

    // FRAM latches shadow whenever power is up (sys_rst_n == 1)
    always @(posedge clk) begin
        if (!por_rst_n) begin
            fram_valid       <= 1'b0;
            fram_state       <= 2'd0;
            fram_nonce       <= 32'd0;
            fram_hist_len    <= 32'd0;
            fram_digest      <= 32'd0;
            fram_pending_tag <= 32'd0;
        end else if (sys_rst_n) begin
            fram_valid       <= nvm_out_valid;
            fram_state       <= nvm_out_state;
            fram_nonce       <= nvm_out_nonce;
            fram_hist_len    <= nvm_out_hist_len;
            fram_digest      <= nvm_out_digest;
            fram_pending_tag <= nvm_out_pending_tag;
        end
    end

    function automatic [31:0] compute_l01_sig(
        input [31:0] dig,
        input  [1:0] st,
        input [31:0] hlen
    );
        reg [31:0] c;
        integer k;
        begin
            c = (dig ^ {30'd0, st}) ^ (32'h4B30_354D ^ hlen);
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
            $display("[DUAL-RAIL FAIL] %0s", msg);
            $finish(1);
        end
    endtask

    reg [31:0] saved_pre_tag;
    reg [31:0] used_sig;
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
        por_rst_n = 1'b1;
        sys_rst_n = 1'b1;
        @(posedge clk);
        #1;

        // 1. ToF boundary: 133_426 ps rejected as TRAP (0x5A); echo at 133_425 ps without Chu rejected
        tof_meas_ps = 32'd133426;
        set_cmd(4'b0001); // req_open
        @(posedge clk);
        #1;
        if (verdict_code !== 8'h5A || act_enable_p !== 1'b0 || act_enable_n !== 1'b1)
            fail_now("133_426 ps outside ToF window was not trapped");

        tof_meas_ps = 32'd133425;
        chu_w0      = 2'b00;
        chu_w1      = 2'b00; // Echo inside ToF window, but no Chu witness!
        @(posedge clk);
        #1;
        if (verdict_code !== 8'h3C || act_enable_p !== 1'b0)
            fail_now("Echo inside ToF window without Chu witness was not held");

        // 2. Corner trap (80, 80, 10): passes L-inf <= 100, fails 80^2+80^2+10^2 = 12900 > 10000
        chu_w0 = 2'b10;
        chu_w1 = 2'b10;
        vec_x  = 16'sd80;
        vec_y  = 16'sd80;
        vec_z  = 16'sd10;
        @(posedge clk);
        #1;
        if (verdict_code !== 8'h5A || act_enable_p !== 1'b0)
            fail_now("Corner vector (80,80,10) slipped past 100^2 guard");

        // 3. Valid PRE (50, 50, 10 -> 5100 <= 10000) -> OPEN (IN_FLIGHT), then COLD-BOOT POWER LOSS!
        vec_x = 16'sd50;
        vec_y = 16'sd50;
        vec_z = 16'sd10;
        @(posedge clk); // Cycle of OPEN; FRAM captures IN_FLIGHT on next posedge
        #1;
        if (verdict_code !== 8'hA5 || lock_state !== 2'd1 || act_enable_p !== 1'b1 || act_enable_n !== 1'b0)
            fail_now("Valid PRE did not issue dual-rail OPEN");
        saved_pre_tag = issued_pre_tag;
        set_cmd(4'b0000);
        @(posedge clk); // FRAM latches STATE_IN_FLIGHT + saved_pre_tag
        #1;
        len_at_cut = history_len;

        // Cut power (sys_rst_n = 0) BEFORE W_POST!
        sys_rst_n = 1'b0;
        @(posedge clk);
        #1;
        sys_rst_n = 1'b1;
        #1;

        // Acceptance proof: after cold reboot, output is OFF, state is UNRESOLVED,
        // interrupted tag is preserved, history_len incremented, and new OPEN is blocked!
        if (lock_state !== 2'd2 || act_enable_p !== 1'b0 || act_enable_n !== 1'b1)
            fail_now("Cold boot after PRE did not wake in STATE_UNRESOLVED with output OFF");
        if (issued_pre_tag !== saved_pre_tag || history_len !== (len_at_cut + 32'd1))
            fail_now("Cold boot lost interrupted tag or failed to append UNRESOLVED record");

        auth_nonce = 32'd2;
        set_cmd(4'b0001); // Try new OPEN while UNRESOLVED
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd2 || act_enable_p !== 1'b0)
            fail_now("New OPEN was not blocked after cold-boot UNRESOLVED");

        // Sign one-time L01 recovery challenge and verify history extends
        used_sig          = compute_l01_sig(history_digest, lock_state, history_len);
        l01_challenge_sig = used_sig;
        set_cmd(4'b1000); // req_l01
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd0 || history_len !== (len_at_cut + 32'd2))
            fail_now("Challenge-signed L01 recovery failed or did not extend history");

        // 4. Dual-rail symmetry breach (cmd_rail_p == 4'b0001, cmd_rail_n == 4'b1111 -> bit 0 is 11!)
        cmd_rail_p = 4'b0001;
        cmd_rail_n = 4'b1111;
        #1; // Check 0-cycle combinational crowbar!
        if (crowbar_trip !== 1'b1 || act_enable_p !== 1'b0 || act_enable_n !== 1'b1)
            fail_now("Dual-rail 11 fault did not trip combinational crowbar in 0 cycles");
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd3 || crowbar_trip !== 1'b1)
            fail_now("Dual-rail fault did not latch STATE_KILL");

        // Replaying the previous L01 signature must FAIL because history_digest changed!
        l01_challenge_sig = used_sig;
        set_cmd(4'b1000);
        @(posedge clk);
        #1;
        set_cmd(4'b0000);
        if (lock_state !== 2'd3)
            fail_now("Replayed L01 signature wrongly cleared STATE_KILL");

        $display("[PASS] RTL kosmos_dual_rail_gate: cold-boot UNRESOLVED, ToF, (80,80,10) corner, and 0-cycle dual-rail crowbar verified (hist=%0d).",
                 history_len);
        $finish(0);
    end

endmodule
