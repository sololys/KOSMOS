#include "kosmos_newton.h"

#include <string.h>

__extension__ typedef __int128 kosmos_i128_t;

static int valid_momentum(const kosmos_v02_input_t *in, int64_t next[KOSMOS_V02_AXES]) {
    for (unsigned i = 0; i < KOSMOS_V02_AXES; ++i) {
        const int64_t p = in->p[i];
        const int64_t f = in->force_a[i];
        const int64_t reaction = in->force_b[i];
        if (p == INT64_MIN || f == INT64_MIN || reaction != -f) {
            return 0;
        }
        const kosmos_i128_t value = (kosmos_i128_t)p + (kosmos_i128_t)f * in->clocks;
        if (value < -INT64_MAX || value > INT64_MAX) {
            return 0;
        }
        next[i] = (int64_t)value;
    }
    return 1;
}

void kosmos_v02_init(kosmos_v02_gate_t *gate) {
    memset(gate, 0, sizeof(*gate));
}

static kosmos_v02_verdict_t decide(kosmos_v02_gate_t *gate, const kosmos_v02_input_t *in,
                                  int64_t next[KOSMOS_V02_AXES]) {
    if (in->rail_a != ~in->rail_b || !valid_momentum(in, next) ||
        !kosmos_v02_bounds_valid(in->point, in->reference, in->tolerances, in->corner_limit)) {
        gate->killed = 1u;
        return KOSMOS_V02_KILL;
    }
    if (in->reset_by_l01 != 0u) {
        gate->killed = 0u;
        return KOSMOS_V02_HOLD;
    }
    if (gate->killed != 0u) {
        return KOSMOS_V02_KILL;
    }
    if (!kosmos_channel_valid(in->channel)) {
        return KOSMOS_V02_HOLD;
    }
    if (in->trap != 0u) {
        kosmos_channel_trap(&gate->channels, in->channel);
    }
    if (kosmos_channel_is_trapped(&gate->channels, in->channel)) {
        return KOSMOS_V02_TRAP;
    }
    if (in->pre == 0u || !kosmos_tdc_in_window(in->tdc, in->tau_min, in->tau_max) ||
        gate->branches == UINT64_MAX || gate->bits > in->bit_budget ||
        in->erased_bits > in->bit_budget - gate->bits) {
        return KOSMOS_V02_HOLD;
    }
    ++gate->branches;
    gate->bits += in->erased_bits;
    return KOSMOS_V02_OPEN;
}

void kosmos_v02_step(kosmos_v02_gate_t *gate, const kosmos_v02_input_t *in,
                     kosmos_v02_result_t *out) {
    int64_t next[KOSMOS_V02_AXES];
    memset(out, 0, sizeof(*out));
    out->verdict = (uint32_t)decide(gate, in, next);
    if (out->verdict == KOSMOS_V02_OPEN) {
        memcpy(out->command, next, sizeof(next));
    }
    out->branches = gate->branches;
    out->bits = gate->bits;
}

kosmos_v02_landauer_t kosmos_v02_landauer(const kosmos_v02_gate_t *gate,
                                        uint64_t millikelvin) {
    return (kosmos_v02_landauer_t){gate->bits, millikelvin};
}
