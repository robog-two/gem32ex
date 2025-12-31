#ifndef HTTP_H
#define HTTP_H

#include "protocol.h"

network_response_t* http_fetch(const char *url);

#endif // HTTP_H
