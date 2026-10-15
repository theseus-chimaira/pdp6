/* Logical time for the APR relative-delay pulse queue (C99).
 * Units are the scheduler's delay units, NOT calibrated nanoseconds.
 * A zero-delay pulse shares the previous event's timestamp.
 */
#ifndef PDP6_APR_PULSE_CLOCK_V1_H
#define PDP6_APR_PULSE_CLOCK_V1_H
#include <stdint.h>

static inline int
apr_pulse_clock_advance_v1(uint64_t *clock, int delay)
{
    if (clock == 0 || delay < 0 || UINT64_MAX - *clock < (uint64_t)delay)
        return -1;
    *clock += (uint64_t)delay;
    return 0;
}
#endif
