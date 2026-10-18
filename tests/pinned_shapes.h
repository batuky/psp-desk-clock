/* The two Time display shapes issue #14 asks for (layouts B and D), spelled
 * out so the tests do not depend on the compositor: if clock_face.c changes
 * a shape, the tests fail until these numbers are changed on purpose too.
 * This is the only place the tests spell them out. */
#ifndef PINNED_SHAPES_H
#define PINNED_SHAPES_H

#include "time_display.h"

/* digit_w, digit_h, thickness, corner_gap, top */
static const TimeDisplayShape PINNED_SHAPE_B = {84, 130, 12, 3, 12};
static const TimeDisplayShape PINNED_SHAPE_D = {80, 120, 11, 3, 8};

#endif
