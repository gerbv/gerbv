// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef GERBV_GERBV_SAFETY
#define GERBV_GERBV_SAFETY

#include <assert.h>
#include <stddef.h>

#include "gerbv.h"

inline size_t gerbv_simplified_amacro_parameter_idx(gerbv_simplified_amacro_t const* amacro, size_t idx) {
    static_assert(
        (sizeof(amacro->parameter) / sizeof(amacro->parameter[0])) == APERTURE_PARAMETERS_MAX,
        "Statically known gerbv_simplified_amacro parameter array size has changed");

    if (idx < APERTURE_PARAMETERS_MAX) {
        return idx;
    }

    GERB_FATAL_ERROR(/*_*/("invalid gerbv_simplified_amacro parameter index"));
    return 0;
}

#define gerbv_simplified_amacro_parameter(gerbv_simplified_amacro, idx)	\
	((gerbv_simplified_amacro)->parameter[gerbv_simplified_amacro_parameter_idx((gerbv_simplified_amacro), (idx))])


#endif

