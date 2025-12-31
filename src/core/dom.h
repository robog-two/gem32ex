#ifndef DOM_H
#define DOM_H

#include <stddef.h>

#include "style.h"

typedef enum {
    NODE_ELEMENT,
    NODE_TEXT
} node_type_t;

typedef struct attr_s {
    char *name;
    char *value;
    struct attr_s *next;
} attr_t;

typedef struct node_s {
    node_type_t type;
    char *tag_name;
    char *content; // For text nodes
    attr_t *attributes;
    style_t *style;
    struct node_s *first_child;
    struct node_s *last_child;
    struct node_s *next_sibling;
    struct node_s *parent;
} node_t;

node_t* node_create(node_type_t type);
void node_free(node_t *node);
void node_add_child(node_t *parent, node_t *child);
void node_add_attr(node_t *node, const char *name, const char *value);

#endif // DOM_H
