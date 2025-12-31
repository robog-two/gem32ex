#ifndef PLATFORM_H
#define PLATFORM_H

#include "style.h"

// Interface for platform-specific operations required by the core engine.
// The implementation resides in src/ui/ (the imperative edge).

// Measure the dimensions of a text string given a style and constraints.
// width_constraint: Maximum width available. -1 if unconstrained (single line).
// returns: Fills out_width, out_height, and out_baseline.
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height, int *out_baseline);

#endif // PLATFORM_H
