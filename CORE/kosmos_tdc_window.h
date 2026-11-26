#ifndef KOSMOS_TDC_WINDOW_H
#define KOSMOS_TDC_WINDOW_H

#include <stdint.h>

enum { KOSMOS_V02_DIMS = 6 };

int kosmos_tdc_in_window(uint64_t sample, uint64_t minimum, uint64_t maximum);
int kosmos_v02_bounds_valid(const int64_t sample[KOSMOS_V02_DIMS],
                            const int64_t reference[KOSMOS_V02_DIMS],
                            const uint64_t tolerance[KOSMOS_V02_DIMS],
                            uint64_t total_limit);

#endif
