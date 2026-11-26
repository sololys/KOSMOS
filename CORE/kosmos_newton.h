#ifndef KOSMOS_NEWTON_H
#define KOSMOS_NEWTON_H

#include "kosmos_channel_sluice.h"
#include "kosmos_tdc_window.h"

enum { KOSMOS_V02_AXES = 3 };

typedef enum {
    KOSMOS_V02_HOLD, KOSMOS_V02_OPEN, KOSMOS_V02_TRAP, KOSMOS_V02_KILL
} kosmos_v02_verdict_t;

typedef struct {
    uint16_t channel;
    uint8_t trap, pre, reset_by_l01;
    uint64_t tdc, tau_min, tau_max;
    uint64_t rail_a, rail_b;
    int64_t p[KOSMOS_V02_AXES], force_a[KOSMOS_V02_AXES], force_b[KOSMOS_V02_AXES];
    uint64_t clocks;
    int64_t point[KOSMOS_V02_DIMS], reference[KOSMOS_V02_DIMS];
    uint64_t tolerances[KOSMOS_V02_DIMS], corner_limit;
    uint64_t erased_bits, bit_budget;
} kosmos_v02_input_t;

typedef struct {
    kosmos_channel_sluice_t channels;
    uint64_t branches, bits;
    uint8_t killed;
} kosmos_v02_gate_t;

typedef struct {
    uint32_t verdict;
    int64_t command[KOSMOS_V02_AXES];
    uint64_t branches, bits;
} kosmos_v02_result_t;

/* The exact coefficient of ln(2) joules is bits * millikelvin * 1380649 / 10^32. */
typedef struct {
    uint64_t bits, millikelvin;
} kosmos_v02_landauer_t;

void kosmos_v02_init(kosmos_v02_gate_t *gate);
void kosmos_v02_step(kosmos_v02_gate_t *gate, const kosmos_v02_input_t *in,
                     kosmos_v02_result_t *out);
kosmos_v02_landauer_t kosmos_v02_landauer(const kosmos_v02_gate_t *gate,
                                        uint64_t millikelvin);

#endif
