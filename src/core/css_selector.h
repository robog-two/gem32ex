#ifndef CSS_SELECTOR_H
#define CSS_SELECTOR_H

#include "dom.h"

/*
 * CSS Selector Matching System
 *
 * Implements CSS2 selector matching for applying stylesheet rules to DOM elements.
 * Supports basic selectors: type, class, ID, descendant, and universal selectors.
 *
 * Architecture:
 * - Selectors are parsed into a lightweight data structure
 * - Matching is O(n) where n is the selector complexity
 * - Specificity is calculated for cascade resolution
 *
 * Supported Selectors:
 * - Type selector: div, p, span
 * - Class selector: .classname
 * - ID selector: #idname
 * - Universal selector: *
 * - Descendant combinator: div p (space)
 * - Multiple classes: .class1.class2
 *
 * Future enhancements could add:
 * - Child combinator: >
 * - Adjacent sibling: +
 * - Attribute selectors: [attr=value]
 * - Pseudo-classes: :hover, :first-child
 */

// Selector types
typedef enum {
    SELECTOR_TYPE,        // Element type (e.g., "div")
    SELECTOR_CLASS,       // Class name (e.g., ".button")
    SELECTOR_ID,          // ID (e.g., "#header")
    SELECTOR_UNIVERSAL    // Universal (*) - matches anything
} selector_type_t;

// Combinator types (relationship between selectors)
typedef enum {
    COMBINATOR_DESCENDANT,  // Space (e.g., "div p")
    COMBINATOR_CHILD,       // > (future)
    COMBINATOR_ADJACENT     // + (future)
} combinator_t;

// A single simple selector (e.g., "div", ".classname", "#id")
typedef struct {
    selector_type_t type;
    char *value;  // Tag name, class name, or ID
} simple_selector_t;

// A compound selector part (can have multiple simple selectors)
// Example: "div.container#main" has 3 simple selectors
#define MAX_SIMPLE_SELECTORS 8
typedef struct css_selector_part_s {
    simple_selector_t selectors[MAX_SIMPLE_SELECTORS];
    int selector_count;
    combinator_t combinator;  // Relationship to next part
    struct css_selector_part_s *next;  // Next part in chain
} css_selector_part_t;

// Complete selector with specificity
typedef struct {
    css_selector_part_t *head;  // Linked list of selector parts
    int specificity;             // CSS specificity (for cascade)
} css_selector_t;

/*
 * CSS Specificity Calculation (CSS2 spec)
 * Returns a 3-digit number: (a,b,c)
 * - a = count of ID selectors
 * - b = count of class selectors + attribute selectors + pseudo-classes
 * - c = count of type selectors + pseudo-elements
 * Represented as: a*100 + b*10 + c
 */
int css_selector_specificity(css_selector_t *selector);

/*
 * Parse a selector string into a css_selector_t structure
 * Examples:
 *   "div" -> type selector
 *   ".button" -> class selector
 *   "#header" -> ID selector
 *   "div.container" -> type + class
 *   "div p" -> descendant selector
 *   "div.container p.text" -> complex descendant selector
 */
css_selector_t* css_selector_parse(const char *selector_string);

/*
 * Free a parsed selector
 */
void css_selector_free(css_selector_t *selector);

/*
 * Check if a selector matches a specific DOM node
 * Returns 1 if match, 0 if no match
 */
int css_selector_matches(css_selector_t *selector, node_t *node);

/*
 * Utility: Check if a node has a specific class
 * (Helper for class selector matching)
 */
int node_has_class(node_t *node, const char *class_name);

/*
 * Utility: Get node's ID attribute
 * (Helper for ID selector matching)
 */
const char* node_get_id(node_t *node);

#endif // CSS_SELECTOR_H
