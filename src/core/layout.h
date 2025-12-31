#ifndef LAYOUT_H
#define LAYOUT_H

#include "dom.h"

/*
 * Layout Engine - LayoutNG-inspired Architecture
 *
 * This layout engine is inspired by Chromium's LayoutNG inline layout system.
 * See: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/core/layout/inline/README.md
 *
 * Key Architectural Concepts:
 *
 * 1. CONSTRAINT-BASED LAYOUT
 *    Input: constraint_space_t (available width/height)
 *    Output: physical_fragment_t (computed position/size)
 *
 * 2. INLINE LAYOUT PHASES (similar to LayoutNG):
 *
 *    Phase 1 - Pre-layout:
 *      - Measure text nodes once using platform APIs
 *      - Cache measurements in layout_box_t (measured_width, measured_height, measured_baseline)
 *      - Avoid redundant text measurement calls
 *
 *    Phase 2 - Line Breaking:
 *      - Iterate through inline items (text, replaced elements, inline-blocks)
 *      - Fit items into lines based on available width
 *      - Break at item boundaries when content doesn't fit
 *      - Accumulate items in line_box_t structure
 *
 *    Phase 3 - Line Box Construction:
 *      - Position items horizontally (handle text-align)
 *      - Position items vertically (baseline alignment)
 *      - Handle different baseline rules for text vs replaced elements
 *
 * 3. BASELINE ALIGNMENT
 *    - Text: aligned using their baseline metric from font
 *    - Replaced elements (img, iframe): bottom edge aligned to baseline
 *    - Inline-block: bottom margin edge aligned to baseline
 *
 * 4. SEPARATION OF CONCERNS
 *    - DOM layer: parse HTML, manage tree structure
 *    - Layout layer: compute geometry (this module)
 *    - Platform layer: text measurement, rendering (see platform.h)
 */

typedef struct rect_s {
    int x, y, width, height;
} rect_t;

// LayoutNG-inspired: Input constraints for layout
typedef struct {
    int available_width;
    int available_height;
    int is_fixed_width;
    int is_fixed_height;
} constraint_space_t;

// LayoutNG-inspired: The output of a layout pass
typedef struct {
    rect_t border_box;   // Position and size including borders
    rect_t content_box;  // Inner content area
    int baseline;        // Distance from top of box to text baseline
} physical_fragment_t;

typedef struct layout_box_s {
    node_t *node;
    physical_fragment_t fragment;

    // Cached constraints (for future invalidation logic)
    constraint_space_t last_space;

    // LayoutNG-inspired: Cached measurements for inline content
    // This implements the "pre-layout" phase - we measure once, use many times
    int measured_width;     // Cached text/content width
    int measured_height;    // Cached text/content height
    int measured_baseline;  // Cached baseline for text
    int is_measured;        // Flag: 1 if measurements are cached and valid

    struct layout_box_s *parent;
    struct layout_box_s *first_child;
    struct layout_box_s *next_sibling;

    struct layout_box_s *iframe_root; // For iframes
} layout_box_t;

layout_box_t* layout_create_tree(node_t *root, int container_width);
void layout_free(layout_box_t *box);

// Modernized layout entry point
void layout_compute(layout_box_t *box, constraint_space_t space);

layout_box_t* layout_hit_test(layout_box_t *root, int x, int y);

#endif // LAYOUT_H
