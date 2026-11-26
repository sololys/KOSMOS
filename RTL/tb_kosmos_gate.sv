// KOSMOS — Symplectic Witness-Gated Realization Space
// RTL/tb_kosmos_gate.sv : Self-checking IEEE 1800-2012 Testbench
//
// Exercises all 8 mandatory invariants in hardware simulation.
// SPDX-License-Identifier: Apache-2.0

`timescale 1ns / 1ps

module tb_kosmos_gate;

    reg                clk;
    reg                rst_n;

    reg  signed [31:0] hard_meas_0, hard_meas_1, hard_meas_2;
    reg  signed [31:0] hard_tol_0,  hard_tol_1,  hard_tol_2;

    reg  signed [31:0] q_x, q_y, q_z;
    reg  signed [31:0] p_x, p_y, p_z;
    reg  signed [31:0] u_x, u_y, u_z;
    reg  signed [31:0] cand_pos_tol, cand_ctrl_tol;

    reg                req_open;
    reg         [31:0] witness_pre_id;
    reg         [31:0] auth_nonce;
    reg  signed [31:0] seam_defect;
    reg          [1:0] chu_w0, chu_w1, chu_w2, chu_w3;

    reg                req_post;
    reg         [31:0] witness_post_id;
    reg         [31:0] post_tether_tag;
    reg                post_outcome_ok;
    reg                power_cut_fault;

    reg                req_l01_recover;
    reg         [63:0] l01_key;

    wire signed [31:0] mirror_q_x, mirror_q_y, mirror_q_z;
    wire signed [31:0] mirror_p_x, mirror_p_y, mirror_p_z;
    wire               mirror_poison;

    wire         [1:0] lock_state;
    wire         [7:0] verdict_code;
    wire        [31:0] issued_pre_tag;
    wire        [31:0] history_len;
    wire        [31:0] realized_cnt;
    wire        [31:0] history_digest;
    wire signed [31:0] out_u_x, out_u_y, out_u_z;

    kosmos_gate #(
        .MAX_INFLIGHT_CYCLES(8)
    ) dut (
        .clk             (clk),
        .rst_n           (rst_n),
        .hard_meas_0     (hard_meas_0),
        .hard_meas_1     (hard_meas_1),
        .hard_meas_2     (hard_meas_2),
        .hard_tol_0      (hard_tol_0),
        .hard_tol_1      (hard_tol_1),
        .hard_tol_2      (hard_tol_2),
        .q_x             (q_x),
        .q_y             (q_y),
        .q_z             (q_z),
        .p_x             (p_x),
        .p_y             (p_y),
        .p_z             (p_z),
        .u_x             (u_x),
        .u_y             (u_y),
        .u_z             (u_z),
        .cand_pos_tol    (cand_pos_tol),
        .cand_ctrl_tol   (cand_ctrl_tol),
        .req_open        (req_open),
        .witness_pre_id  (witness_pre_id),
        .auth_nonce      (auth_nonce),
        .seam_defect     (seam_defect),
        .chu_w0          (chu_w0),
        .chu_w1          (chu_w1),
        .chu_w2          (chu_w2),
        .chu_w3          (chu_w3),
        .req_post        (req_post),
        .witness_post_id (witness_post_id),
        .post_tether_tag (post_tether_tag),
        .post_outcome_ok (post_outcome_ok),
        .power_cut_fault (power_cut_fault),
        .req_l01_recover (req_l01_recover),
        .l01_key         (l01_key),
        .mirror_q_x      (mirror_q_x),
        .mirror_q_y      (mirror_q_y),
        .mirror_q_z      (mirror_q_z),
        .mirror_p_x      (mirror_p_x),
        .mirror_p_y      (mirror_p_y),
        .mirror_p_z      (mirror_p_z),
        .mirror_poison   (mirror_poison),
        .lock_state      (lock_state),
        .verdict_code    (verdict_code),
        .issued_pre_tag  (issued_pre_tag),
        .history_len     (history_len),
        .realized_cnt    (realized_cnt),
        .history_digest  (history_digest),
        .out_u_x         (out_u_x),
        .out_u_y         (out_u_y),
        .out_u_z         (out_u_z)
    );

    always #5 clk = ~clk;

    task automatic fail_now(input [255:0] msg);
        begin
            $fatal(1, "[RTL FAIL] %0s", msg);
        end
    endtask

    reg [31:0] saved_tag;
    reg [31:0] len_before;

    initial begin
        clk             = 1'b0;
        rst_n           = 1'b0;
        hard_meas_0     = 32'sd10;
        hard_meas_1     = -32'sd20;
        hard_meas_2     = 32'sd5;
        hard_tol_0      = 32'sd1000;
        hard_tol_1      = 32'sd1000;
        hard_tol_2      = 32'sd1000;
        q_x             = 32'sd120;
        q_y             = -32'sd340;
        q_z             = 32'sd560;
        p_x             = 32'sd777;
        p_y             = -32'sd888;
        p_z             = 32'sd2147483647;
        u_x             = 32'sd250;
        u_y             = -32'sd125;
        u_z             = 32'sd50;
        cand_pos_tol    = 32'sd10000;
        cand_ctrl_tol   = 32'sd500;
        req_open        = 1'b0;
        witness_pre_id  = 32'd0;
        auth_nonce      = 32'd0;
        seam_defect     = 32'sd0;
        chu_w0          = 2'b00;
        chu_w1          = 2'b00;
        chu_w2          = 2'b00;
        chu_w3          = 2'b00;
        req_post        = 1'b0;
        witness_post_id = 32'd0;
        post_tether_tag = 32'd0;
        post_outcome_ok = 1'b0;
        power_cut_fault = 1'b0;
        req_l01_recover = 1'b0;
        l01_key         = 64'd0;

        repeat (2) @(posedge clk);
        rst_n = 1'b1;
        @(posedge clk);
        #1;

        // 1. Involution J(q, p) = (q, -p) and HOLD preservation over time
        if (mirror_q_x !== 32'sd120 || mirror_p_x !== -32'sd777 ||
            mirror_p_y !== 32'sd888 || mirror_p_z !== -32'sd2147483647 || mirror_poison !== 1'b0)
            fail_now("Mirror J(q,p)=(q,-p) mismatch");

        repeat (10) @(posedge clk);
        #1;
        if (verdict_code !== 8'h3C || out_u_x !== 32'sd0 || history_len !== 32'd1)
            fail_now("Elapsed cycles mutated HOLD or history");

        // 2. Missing PRE witness blocks OPEN and preserves history_len
        req_open       = 1'b1;
        witness_pre_id = 32'd0;
        auth_nonce     = 32'd1;
        chu_w0         = 2'b10;
        @(posedge clk);
        #1;
        req_open = 1'b0;
        if (verdict_code !== 8'h3C || out_u_x !== 32'sd0 || history_len !== 32'd1)
            fail_now("Missing witness_pre_id allowed OPEN");

        // 3. Valid Stroke 1 -> OPEN (IN_FLIGHT), OPEN != REALIZED until Stroke 2
        req_open       = 1'b1;
        witness_pre_id = 32'hCAFE_0001;
        auth_nonce     = 32'd1;
        chu_w0         = 2'b10;
        chu_w1         = 2'b10;
        @(posedge clk);
        #1;
        req_open = 1'b0;
        if (verdict_code !== 8'hA5 || lock_state !== 2'd1 ||
            out_u_x !== 32'sd250 || realized_cnt !== 32'd0)
            fail_now("Valid W_PRE failed to issue OPEN or prematurely set REALIZED");
        saved_tag = issued_pre_tag;

        // Next cycle: rail drops to 0 while awaiting W_POST
        @(posedge clk);
        #1;
        if (out_u_x !== 32'sd0 || lock_state !== 2'd1 || realized_cnt !== 32'd0)
            fail_now("Actuator pulse did not return to 0 while IN_FLIGHT");

        // Complete Stroke 2 with tethered W_POST
        req_post        = 1'b1;
        witness_post_id = 32'hBEEF_0001;
        post_tether_tag = saved_tag;
        post_outcome_ok = 1'b1;
        @(posedge clk);
        #1;
        req_post = 1'b0;
        if (lock_state !== 2'd0 || realized_cnt !== 32'd1 || verdict_code !== 8'h3C)
            fail_now("Valid tethered W_POST failed to confirm realization");

        // 4. Power cut during IN_FLIGHT locks gate in UNRESOLVED and blocks new OPEN
        req_open       = 1'b1;
        witness_pre_id = 32'hCAFE_0002;
        auth_nonce     = 32'd2;
        @(posedge clk);
        #1;
        req_open = 1'b0;
        if (lock_state !== 2'd1)
            fail_now("Second OPEN failed to enter IN_FLIGHT");

        power_cut_fault = 1'b1;
        @(posedge clk);
        #1;
        power_cut_fault = 1'b0;
        if (lock_state !== 2'd2 || out_u_x !== 32'sd0 || realized_cnt !== 32'd1)
            fail_now("Power loss did not lock into UNRESOLVED");

        // Attempt new OPEN while UNRESOLVED -> must stay blocked at 0 mV
        req_open   = 1'b1;
        auth_nonce = 32'd3;
        @(posedge clk);
        #1;
        req_open = 1'b0;
        if (lock_state !== 2'd2 || verdict_code !== 8'h3C || out_u_x !== 32'sd0)
            fail_now("UNRESOLVED state failed to block new OPEN");

        // Recover via Level-01 key (appends to history, never truncates)
        len_before      = history_len;
        req_l01_recover = 1'b1;
        l01_key         = 64'h4B4F_534D_4F53_3031;
        @(posedge clk);
        #1;
        req_l01_recover = 1'b0;
        if (lock_state !== 2'd0 || history_len !== (len_before + 32'd1))
            fail_now("L01 recovery failed or did not append to history");

        // 5. Replay nonce (reusing auth_nonce == 2) -> immediate KILL
        req_open   = 1'b1;
        auth_nonce = 32'd2;
        @(posedge clk);
        #1;
        req_open = 1'b0;
        if (lock_state !== 2'd3 || verdict_code !== 8'h00 || out_u_x !== 32'sd0)
            fail_now("Replayed auth_nonce failed to trip KILL");

        // L01 recover again
        req_l01_recover = 1'b1;
        @(posedge clk);
        #1;
        req_l01_recover = 1'b0;

        // 6. Single-channel L-infinity hard breach -> immediate KILL
        hard_meas_2 = 32'sd1001; // > hard_tol_2 (1000) while 0 and 1 are tiny
        @(posedge clk);
        #1;
        if (lock_state !== 2'd3 || verdict_code !== 8'h00 || out_u_x !== 32'sd0)
            fail_now("Single-channel L-inf breach failed to trip KILL");
        hard_meas_2 = 32'sd5;

        // L01 recover and test INT32_MIN poison trap
        req_l01_recover = 1'b1;
        @(posedge clk);
        #1;
        req_l01_recover = 1'b0;

        p_x = 32'sh8000_0000; // INT32_MIN poison!
        #1;
        if (mirror_poison !== 1'b1)
            fail_now("INT32_MIN poison not flagged by combinational mirror");
        @(posedge clk);
        #1;
        if (lock_state !== 2'd3 || verdict_code !== 8'h00)
            fail_now("INT32_MIN poison failed to trip KILL");

        $display("[PASS] RTL kosmos_gate: all 8 hardware invariants verified (history_len=%0d, digest=0x%08x).",
                 history_len, history_digest);
        $finish(0);
    end

endmodule
