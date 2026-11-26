module tb_kosmos_dual_rail_gate;
    reg clk = 0, rst_n = 0;
    reg [15:0] channel = 0;
    reg trap = 0, pre = 0, reset_by_l01 = 0;
    reg [63:0] tdc = 133425, tau_min = 133424, tau_max = 133425;
    reg [63:0] rail_a = 0, rail_b = 64'hffff_ffff_ffff_ffff;
    reg [191:0] p = 0, force_a = 2, force_b = {128'd0, -64'sd2};
    reg [63:0] clocks = 3;
    reg [383:0] point = 0, reference = 0, tolerances = {6{64'd1}};
    reg [63:0] corner_limit = 0, erased_bits = 3, bit_budget = 10;
    wire [1:0] verdict;
    wire [191:0] command;
    wire [63:0] branches, bits;

    kosmos_dual_rail_gate dut (.*);

    task tick;
        begin
            #1 clk = 1;
            #1 clk = 0;
        end
    endtask

    initial begin
        tick();
        rst_n = 1;
        tick();
        if (verdict != 0 || command != 0) $fatal(1, "missing PRE");
        pre = 1;
        tdc = 133426;
        tick();
        if (verdict != 0 || branches != 0) $fatal(1, "TDC edge");
        tdc = 133425;
        tick();
        if (verdict != 1 || command != 6 || bits != 3) $fatal(1, "OPEN impulse");
        trap = 1;
        channel = 999;
        tick();
        if (verdict != 2) $fatal(1, "TRAP");
        trap = 0;
        channel = 0;
        tick();
        if (verdict != 1 || bits != 6) $fatal(1, "isolated channel");
        rail_a = 1;
        #1;
        if (verdict != 3 || command != 0) $fatal(1, "combinational clamp");
        tick();
        rail_a = 0;
        tick();
        if (verdict != 3) $fatal(1, "KILL latch");
        reset_by_l01 = 1;
        tick();
        if (verdict != 0) $fatal(1, "authorized reset");
        $display("KOSMOS v0.2 digital RTL smoke PASS");
        $finish;
    end
endmodule
