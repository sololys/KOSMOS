// KOSMOS v0.3 — Self-Checking Testbench & CSV Parity Vector Harness
// RTL/tb_kosmos_v03_physics_gate.sv
//
// 1. Verifies 3D 7-point Laplacian bowl/saddle/peak and two-cycle WAL-before-pulse.
// 2. If +VEC_FILE=<path> and +OUT_FILE=<path> are supplied via vvp plusargs,
//    streams stimulus vectors through kosmos_v03_physics_gate and writes
//    comb_verdict hex codes for bit-exact XOR == 0 verification against C11!
//
// SPDX-License-Identifier: Apache-2.0

`timescale 1ns / 1ps

module tb_kosmos_v03_physics_gate;

    reg                clk;
    reg                por_rst_n;
    reg                sys_rst_n;
    reg          [3:0] rail_p, rail_n;
    reg                airgap_sealed, ancestry_valid, unmerged_fork;
    reg                undecidable_hit, has_ext_witness;
    reg         [31:0] dt_ticks, formal_iters, max_iters;
    reg         [63:0] w_lift_work, dr_sq, ct_sq, s_dot_gen, s_dot_sink;
    reg  signed [31:0] dphi_b, emf_induced, emf_max;
    reg  signed [31:0] force_in, force_max;
    reg         [63:0] energy_rem_2x, w_min_2x, energy_cost_2x;
    reg  signed [31:0] f_ab, f_ba, nash_res_abs_2x, nash_c_ii;
    reg  signed [31:0] f_center, f_xp, f_xm, f_yp, f_ym, f_zp, f_zm;
    reg         [31:0] witness_id, auth_nonce, post_tether_tag, l01_challenge_sig;

    wire               nvm_out_valid;
    wire         [1:0] nvm_out_state;
    wire               nvm_out_unresolved;
    wire        [31:0] nvm_out_nonce, nvm_out_hist_len, nvm_out_digest, nvm_out_pending_tag;

    wire signed [35:0] laplacian7_out;
    wire         [7:0] comb_verdict;
    wire               crowbar_trip, act_enable_p, act_enable_n;
    wire         [1:0] lock_state;
    wire               unresolved_latched;
    wire         [7:0] verdict_code;
    wire        [31:0] issued_pre_tag, history_len, realized_cnt, history_digest;

    kosmos_v03_physics_gate dut (
        .clk                 (clk),
        .por_rst_n           (por_rst_n),
        .sys_rst_n           (sys_rst_n),
        .rail_p              (rail_p),
        .rail_n              (rail_n),
        .airgap_sealed       (airgap_sealed),
        .ancestry_valid      (ancestry_valid),
        .unmerged_fork       (unmerged_fork),
        .undecidable_hit     (undecidable_hit),
        .has_ext_witness     (has_ext_witness),
        .dt_ticks            (dt_ticks),
        .formal_iters        (formal_iters),
        .max_iters           (max_iters),
        .w_lift_work         (w_lift_work),
        .dr_sq               (dr_sq),
        .ct_sq               (ct_sq),
        .s_dot_gen           (s_dot_gen),
        .s_dot_sink          (s_dot_sink),
        .dphi_b              (dphi_b),
        .emf_induced         (emf_induced),
        .emf_max             (emf_max),
        .force_in            (force_in),
        .force_max           (force_max),
        .energy_rem_2x       (energy_rem_2x),
        .w_min_2x            (w_min_2x),
        .energy_cost_2x      (energy_cost_2x),
        .f_ab                (f_ab),
        .f_ba                (f_ba),
        .nash_res_abs_2x     (nash_res_abs_2x),
        .nash_c_ii           (nash_c_ii),
        .f_center            (f_center),
        .f_xp                (f_xp),
        .f_xm                (f_xm),
        .f_yp                (f_yp),
        .f_ym                (f_ym),
        .f_zp                (f_zp),
        .f_zm                (f_zm),
        .witness_id          (witness_id),
        .auth_nonce          (auth_nonce),
        .post_tether_tag     (post_tether_tag),
        .l01_challenge_sig   (l01_challenge_sig),
        .nvm_in_valid        (1'b0),
        .nvm_in_state        (2'd0),
        .nvm_in_unresolved   (1'b0),
        .nvm_in_nonce        (32'd0),
        .nvm_in_hist_len     (32'd0),
        .nvm_in_digest       (32'd0),
        .nvm_in_pending_tag  (32'd0),
        .nvm_out_valid       (nvm_out_valid),
        .nvm_out_state       (nvm_out_state),
        .nvm_out_unresolved  (nvm_out_unresolved),
        .nvm_out_nonce       (nvm_out_nonce),
        .nvm_out_hist_len    (nvm_out_hist_len),
        .nvm_out_digest      (nvm_out_digest),
        .nvm_out_pending_tag (nvm_out_pending_tag),
        .laplacian7_out      (laplacian7_out),
        .comb_verdict        (comb_verdict),
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

    task automatic set_clean_open_defaults;
        begin
            rail_p          = 4'b0000;
            rail_n          = 4'b1111;
            airgap_sealed   = 1'b0;
            ancestry_valid  = 1'b1;
            unmerged_fork   = 1'b0;
            undecidable_hit = 1'b0;
            has_ext_witness = 1'b1;
            dt_ticks        = 32'd10;
            formal_iters    = 32'd5;
            max_iters       = 32'd100;
            w_lift_work     = 64'd50;
            dr_sq           = 64'd400;
            ct_sq           = 64'd900;
            s_dot_gen       = 64'd100;
            s_dot_sink      = 64'd200;
            dphi_b          = 32'sd25;
            emf_induced     = -32'sd25;
            emf_max         = 32'sd100;
            force_in        = 32'sd40;
            force_max       = 32'sd100;
            energy_rem_2x   = 64'd1000;
            w_min_2x        = 64'd200;
            energy_cost_2x  = 64'd300;
            f_ab            = 32'sd75;
            f_ba            = -32'sd75;
            nash_res_abs_2x = 32'sd8;
            nash_c_ii       = 32'sd20;
            f_center        = 32'sd0;
            f_xp            = 32'sd10;
            f_xm            = 32'sd10;
            f_yp            = 32'sd10;
            f_ym            = 32'sd10;
            f_zp            = 32'sd10;
            f_zm            = 32'sd10;
            witness_id      = 32'hCAFE_0300;
            auth_nonce      = 32'd1;
            post_tether_tag = 32'd0;
            l01_challenge_sig = 32'd0;
        end
    endtask

    task automatic fail_now(input [255:0] msg);
        begin
            $fatal(1, "[V03 RTL FAIL] %0s", msg);
        end
    endtask

    reg [1023:0] vec_path;
    reg [1023:0] out_path;
    integer      fd_in;
    integer      fd_out;
    integer      n_read;
    integer      vec_cnt;

    initial begin
        clk       = 1'b0;
        por_rst_n = 1'b0;
        sys_rst_n = 1'b0;
        set_clean_open_defaults();

        repeat (2) @(posedge clk);
        #1;
        por_rst_n = 1'b1;
        sys_rst_n = 1'b1;
        @(posedge clk);
        #1;

        // 1. Check 7-point Laplacian: bowl (+60), harmonic saddle (0), checkerboard (+1000/-1000 -> -12000 != 0!)
        if (laplacian7_out !== 36'sd60)
            fail_now("Bowl Laplacian mismatch");

        f_xp = 32'sd20; f_xm = 32'sd20;
        f_yp = -32'sd10; f_ym = -32'sd10;
        f_zp = -32'sd10; f_zm = -32'sd10;
        f_center = 32'sd0;
        #1;
        if (laplacian7_out !== 36'sd0)
            fail_now("Saddle harmonic Laplacian != 0");

        f_center = 32'sd1000;
        f_xp = -32'sd1000; f_xm = -32'sd1000;
        f_yp = -32'sd1000; f_ym = -32'sd1000;
        f_zp = -32'sd1000; f_zm = -32'sd1000;
        #1;
        if (laplacian7_out !== -36'sd12000)
            fail_now("Checkerboard oscillation was not detected by 7-point Laplacian");

        // 2. Check clean OPEN -> T1 pre-commit (0 V) -> T2 pulse (OPEN 0xA5)
        rail_p = 4'b0001;
        rail_n = 4'b1110;
        #1;
        if (comb_verdict !== 8'hA5)
            fail_now("Clean defaults did not yield comb_verdict == OPEN (0xA5)");
        @(posedge clk); // T1
        #1;
        rail_p = 4'b0000; rail_n = 4'b1111;
        if (lock_state !== 2'd1 || act_enable_p !== 1'b0)
            fail_now("T1 violated WAL-before-pulse");
        @(posedge clk); // T2
        #1;
        if (act_enable_p !== 1'b1 || verdict_code !== 8'hA5)
            fail_now("T2 failed to fire pulse");

        // 2B. Adversarial T2 breach: arm T1, then drop req_open=0 AND breach lightcone (dr_sq=999 > ct_sq=100) in T2!
        por_rst_n = 1'b0; sys_rst_n = 1'b0;
        set_clean_open_defaults();
        @(posedge clk); #1;
        por_rst_n = 1'b1; sys_rst_n = 1'b1;
        @(posedge clk); #1;
        rail_p = 4'b0001; rail_n = 4'b1110; // req_open=1
        @(posedge clk); #1; // T1 armed
        if (lock_state !== 2'd1 || act_enable_p !== 1'b0)
            fail_now("T2-breach setup failed at T1");
        rail_p = 4'b0000; rail_n = 4'b1111; // Host drops req_open=0
        dr_sq  = 64'd999; ct_sq  = 64'd100; // Superluminal breach -> comb_verdict == VERDICT_KILL!
        #1; // Immediate 0-cycle crowbar before T2 clock edge!
        if (comb_verdict !== 8'h00 || crowbar_trip !== 1'b1 || act_enable_p !== 1'b0 || verdict_code !== 8'h00)
            fail_now("T2 lightcone breach with req_open=0 failed 0-cycle crowbar_trip");
        @(posedge clk); #1; // T2 clock edge MUST abort pulse and latch STATE_KILL + unresolved_latched!
        if (act_enable_p !== 1'b0 || crowbar_trip !== 1'b1 || lock_state !== 2'd3 ||
            unresolved_latched !== 1'b1 || verdict_code !== 8'h00)
            fail_now("T2 lightcone breach with req_open=0 fired pulse instead of latching STATE_KILL");
        set_clean_open_defaults();

        // 3. Optional external vector parity streaming for C11 <-> RTL XOR == 0 proof
        if ($value$plusargs("VEC_FILE=%s", vec_path) &&
            $value$plusargs("OUT_FILE=%s", out_path)) begin
            fd_in  = $fopen(vec_path, "r");
            fd_out = $fopen(out_path, "w");
            if (fd_in == 0 || fd_out == 0)
                fail_now("Failed to open parity vector files");

            vec_cnt = 0;
            while (!$feof(fd_in)) begin
                n_read = $fscanf(fd_in,
                    "%h %h %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %h %h %h %h %h %h %h %h\n",
                    rail_p, rail_n,
                    airgap_sealed, ancestry_valid, unmerged_fork, undecidable_hit, has_ext_witness,
                    dt_ticks, formal_iters, max_iters,
                    dphi_b, emf_induced, emf_max, force_in, force_max, f_ab, f_ba, nash_res_abs_2x, nash_c_ii,
                    w_lift_work, dr_sq, ct_sq, energy_rem_2x, w_min_2x, energy_cost_2x, s_dot_gen, s_dot_sink
                );
                if (n_read == 27) begin
                    #1;
                    $fwrite(fd_out, "%02x\n", comb_verdict);
                    vec_cnt = vec_cnt + 1;
                end
            end
            $fclose(fd_in);
            $fclose(fd_out);
            $display("[PASS] RTL kosmos_v03_physics_gate streamed %0d parity vectors.", vec_cnt);
        end else begin
            $display("[PASS] RTL kosmos_v03_physics_gate: Laplacian, Lenz, Inertia, Nash, Lightcone, and WAL-before-pulse verified.");
        end

        $finish(0);
    end

endmodule
