#include "loader.h"
#include "http.h"
#include "core/html.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char* get_attr(node_t *node, const char *name) {
    attr_t *attr = node->attributes;
    while (attr) {
        if (strcasecmp(attr->name, name) == 0) return attr->value;
        attr = attr->next;
    }
    return NULL;
}

void loader_fetch_resources(node_t *node, const char *base_url) {
    if (!node) return;

    if (node->type == NODE_ELEMENT) {
        if (strcasecmp(node->tag_name, "img") == 0) {
            const char *src = get_attr(node, "src");
            if (src) {
                char full_url[1024];
                if (strncmp(src, "http", 4) == 0) {
                    strncpy(full_url, src, 1023);
                } else {
                    snprintf(full_url, 1023, "%s/%s", base_url, src);
                }

                network_response_t *res = http_fetch(full_url);
                if (res) {
                    node->image_data = res->data;
                    node->image_size = res->size;
                    res->data = NULL; // Take ownership
                    network_response_free(res);
                }
            }
        } else if (strcasecmp(node->tag_name, "iframe") == 0) {
            const char *src = get_attr(node, "src");
            if (src) {
                char full_url[1024];
                if (strncmp(src, "http", 4) == 0) {
                    strncpy(full_url, src, 1023);
                } else {
                    snprintf(full_url, 1023, "%s/%s", base_url, src);
                }

                network_response_t *res = http_fetch(full_url);
                if (res && res->data) {
                    node->iframe_doc = html_parse(res->data);
                    // Recursively fetch resources for the iframe
                    loader_fetch_resources(node->iframe_doc, full_url);
                    network_response_free(res);
                }
            }
        }
    }

    node_t *child = node->first_child;
    while (child) {
        loader_fetch_resources(child, base_url);
        child = child->next_sibling;
    }
}
