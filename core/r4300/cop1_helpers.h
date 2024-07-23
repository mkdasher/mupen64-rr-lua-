#ifndef COP1_HELPERS_H
#define COP1_HELPERS_H

#include <math.h>
#include "macros.h"

extern float largest_denormal_float;
extern double largest_denormal_double;

void fail_float_input();
void fail_float_input_arg(double x);
void fail_float_output();
void fail_float_convert();

#define LARGEST_DENORMAL(x) (sizeof(x) == 4 ? largest_denormal_float : largest_denormal_double)

#define CHECK_INPUT(x) \
    do { if (g_config.is_float_exception_propagation_enabled && !(fabs(x) > LARGEST_DENORMAL(x)) && x != 0) { \
        fail_float_input_arg(x); \
        return; \
    } } while (0)

#define CHECK_OUTPUT(x) \
    do { if (g_config.is_float_exception_propagation_enabled && !(fabs(x) > LARGEST_DENORMAL(x))) { \
        if (isnan(x)) { \
            fail_float_output(); \
            return; \
        } else { \
            /* Flush denormals to zero manually, since x87 doesn't have a built-in */ \
            /* way to do it. Typically this doesn't matter, because denormals are */ \
            /* too small to cause visible console/emu divergences, but since we */ \
            /* check for them on entry to each operation this becomes important... */ \
            x = copysign(0, x); \
        } \
    } } while (0)

#define CHECK_CONVERT_EXCEPTIONS() \
    do { if (g_config.is_float_exception_propagation_enabled) { read_x87_status_word(); if (x87_status_word & 1) { \
        fail_float_convert(); \
        return; \
    } } } while (0)


#endif /* COP1_HELPERS_H */
