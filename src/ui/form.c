#include "form.h"
#include "network/http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static node_t* find_enclosing_form(node_t *node) {
    node_t *curr = node;
    while (curr) {
        if (curr->type == DOM_NODE_ELEMENT && curr->tag_name && strcasecmp(curr->tag_name, "form") == 0) {
            return curr;
        }
        curr = curr->parent;
    }
    return NULL;
}

// Simple URL encoder (A-Z, a-z, 0-9, -, ., _, ~ are safe)
static char* url_encode(const char *str) {
    if (!str) return strdup("");
    size_t len = strlen(str);
    size_t capacity = len * 3 + 1;
    char *res = malloc(capacity);
    if (!res) return NULL;
    
    const char *p = str;
    char *d = res;
    while (*p) {
        if (isalnum((unsigned char)*p) || *p == '-' || *p == '.' || *p == '_' || *p == '~') {
            *d++ = *p;
        } else {
            sprintf(d, "%%%02X", (unsigned char)*p);
            d += 3;
        }
        p++;
    }
    *d = '\0';
    return res;
}

static void collect_inputs(node_t *root, char **buffer, size_t *size) {
    if (!root) return;

    if (root->type == DOM_NODE_ELEMENT && root->tag_name && strcasecmp(root->tag_name, "input") == 0) {
        const char *name = node_get_attr(root, "name");
        if (name) {
            const char *val = root->current_value ? root->current_value : node_get_attr(root, "value");
            if (!val) val = "";

            char *enc_name = url_encode(name);
            char *enc_val = url_encode(val);
            
            size_t entry_len = strlen(enc_name) + strlen(enc_val) + 2; // name=val&
            *buffer = realloc(*buffer, *size + entry_len + 1);
            
            if (*size > 0) {
                strcat(*buffer, "&");
                *size += 1;
            }
            strcat(*buffer, enc_name);
            strcat(*buffer, "=");
            strcat(*buffer, enc_val);
            *size += strlen(enc_name) + 1 + strlen(enc_val);

            free(enc_name);
            free(enc_val);
        }
    }

    node_t *child = root->first_child;
    while (child) {
        collect_inputs(child, buffer, size);
        child = child->next_sibling;
    }
}

network_response_t* form_submit(node_t *submit_node, const char *base_url) {
    node_t *form = find_enclosing_form(submit_node);
    if (!form) return NULL;

    const char *action = node_get_attr(form, "action");
    const char *method = node_get_attr(form, "method");
    if (!method) method = "GET";

    // Resolve URL (Simple logic)
    char target_url[2048];
    if (action && strlen(action) > 0) {
        // Check if absolute
        if (strstr(action, "://")) {
            strncpy(target_url, action, sizeof(target_url));
        } else {
            // Relative
            // TODO: Better relative URL resolving using base_url
            snprintf(target_url, sizeof(target_url), "%s/%s", base_url, action); 
            // Very hacky, ideally strip filename from base_url first
        }
    } else {
        strncpy(target_url, base_url, sizeof(target_url));
    }
    target_url[sizeof(target_url)-1] = '\0';

    char *body = calloc(1, 1);
    size_t body_size = 0;
    collect_inputs(form, &body, &body_size);

    network_response_t *res = NULL;
    if (strcasecmp(method, "POST") == 0) {
        res = http_post(target_url, body, "application/x-www-form-urlencoded");
    } else {
        // GET - append query string
        char full_url[4096];
        snprintf(full_url, sizeof(full_url), "%s?%s", target_url, body);
        res = http_fetch(full_url);
    }

    free(body);
    return res;
}
