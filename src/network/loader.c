#include "loader.h"
#include "http.h"
#include "core/html.h"
#include "core/log.h"
#include "core/cache.h"
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

int loader_count_resources(node_t *node) {
    if (!node) return 0;
    int count = 0;
    if (node->type == DOM_NODE_ELEMENT) {
        if (node->style && node->style->bg_image) count++;
        if (strcasecmp(node->tag_name, "img") == 0) {
             if (get_attr(node, "src")) count++;
        }
        else if (strcasecmp(node->tag_name, "iframe") == 0) {
             if (get_attr(node, "src")) count++;
        }
    }
    node_t *child = node->first_child;
    while (child) {
        count += loader_count_resources(child);
        child = child->next_sibling;
    }
    return count;
}

void loader_fetch_resources(node_t *node, const char *base_url, loader_progress_cb_t cb, void *ctx, int *current_count, int total_count) {
    if (!node) return;

    if (node->type == DOM_NODE_ELEMENT) {
        // Background Image
        if (node->style && node->style->bg_image) {
            char full_url[1024];
            if (strncmp(node->style->bg_image, "http", 4) == 0) {
                strncpy(full_url, node->style->bg_image, 1023);
            } else {
                snprintf(full_url, 1023, "%s/%s", base_url, node->style->bg_image);
            }

            // Try cache first
            size_t cached_size = 0;
            void *cached_data = cache_get_image(full_url, &cached_size);
            if (cached_data) {
                LOG_DEBUG("Loaded background from cache: %s (%zu bytes)", full_url, cached_size);
                node->bg_image_data = cached_data;
                node->bg_image_size = cached_size;
            } else {
                // Fetch from network
                network_response_t *res = network_fetch(full_url);
                if (res) {
                    LOG_DEBUG("Loaded background from network: %s (%lu bytes)", full_url, (unsigned long)res->size);
                    node->bg_image_data = res->data;
                    node->bg_image_size = res->size;
                    // Cache for later
                    cache_put_image(full_url, res->data, res->size);
                    res->data = NULL; // Take ownership
                    network_response_free(res);
                } else {
                    LOG_WARN("Failed to load background: %s", full_url);
                }
            }
            if (cb) {
                (*current_count)++;
                cb(*current_count, total_count, ctx);
            }
        }

        if (strcasecmp(node->tag_name, "img") == 0) {
            const char *src = get_attr(node, "src");
            if (src) {
                char full_url[1024];
                if (strncmp(src, "http", 4) == 0) {
                    strncpy(full_url, src, 1023);
                } else {
                    snprintf(full_url, 1023, "%s/%s", base_url, src);
                }

                // Try cache first
                size_t cached_size = 0;
                void *cached_data = cache_get_image(full_url, &cached_size);
                if (cached_data) {
                    LOG_DEBUG("Loaded image from cache: %s (%zu bytes)", full_url, cached_size);
                    node->image_data = cached_data;
                    node->image_size = cached_size;
                } else {
                    // Fetch from network
                    network_response_t *res = network_fetch(full_url);
                    if (res) {
                        LOG_DEBUG("Loaded image from network: %s (%lu bytes)", full_url, (unsigned long)res->size);
                        node->image_data = res->data;
                        node->image_size = res->size;
                        // Cache for later
                        cache_put_image(full_url, res->data, res->size);
                        res->data = NULL; // Take ownership
                        network_response_free(res);
                    } else {
                        LOG_WARN("Failed to load image: %s", full_url);
                    }
                }
                if (cb) {
                    (*current_count)++;
                    cb(*current_count, total_count, ctx);
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

                network_response_t *res = network_fetch(full_url);
                if (res && res->data) {
                    node->iframe_doc = html_parse(res->data);
                    // Recursively fetch resources for the iframe
                    loader_fetch_resources(node->iframe_doc, full_url, cb, ctx, current_count, total_count); // Pass pointers down? No, total_count is for *this* doc? 
                    // Actually, if we want global progress, we should count iframe resources too.
                    // But loader_count_resources isn't recursive into iframes because they aren't loaded yet!
                    // So progress bar will jump.
                    // That's acceptable. Or we can just count the iframe fetch itself as 1 step.
                    // I'll count the iframe fetch as 1 step.
                    // The recursive fetch inside iframe won't update the MAIN progress bar total, 
                    // but it will increment current... potentially exceeding total if not careful.
                    // Simplest: Don't pass cb to recursive iframe load? Or pass it but don't increment main count?
                    // I'll pass it. The user will see progress go beyond 100% or just keep moving.
                    // Or I can treat iframe content loading as separate.
                    // Let's just update progress for the iframe *element* itself.
                    network_response_free(res);
                }
                if (cb) {
                    (*current_count)++;
                    cb(*current_count, total_count, ctx);
                }
            }
        }
    }

    node_t *child = node->first_child;
    while (child) {
        loader_fetch_resources(child, base_url, cb, ctx, current_count, total_count);
        child = child->next_sibling;
    }
}