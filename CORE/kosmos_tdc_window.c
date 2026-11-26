#include "kosmos_tdc_window.h"

__extension__ typedef unsigned __int128 kosmos_u128_t;

int kosmos_tdc_in_window(uint64_t sample, uint64_t minimum, uint64_t maximum) {
    return minimum <= sample && sample <= maximum;
}

int kosmos_v02_bounds_valid(const int64_t sample[KOSMOS_V02_DIMS],
                            const int64_t reference[KOSMOS_V02_DIMS],
                            const uint64_t tolerance[KOSMOS_V02_DIMS],
                            uint64_t total_limit) {
    kosmos_u128_t sum = 0;
    const uint64_t sign = UINT64_C(1) << 63;

    for (unsigned i = 0; i < KOSMOS_V02_DIMS; ++i) {
        if (sample[i] == INT64_MIN || reference[i] == INT64_MIN || tolerance[i] == 0u) {
            return 0;
        }
        const uint64_t a = (uint64_t)sample[i] ^ sign;
        const uint64_t b = (uint64_t)reference[i] ^ sign;
        const uint64_t distance = a >= b ? a - b : b - a;
        if (distance > tolerance[i]) {
            return 0;
        }
        sum += distance;
    }
    return sum <= total_limit;
}
