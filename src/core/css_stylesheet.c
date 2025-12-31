#include "css_stylesheet.h"
#include "css_property.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Create new stylesheet
css_stylesheet_t* css_stylesheet_create(void) {
    css_stylesheet_t *sheet = calloc(1, sizeof(css_stylesheet_t));
    return sheet;
}

// Free stylesheet
void css_stylesheet_free(css_stylesheet_t *sheet) {
    if (!sheet) return;

    css_rule_t *rule = sheet->rules;
    while (rule) {
        css_rule_t *next = rule->next;

        // Free selector
        if (rule->selector) {
            css_selector_free(rule->selector);
        }

        // Free properties
        for (int i = 0; i < rule->property_count; i++) {
            if (rule->properties[i].property) free(rule->properties[i].property);
            if (rule->properties[i].value) free(rule->properties[i].value);
        }

        free(rule);
        rule = next;
    }

    free(sheet);
}

// Add rule to stylesheet
void css_stylesheet_add_rule(css_stylesheet_t *sheet, css_rule_t *rule) {
    if (!sheet || !rule) return;

    // Add to head of list
    rule->next = sheet->rules;
    sheet->rules = rule;
    sheet->rule_count++;
}

// Parse CSS and add rules
int css_stylesheet_parse(css_stylesheet_t *sheet, const char *css_text) {
    if (!sheet || !css_text) return 0;

    int rules_added = 0;
    char *copy = strdup(css_text);
    if (!copy) return 0;

    char *p = copy;

    // Simple CSS parser: finds "selector { property: value; ... }"
    while (*p) {
        // Skip whitespace and comments
        while (*p && isspace(*p)) p++;
        if (*p == '\0') break;

        // TODO: Handle /* comments */

        // Find selector (everything before '{')
        char *selector_start = p;
        char *open_brace = strchr(p, '{');
        if (!open_brace) break;

        *open_brace = '\0';
        char *selector_str = selector_start;

        // Trim selector
        while (*selector_str && isspace(*selector_str)) selector_str++;
        char *selector_end = open_brace - 1;
        while (selector_end > selector_str && isspace(*selector_end)) *selector_end-- = '\0';

        // Find matching close brace
        p = open_brace + 1;
        char *close_brace = strchr(p, '}');
        if (!close_brace) break;

        *close_brace = '\0';
        char *declarations = p;

        // Parse selector
        css_selector_t *selector = css_selector_parse(selector_str);
        if (selector) {
            // Create rule
            css_rule_t *rule = calloc(1, sizeof(css_rule_t));
            if (rule) {
                rule->selector = selector;

                // Parse declarations
                char *decl_p = declarations;
                while (*decl_p && rule->property_count < MAX_PROPERTIES_PER_RULE) {
                    // Skip whitespace
                    while (*decl_p && isspace(*decl_p)) decl_p++;
                    if (*decl_p == '\0') break;

                    // Find property:value pair
                    char *prop_start = decl_p;
                    char *colon = strchr(decl_p, ':');
                    if (!colon) break;

                    *colon = '\0';
                    char *value_start = colon + 1;

                    // Find end of value (semicolon or end)
                    char *semicolon = strchr(value_start, ';');
                    if (semicolon) {
                        *semicolon = '\0';
                        decl_p = semicolon + 1;
                    } else {
                        decl_p = value_start + strlen(value_start);
                    }

                    // Trim property and value
                    while (*prop_start && isspace(*prop_start)) prop_start++;
                    char *prop_end = colon - 1;
                    while (prop_end > prop_start && isspace(*prop_end)) *prop_end-- = '\0';

                    while (*value_start && isspace(*value_start)) value_start++;
                    char *value_end = value_start + strlen(value_start) - 1;
                    while (value_end > value_start && isspace(*value_end)) *value_end-- = '\0';

                    // Add property to rule
                    if (*prop_start && *value_start) {
                        rule->properties[rule->property_count].property = strdup(prop_start);
                        rule->properties[rule->property_count].value = strdup(value_start);
                        rule->property_count++;
                    }
                }

                // Add rule to stylesheet
                css_stylesheet_add_rule(sheet, rule);
                rules_added++;
            } else {
                css_selector_free(selector);
            }
        }

        p = close_brace + 1;
    }

    free(copy);
    return rules_added;
}

