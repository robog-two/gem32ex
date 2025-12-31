#include "html.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int is_void_element(const char *tag) {
    const char *voids[] = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr", NULL};
    for (int i = 0; voids[i]; i++) {
        if (strcasecmp(tag, voids[i]) == 0) return 1;
    }
    return 0;
}

node_t* html_parse(const char *html) {
    if (!html) return NULL;

    node_t *root = node_create(DOM_NODE_ELEMENT);
    root->tag_name = strdup("root");
    node_t *current = root;

    const char *p = html;
    while (*p) {
        if (*p == '<') {
            p++;
            if (*p == '!') { // Comment or DOCTYPE
                while (*p && *p != '>') p++;
                if (*p) p++;
                continue;
            }

            int is_closing = (*p == '/');
            if (is_closing) p++;

            char tag_name[64];
            int i = 0;
            while (*p && !isspace(*p) && *p != '>' && *p != '/') {
                if (i < 63) tag_name[i++] = *p;
                p++;
            }
            tag_name[i] = '\0';

            if (is_closing) {
                if (current->parent) current = current->parent;
                while (*p && *p != '>') p++;
                if (*p) p++;
            } else {
                node_t *new_node = node_create(DOM_NODE_ELEMENT);
                new_node->tag_name = strdup(tag_name);
                
                // Parse attributes
                while (*p && *p != '>' && *p != '/') {
                    while (*p && isspace(*p)) p++;
                    if (*p == '>' || *p == '/') break;

                    char attr_name[64];
                    int j = 0;
                    while (*p && !isspace(*p) && *p != '=' && *p != '>') {
                        if (j < 63) attr_name[j++] = *p;
                        p++;
                    }
                    attr_name[j] = '\0';

                    char *attr_value = NULL;
                    if (*p == '=') {
                        p++;
                        char quote = 0;
                        if (*p == '"' || *p == '\'') quote = *p++;
                        
                        char val_buf[1024];
                        int k = 0;
                        if (quote) {
                            while (*p && *p != quote) {
                                if (k < 1023) val_buf[k++] = *p;
                                p++;
                            }
                            if (*p) p++;
                        } else {
                            while (*p && !isspace(*p) && *p != '>') {
                                if (k < 1023) val_buf[k++] = *p;
                                p++;
                            }
                        }
                        val_buf[k] = '\0';
                        attr_value = strdup(val_buf);
                    }
                    node_add_attr(new_node, attr_name, attr_value);
                    if (attr_value) free(attr_value);
                }
                
                // Initialize current_value for inputs
                if (strcasecmp(tag_name, "input") == 0 || 
                    strcasecmp(tag_name, "textarea") == 0 ||
                    strcasecmp(tag_name, "select") == 0) {
                    const char *val = node_get_attr(new_node, "value");
                    if (val) new_node->current_value = strdup(val);
                }

                int self_closing = (*p == '/');
                if (self_closing) p++;
                while (*p && *p != '>') p++;
                if (*p) p++;

                node_add_child(current, new_node);
                if (!self_closing && !is_void_element(tag_name)) {
                    current = new_node;
                }
            }
        } else {
            const char *start = p;
            while (*p && *p != '<') p++;
            size_t len = p - start;
            if (len > 0) {
                // Check if it's just whitespace
                int only_ws = 1;
                for (size_t k = 0; k < len; k++) {
                    if (!isspace(start[k])) {
                        only_ws = 0;
                        break;
                    }
                }
                
                if (!only_ws) {
                    node_t *text = node_create(DOM_NODE_TEXT);
                    text->content = malloc(len + 1);
                    memcpy(text->content, start, len);
                    text->content[len] = '\0';
                    node_add_child(current, text);
                }
            }
        }
    }

    return root;
}
