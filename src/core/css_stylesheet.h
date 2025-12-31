#ifndef CSS_STYLESHEET_H
#define CSS_STYLESHEET_H

#include "css_selector.h"
#include "style.h"

/*
 * CSS Stylesheet Management System
 *
 * Manages CSS rules from multiple sources:
 * - User-agent stylesheet (browser defaults)
 * - External stylesheets (<link> tags)
 * - <style> tags
 * - Inline styles (handled separately, highest priority)
 *
 * Architecture:
 * - Rules are stored with their selectors and property lists
 * - Rules are sorted by specificity for cascade resolution
 * - Efficient lookup when computing styles for elements
 *
 * CSS Cascade Order (lowest to highest priority):
 * 1. User-agent stylesheet (browser defaults)
 * 2. External/internal stylesheets (by specificity)
 * 3. Inline styles
 * 4. !important declarations (not yet implemented)
 */

#define MAX_PROPERTIES_PER_RULE 32

// A single property/value pair in a CSS rule
typedef struct {
    char *property;  // e.g., "color"
    char *value;     // e.g., "red"
} css_property_pair_t;

// A CSS rule: selector + declarations
typedef struct css_rule_s {
    css_selector_t *selector;
    css_property_pair_t properties[MAX_PROPERTIES_PER_RULE];
    int property_count;
    struct css_rule_s *next;  // Linked list
} css_rule_t;

// A complete stylesheet
typedef struct {
    css_rule_t *rules;  // Linked list of rules
    int rule_count;
} css_stylesheet_t;

/*
 * Create a new empty stylesheet
 */
css_stylesheet_t* css_stylesheet_create(void);

/*
 * Free a stylesheet and all its rules
 */
void css_stylesheet_free(css_stylesheet_t *sheet);

/*
 * Parse CSS text and add rules to stylesheet
 * Example CSS:
 *   div { color: red; font-size: 16px; }
 *   .button { background-color: blue; }
 *   #header { width: 100%; }
 *
 * Returns number of rules parsed
 */
int css_stylesheet_parse(css_stylesheet_t *sheet, const char *css_text);

/*
 * Add a single rule to stylesheet
 * Takes ownership of the rule (do not free after adding)
 */
void css_stylesheet_add_rule(css_stylesheet_t *sheet, css_rule_t *rule);

/*
 * Find all matching rules for a node
 * Returns array of matching rules (sorted by specificity)
 * Caller must free the returned array (but not the rules themselves)
 *
 * out_count: filled with number of matching rules
 */
css_rule_t** css_stylesheet_match(css_stylesheet_t *sheet, node_t *node, int *out_count);

/*
 * Apply matching rules to a style_t structure
 * This implements the CSS cascade:
 * - Applies rules in specificity order
 * - Later rules override earlier ones for same property
 */
void css_stylesheet_apply_to_style(css_stylesheet_t *sheet, node_t *node, style_t *style);

/*
 * Get the default user-agent stylesheet
 * Returns a static stylesheet with browser defaults
 * (initialized on first call, persists for application lifetime)
 */
css_stylesheet_t* css_get_user_agent_stylesheet(void);

#endif // CSS_STYLESHEET_H
