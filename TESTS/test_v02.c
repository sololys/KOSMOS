#include "kosmos_newton.h"

#include <assert.h>

int main(void) {
    kosmos_v02_gate_t gate;
    kosmos_v02_result_t out;
    kosmos_v02_input_t in = {
        .tdc = 133425, .tau_min = 133424, .tau_max = 133425,
        .rail_b = UINT64_MAX, .force_a = {2, 0, 0}, .force_b = {-2, 0, 0},
        .clocks = 3, .tolerances = {1, 1, 1, 1, 1, 1}, .bit_budget = 10,
        .erased_bits = 3,
    };
    const kosmos_channel_edge_t edges[] = {{0, 999}, {1, 999}};
    assert(kosmos_channel_graph_valid(edges, 2, 2));
    assert(!kosmos_channel_graph_valid(edges, 2, 1));

    kosmos_v02_init(&gate);
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_HOLD && out.command[0] == 0);
    in.pre = 1;
    in.tdc++;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_HOLD && out.branches == 0);
    in.tdc--;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_OPEN && out.command[0] == 6 && out.bits == 3);
    const kosmos_v02_landauer_t landauer = kosmos_v02_landauer(&gate, 300000);
    assert(landauer.bits == 3 && landauer.millikelvin == 300000);

    in.channel = 999;
    in.trap = 1;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_TRAP);
    in.channel = 0;
    in.trap = 0;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_OPEN);
    in.point[0] = INT64_MIN;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_KILL && out.command[0] == 0);
    in.point[0] = 0;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_KILL);
    in.reset_by_l01 = 1;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_HOLD);
    in.reset_by_l01 = 0;
    in.point[0] = INT64_MAX;
    in.reference[0] = -INT64_MAX;
    in.point[1] = INT64_MAX;
    in.reference[1] = -INT64_MAX;
    in.tolerances[0] = in.tolerances[1] = UINT64_MAX;
    in.corner_limit = UINT64_MAX;
    kosmos_v02_step(&gate, &in, &out);
    assert(out.verdict == KOSMOS_V02_KILL);
    return 0;
}
