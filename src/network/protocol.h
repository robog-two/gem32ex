#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

typedef enum {
    PROTOCOL_HTTP,
    PROTOCOL_HTTPS,
    PROTOCOL_GEMINI,
    PROTOCOL_UNKNOWN
} protocol_type_t;

typedef struct {
    char *data;
    size_t size;
    char *content_type;
    int status_code;
    char *final_url; // The URL after all redirects
} network_response_t;

void network_response_free(network_response_t *res);

network_response_t* network_fetch(const char *url);
network_response_t* network_post(const char *url, const char *body, const char *content_type);

#endif // PROTOCOL_H
