#include "css_selector.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// Helper: Check if node has a specific class
int node_has_class(node_t *node, const char *class_name) {
    if (!node || !class_name) return 0;
    const char *class_attr = node_get_attr(node, "class");
    if (!class_attr) return 0;

    // Handle multiple classes (space-separated)
    char *copy = strdup(class_attr);
    if (!copy) return 0;

    int found = 0;
    char *token = strtok(copy, " \t\n");
    while (token) {
        if (strcasecmp(token, class_name) == 0) {
            found = 1;
            break;
        }
        token = strtok(NULL, " \t\n");
    }

    free(copy);
    return found;
}

// Helper: Get node's ID
const char* node_get_id(node_t *node) {
    if (!node) return NULL;
    return node_get_attr(node, "id");
}

// Calculate specificity
int css_selector_specificity(css_selector_t *selector) {
    if (!selector) return 0;

    int id_count = 0;
    int class_count = 0;
    int type_count = 0;

    css_selector_part_t *part = selector->head;
    while (part) {
        for (int i = 0; i < part->selector_count; i++) {
            simple_selector_t *s = &part->selectors[i];
            if (s->type == SELECTOR_ID) id_count++;
            else if (s->type == SELECTOR_CLASS) class_count++;
            else if (s->type == SELECTOR_TYPE) type_count++;
            // Universal selector adds nothing to specificity
        }
        part = part->next;
    }

    return (id_count * 100) + (class_count * 10) + type_count;
}

// Free selector
void css_selector_free(css_selector_t *selector) {
    if (!selector) return;

    css_selector_part_t *part = selector->head;
    while (part) {
        css_selector_part_t *next = part->next;
        for (int i = 0; i < part->selector_count; i++) {
            if (part->selectors[i].value) {
                free(part->selectors[i].value);
            }
        }
        free(part);
        part = next;
    }

    free(selector);
}

// Parse a selector string
css_selector_t* css_selector_parse(const char *selector_string) {
    if (!selector_string) return NULL;

    css_selector_t *selector = calloc(1, sizeof(css_selector_t));
    if (!selector) return NULL;

    char *copy = strdup(selector_string);
    if (!copy) {
        free(selector);
        return NULL;
    }

    // Trim whitespace
    char *start = copy;
    while (*start && isspace(*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) *end-- = '\0';

    // Parse selector parts separated by combinators (spaces for now)
    css_selector_part_t *last_part = NULL;
    char *part_str = strtok(start, " \t\n");

    while (part_str) {
        css_selector_part_t *part = calloc(1, sizeof(css_selector_part_t));
        if (!part) {
            css_selector_free(selector);
            free(copy);
            return NULL;
        }

        // Parse compound selector (e.g., "div.class#id")
        const char *p = part_str;
        while (*p) {
            if (part->selector_count >= MAX_SIMPLE_SELECTORS) break;

            simple_selector_t *simple = &part->selectors[part->selector_count];

            if (*p == '.') {
                // Class selector
                p++;  // Skip '.'
                const char *class_start = p;
                while (*p && *p != '.' && *p != '#' && !isspace(*p)) p++;
                int len = p - class_start;
                if (len > 0) {
                    simple->type = SELECTOR_CLASS;
                    simple->value = malloc(len + 1);
                    memcpy(simple->value, class_start, len);
                    simple->value[len] = '\0';
                    part->selector_count++;
                }
            } else if (*p == '#') {
                // ID selector
                p++;  // Skip '#'
                const char *id_start = p;
                while (*p && *p != '.' && *p != '#' && !isspace(*p)) p++;
                int len = p - id_start;
                if (len > 0) {
                    simple->type = SELECTOR_ID;
                    simple->value = malloc(len + 1);
                    memcpy(simple->value, id_start, len);
                    simple->value[len] = '\0';
                    part->selector_count++;
                }
            } else if (*p == '*') {
                // Universal selector
                simple->type = SELECTOR_UNIVERSAL;
                simple->value = NULL;
                part->selector_count++;
                p++;
            } else if (isalpha(*p)) {
                // Type selector
                const char *type_start = p;
                while (*p && isalnum(*p) && *p != '.' && *p != '#') p++;
                int len = p - type_start;
                if (len > 0) {
                    simple->type = SELECTOR_TYPE;
                    simple->value = malloc(len + 1);
                    memcpy(simple->value, type_start, len);
                    simple->value[len] = '\0';
                    part->selector_count++;
                }
            } else {
                p++;  // Skip unknown characters
            }
        }

        // Link part into chain
        part->combinator = COMBINATOR_DESCENDANT;  // Space combinator
        if (!selector->head) {
            selector->head = part;
        } else {
            last_part->next = part;
        }
        last_part = part;

        part_str = strtok(NULL, " \t\n");
    }

    free(copy);

    // Calculate specificity
    selector->specificity = css_selector_specificity(selector);

    return selector;
}

// Check if a simple selector matches a node
static int simple_selector_matches(simple_selector_t *sel, node_t *node) {
    if (!sel || !node) return 0;

    switch (sel->type) {
        case SELECTOR_UNIVERSAL:
            return 1;  // * matches everything

        case SELECTOR_TYPE:
            return node->tag_name && strcasecmp(node->tag_name, sel->value) == 0;

        case SELECTOR_CLASS:
            return node_has_class(node, sel->value);

        case SELECTOR_ID: {
            const char *id = node_get_id(node);
            return id && strcasecmp(id, sel->value) == 0;
        }

        default:
            return 0;
    }
}

// Check if a selector part matches a node (all simple selectors must match)
static int selector_part_matches(css_selector_part_t *part, node_t *node) {
    if (!part || !node) return 0;

    // All simple selectors in the part must match
    for (int i = 0; i < part->selector_count; i++) {
        if (!simple_selector_matches(&part->selectors[i], node)) {
            return 0;
        }
    }

    return 1;
}

// Check if selector matches node (handles descendant combinators)
int css_selector_matches(css_selector_t *selector, node_t *node) {
    if (!selector || !node || !selector->head) return 0;

    // Build an array of parts (for easier backward traversal)
    int part_count = 0;
    css_selector_part_t *p = selector->head;
    while (p) {
        part_count++;
        p = p->next;
    }

    if (part_count == 0) return 0;

    css_selector_part_t **parts = malloc(part_count * sizeof(css_selector_part_t*));
    if (!parts) return 0;

    p = selector->head;
    for (int i = 0; i < part_count; i++) {
        parts[i] = p;
        p = p->next;
    }

    // Match rightmost part first (the target element)
    node_t *current = node;
    int part_index = part_count - 1;

    while (part_index >= 0 && current) {
        css_selector_part_t *part = parts[part_index];

        if (selector_part_matches(part, current)) {
            // This part matches, move to next part
            part_index--;
            if (part_index < 0) {
                // All parts matched!
                free(parts);
                return 1;
            }

            // For descendant combinator, search up the tree
            if (part->combinator == COMBINATOR_DESCENDANT) {
                current = current->parent;
            } else {
                // Future: handle child combinator (>) and adjacent sibling (+)
                current = current->parent;
            }
        } else {
            // This part doesn't match, try parent (descendant search)
            current = current->parent;
        }
    }

    free(parts);
    return 0;  // Didn't match all parts
}
