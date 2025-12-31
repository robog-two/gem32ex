#ifndef HTTP_H
#define HTTP_H

#include "protocol.h"

network_response_t* http_fetch(const char *url);
network_response_t* http_post(const char *url, const char *body, const char *content_type);

#endif // HTTP_H
