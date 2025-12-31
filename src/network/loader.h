#ifndef LOADER_H
#define LOADER_H

#include "core/dom.h"

typedef void (*loader_progress_cb_t)(int current, int total, void *ctx);

int loader_count_resources(node_t *root);
void loader_fetch_resources(node_t *root, const char *base_url, loader_progress_cb_t cb, void *ctx, int *current_count, int total_count);

#endif // LOADER_H
