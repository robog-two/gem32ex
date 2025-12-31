#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>

// Initialize cache system (creates cache directories if needed)
void cache_init(void);

// Cleanup cache system
void cache_cleanup(void);

// Get cached image data
// Returns pointer to cached data (caller must free), or NULL if not cached
// Sets *out_size to the size of the cached data
void* cache_get_image(const char *url, size_t *out_size);

// Store image data in cache
// Returns 1 on success, 0 on failure
int cache_put_image(const char *url, const void *data, size_t size);

// Clear all cached images
void cache_clear_all(void);

#endif // CACHE_H
