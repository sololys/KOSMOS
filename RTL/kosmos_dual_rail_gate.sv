// KOSMOS v0.2 digital candidate gate. L3 output authority is external.
module kosmos_dual_rail_gate (
    input  wire         clk, rst_n,
    input  wire [15:0]  channel,
    input  wire         trap, pre, reset_by_l01,
    input  wire [63:0]  tdc, tau_min, tau_max,
    input  wire [63:0]  rail_a, rail_b,
    input  wire [191:0] p, force_a, force_b,
    input  wire [63:0]  clocks,
    input  wire [383:0] point, reference, tolerances,
    input  wire [63:0]  corner_limit, erased_bits, bit_budget,
    output wire [1:0]   verdict,
    output wire [191:0] command,
    output reg  [63:0]  branches, bits
);
    localparam [1:0] HOLD = 2'd0, OPEN = 2'd1, TRAP = 2'd2, KILL = 2'd3;
    localparam [63:0] POISON = 64'h8000_0000_0000_0000;
    localparam signed [127:0] LIMIT = 128'sh7fff_ffff_ffff_ffff;

    wire [2:0] law_ok;
    wire [191:0] momentum;
    wire [5:0] coord_ok;
    wire [383:0] distances;
    reg [127:0] total_distance;
    reg [999:0] trapped;
    reg killed;
    reg [1:0] verdict_q;
    reg [191:0] command_q;

    genvar axis;
    generate for (axis = 0; axis < 3; axis = axis + 1) begin : momentum_axis
        wire signed [63:0] current = p[axis*64 +: 64];
        wire signed [63:0] drive = force_a[axis*64 +: 64];
        wire signed [63:0] reaction = force_b[axis*64 +: 64];
        wire signed [127:0] next_p = current + drive * $signed({1'b0, clocks});
        assign law_ok[axis] = current != POISON && drive != POISON &&
                              reaction != POISON && reaction == -drive &&
                              next_p >= -LIMIT && next_p <= LIMIT;
        assign momentum[axis*64 +: 64] = next_p[63:0];
    end endgenerate

    genvar dim;
    generate for (dim = 0; dim < 6; dim = dim + 1) begin : coordinate
        wire [63:0] value = point[dim*64 +: 64];
        wire [63:0] origin = reference[dim*64 +: 64];
        wire [63:0] tau = tolerances[dim*64 +: 64];
        wire [63:0] a = value ^ POISON;
        wire [63:0] b = origin ^ POISON;
        wire [63:0] distance = (a >= b) ? a - b : b - a;
        assign distances[dim*64 +: 64] = distance;
        assign coord_ok[dim] = value != POISON && origin != POISON &&
                               tau != 0 && distance <= tau;
    end endgenerate

    integer i;
    always @* begin
        total_distance = 128'd0;
        for (i = 0; i < 6; i = i + 1)
            total_distance = total_distance + {64'd0, distances[i*64 +: 64]};
    end

    wire hard_fault = (rail_a != ~rail_b) || !( &law_ok && &coord_ok ) ||
                      total_distance > {64'd0, corner_limit};
    wire time_ok = tau_min <= tdc && tdc <= tau_max;
    wire count_ok = branches != 64'hffff_ffff_ffff_ffff &&
                    bits <= bit_budget && erased_bits <= bit_budget - bits;

    always @(posedge clk) begin
        if (!rst_n) begin
            trapped <= 1000'd0;
            killed <= 1'b0;
            verdict_q <= HOLD;
            command_q <= 192'd0;
            branches <= 64'd0;
            bits <= 64'd0;
        end else begin
            command_q <= 192'd0;
            if (hard_fault) begin
                killed <= 1'b1;
                verdict_q <= KILL;
            end else if (reset_by_l01) begin
                killed <= 1'b0;
                verdict_q <= HOLD;
            end else if (killed) begin
                verdict_q <= KILL;
            end else if (channel >= 16'd1000) begin
                verdict_q <= HOLD;
            end else if (trap || trapped[channel[9:0]]) begin
                trapped[channel[9:0]] <= 1'b1;
                verdict_q <= TRAP;
            end else if (!pre || !time_ok || !count_ok) begin
                verdict_q <= HOLD;
            end else begin
                verdict_q <= OPEN;
                command_q <= momentum;
                branches <= branches + 64'd1;
                bits <= bits + erased_bits;
            end
        end
    end

    assign verdict = !rst_n ? HOLD : hard_fault ? KILL : verdict_q;
    assign command = (verdict == OPEN) ? command_q : 192'd0;
endmodule
