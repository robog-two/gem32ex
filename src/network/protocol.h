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
} network_response_t;

void network_response_free(network_response_t *res);

#endif // PROTOCOL_H