// Match all rules for a node
css_rule_t** css_stylesheet_match(css_stylesheet_t *sheet, node_t *node, int *out_count) {
    if (!sheet || !node) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // First pass: count matching rules
    int match_count = 0;
    css_rule_t *rule = sheet->rules;
    while (rule) {
        if (css_selector_matches(rule->selector, node)) {
            match_count++;
        }
        rule = rule->next;
    }

    if (match_count == 0) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // Allocate array for matching rules
    css_rule_t **matches = malloc(match_count * sizeof(css_rule_t*));
    if (!matches) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // Second pass: collect matching rules
    int index = 0;
    rule = sheet->rules;
    while (rule) {
        if (css_selector_matches(rule->selector, node)) {
            matches[index++] = rule;
        }
        rule = rule->next;
    }

    // Sort by specificity (bubble sort - fine for small arrays)
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = 0; j < match_count - i - 1; j++) {
            if (matches[j]->selector->specificity > matches[j+1]->selector->specificity) {
                css_rule_t *temp = matches[j];
                matches[j] = matches[j+1];
                matches[j+1] = temp;
            }
        }
    }

    if (out_count) *out_count = match_count;
    return matches;
}

// Apply rules to style
void css_stylesheet_apply_to_style(css_stylesheet_t *sheet, node_t *node, style_t *style) {
    if (!sheet || !node || !style) return;

    int count;
    css_rule_t **matches = css_stylesheet_match(sheet, node, &count);

    if (matches) {
        // Apply rules in specificity order (low to high)
        for (int i = 0; i < count; i++) {
            css_rule_t *rule = matches[i];
            for (int j = 0; j < rule->property_count; j++) {
                css_property_parse(style,
                                 rule->properties[j].property,
                                 rule->properties[j].value);
            }
        }

        free(matches);
    }
}

// User-agent stylesheet (browser defaults)
static css_stylesheet_t *user_agent_sheet = NULL;

css_stylesheet_t* css_get_user_agent_stylesheet(void) {
    if (user_agent_sheet) return user_agent_sheet;

    user_agent_sheet = css_stylesheet_create();
    if (!user_agent_sheet) return NULL;

    // Define basic user-agent styles
    // This is a simplified version - real browsers have much more
    const char *ua_css =
        "html, body, div, p, h1, h2, h3, h4, h5, h6, ul, ol, li, dl, dt, dd, "
        "address, blockquote, pre, hr, form, fieldset, table, tr, td, th { "
        "  display: block; "
        "} "
        "head, meta, title, link, script, style { display: none; } "
        "body { margin: 8px; } "
        "p, blockquote, ul, ol, dl { margin-top: 16px; margin-bottom: 16px; } "
        "h1 { font-size: 32px; font-weight: 700; margin-top: 21px; margin-bottom: 21px; } "
        "h2 { font-size: 24px; font-weight: 700; margin-top: 19px; margin-bottom: 19px; } "
        "h3 { font-size: 19px; font-weight: 700; margin-top: 18px; margin-bottom: 18px; } "
        "h4 { font-size: 16px; font-weight: 700; margin-top: 16px; margin-bottom: 16px; } "
        "h5 { font-size: 13px; font-weight: 700; margin-top: 16px; margin-bottom: 16px; } "
        "h6 { font-size: 11px; font-weight: 700; margin-top: 16px; margin-bottom: 16px; } "
        "b, strong { font-weight: 700; } "
        "i, em { font-style: italic; } "
        "u { text-decoration: underline; } "
        "a { color: #0000EE; text-decoration: underline; } "
        "ul, ol { padding-left: 40px; } "
        "li { display: list-item; } "
        "pre { font-family: monospace; } "
        "center { text-align: center; } "
        "hr { border-width: 1px; margin-top: 8px; margin-bottom: 8px; } "
        "blockquote { margin-left: 40px; margin-right: 40px; } ";

    css_stylesheet_parse(user_agent_sheet, ua_css);

    return user_agent_sheet;
}
