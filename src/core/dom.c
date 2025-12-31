#include "dom.h"
#include <stdlib.h>
#include <string.h>

node_t* node_create(node_type_t type) {
    node_t *node = calloc(1, sizeof(node_t));
    if (node) {
        node->type = type;
        node->style = calloc(1, sizeof(style_t));
        style_init_default(node->style);
    }
    return node;
}

void node_free(node_t *node) {
    if (!node) return;

    node_t *child = node->first_child;
    while (child) {
        node_t *next = child->next_sibling;
        node_free(child);
        child = next;
    }

    if (node->tag_name) free(node->tag_name);
    if (node->content) free(node->content);
    if (node->style) free(node->style);
    if (node->image_data) free(node->image_data);

    attr_t *attr = node->attributes;
    while (attr) {
        attr_t *next = attr->next;
        if (attr->name) free(attr->name);
        if (attr->value) free(attr->value);
        free(attr);
        attr = next;
    }

    free(node);
}

void node_add_child(node_t *parent, node_t *child) {
    if (!parent || !child) return;
    child->parent = parent;
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        parent->last_child->next_sibling = child;
    }
    parent->last_child = child;
}

void node_add_attr(node_t *node, const char *name, const char *value) {
    if (!node || !name) return;
    attr_t *attr = calloc(1, sizeof(attr_t));
    if (attr) {
        attr->name = strdup(name);
        attr->value = value ? strdup(value) : NULL;
        attr->next = node->attributes;
        node->attributes = attr;
    }
}
