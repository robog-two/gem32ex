#ifndef CSS_PROPERTY_H
#define CSS_PROPERTY_H

#include "style.h"
#include "css_name_colors.h"

/*
 * CSS Property Parsing Module
 *
 * Modular system for parsing CSS property/value pairs and applying them to style_t.
 * Separates property parsing logic from selector matching and stylesheet management.
 *
 * This module handles:
 * - Parsing individual CSS properties (e.g., "color: red")
 * - Value parsing (colors, dimensions, keywords)
 * - Applying property values to style_t structures
 *
 * Design:
 * - Each property has its own parsing function
 * - Central dispatcher routes property names to appropriate parsers
 * - Extensible: easy to add new CSS properties
 */

/*
 * Parse a single CSS property and apply it to a style
 * name: property name (e.g., "color", "font-size")
 * value: property value (e.g., "red", "16px")
 * style: target style_t to modify
 *
 * Returns: 1 if property was recognized and applied, 0 if unknown
 */
int css_property_parse(style_t *style, const char *name, const char *value);

/*
 * Parse a CSS declaration block (semicolon-separated properties)
 * Example: "color: red; font-size: 16px; display: block"
 *
 * This is useful for:
 * - Inline style attributes
 * - Style blocks in CSS rules
 */
void css_properties_parse_block(style_t *style, const char *css_block);

/*
 * Parse a CSS color value
 * Supports:
 * - Named colors: "red", "blue", "transparent"
 * - Hex: "#ff0000", "#f00"
 * - rgb(): "rgb(255, 0, 0)"
 * - rgba(): "rgba(255, 0, 0, 0.5)" (alpha ignored for now)
 *
 * Returns color as 0xRRGGBB, or 0xFFFFFFFF for invalid/transparent
 */
uint32_t parse_color(const char *value);

/*
 * Parse a dimension value (number with optional unit)
 * Examples: "16px", "2em", "50%", "10"
 *
 * For now, only pixel values are fully supported
 * Returns the numeric value (px, or interpreted as px)
 */
int css_parse_dimension(const char *value);

/*
 * Parse a URL value from url() notation
 * Example: "url(image.png)" -> "image.png"
 * Returns allocated string (caller must free), or NULL if invalid
 */
char* css_parse_url(const char *value);

#endif // CSS_PROPERTY_H
