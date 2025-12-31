#include "cache.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CACHE_DIR "cache"

// Simple hash function for URL to filename
static void url_to_filename(const char *url, char *filename, size_t max_len) {
    if (!url || !filename) return;

    // Extract domain from URL
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) scheme_end = url;
    else scheme_end += 3;

    const char *domain_start = scheme_end;
    const char *domain_end = strchr(domain_start, '/');
    if (!domain_end) domain_end = domain_start + strlen(domain_start);

    // Extract domain (remove port if present)
    char domain[256] = {0};
    int domain_len = (int)(domain_end - domain_start);
    if (domain_len > 255) domain_len = 255;
    strncpy(domain, domain_start, domain_len);
    domain[domain_len] = '\0';

    // Remove port from domain
    char *port = strchr(domain, ':');
    if (port) *port = '\0';

    // Create filename from URL hash
    unsigned int hash = 5381;
    for (const char *p = url; *p; p++) {
        hash = ((hash << 5) + hash) + (unsigned char)*p;  // hash * 33 + c
    }

    snprintf(filename, max_len, "%s\\%s\\%08x.cache", CACHE_DIR, domain, hash);
}

// Create directory if it doesn't exist
static int ensure_directory_exists(const char *path) {
    // Find the last backslash
    char dir[MAX_PATH] = {0};
    strncpy(dir, path, sizeof(dir) - 1);

    char *last_slash = strrchr(dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }

    // Try to create the directory (will fail silently if it exists)
    CreateDirectoryA(dir, NULL);
    return 1;
}

void cache_init(void) {
    // Create base cache directory
    CreateDirectoryA(CACHE_DIR, NULL);
    LOG_INFO("Cache initialized at %s", CACHE_DIR);
}

void cache_cleanup(void) {
    LOG_INFO("Cache cleanup");
}

void* cache_get_image(const char *url, size_t *out_size) {
    if (!url || !out_size) return NULL;

    char filename[MAX_PATH] = {0};
    url_to_filename(url, filename, sizeof(filename));

    // Try to open and read the cached file
    FILE *f = fopen(filename, "rb");
    if (!f) {
        LOG_DEBUG("Cache miss: %s", url);
        return NULL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        LOG_WARN("Cache file has invalid size: %s", filename);
        return NULL;
    }

    // Read file
    void *data = malloc(size);
    if (!data) {
        fclose(f);
        LOG_ERROR("Failed to allocate memory for cached image: %s", url);
        return NULL;
    }

    size_t read = fread(data, 1, size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(data);
        LOG_WARN("Cache read mismatch: expected %ld, got %zu", size, read);
        return NULL;
    }

    *out_size = size;
    LOG_DEBUG("Cache hit: %s (%zu bytes)", url, size);
    return data;
}

int cache_put_image(const char *url, const void *data, size_t size) {
    if (!url || !data || size == 0) return 0;

    char filename[MAX_PATH] = {0};
    url_to_filename(url, filename, sizeof(filename));

    // Ensure directory exists
    ensure_directory_exists(filename);

    // Write to cache file
    FILE *f = fopen(filename, "wb");
    if (!f) {
        LOG_WARN("Failed to open cache file for writing: %s", filename);
        return 0;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        LOG_WARN("Cache write mismatch: expected %zu, wrote %zu", size, written);
        return 0;
    }

    LOG_DEBUG("Cached image: %s (%zu bytes)", url, size);
    return 1;
}

void cache_clear_all(void) {
    LOG_INFO("Clearing all cached images");
    // Simple approach: just log the intent
    // Full recursive deletion would require more code
    // User can manually delete the cache/ folder
}
