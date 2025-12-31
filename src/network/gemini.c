#include "gemini.h"
#include "tls.h"
#include "core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

network_response_t* gemini_fetch(const char *url) {
    if (strncmp(url, "gemini://", 9) != 0) return NULL;

    LOG_INFO("Gemini fetching: %s", url);

    char host[256];
    int port = 1965;
    const char *path = "/";

    const char *host_start = url + 9;
    const char *slash = strchr(host_start, '/');
    const char *colon = strchr(host_start, ':');

    if (slash) {
        size_t host_len = slash - host_start;
        if (colon && colon < slash) {
            host_len = colon - host_start;
            port = atoi(colon + 1);
        }
        if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';
        path = slash;
    } else {
        if (colon) {
            size_t host_len = colon - host_start;
            if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
            strncpy(host, host_start, host_len);
            host[host_len] = '\0';
            port = atoi(colon + 1);
        } else {
            strncpy(host, host_start, sizeof(host)-1);
            host[sizeof(host)-1] = '\0';
        }
    }

    LOG_DEBUG("Gemini connection to %s:%d", host, port);

    tls_connection_t *conn = tls_connect(host, port);
    if (!conn) {
        LOG_ERROR("TLS connection failed for %s", host);
        return NULL;
    }

    if (path[0] == '\0') path = "/";
    char request[2048];
    snprintf(request, sizeof(request), "gemini://%s%s\r\n", host, path);

    if (tls_send(conn, request, (int)strlen(request)) < 0) {
        LOG_ERROR("Gemini request send failed");
        tls_close(conn);
        return NULL;
    }

    network_response_t *res = calloc(1, sizeof(network_response_t));
    char buffer[16384];
    int received;
    int total_received = 0;

    while ((received = tls_recv(conn, buffer, sizeof(buffer)-1)) > 0) {
        char *new_data = realloc(res->data, total_received + received + 1);
        if (!new_data) break;
        res->data = new_data;
        memcpy(res->data + total_received, buffer, received);
        total_received += received;
        res->data[total_received] = '\0';
    }
    res->size = total_received;

    // Robust parsing after full receive
    if (res->data && res->size >= 2) {
        char *line_end = strstr(res->data, "\r\n");
        if (line_end) {
            *line_end = '\0';
            char *space = strchr(res->data, ' ');
            if (space) {
                *space = '\0';
                res->status_code = atoi(res->data);
                res->content_type = strdup(space + 1);
                *space = ' '; // Restore
            } else {
                res->status_code = atoi(res->data);
            }
            *line_end = '\r'; // Restore
            
            // Body starts after CRLF
            char *body_start = line_end + 2;
            size_t body_len = res->size - (body_start - res->data);
            if (body_len > 0) {
                char *body_data = malloc(body_len + 1);
                if (body_data) {
                    memcpy(body_data, body_start, body_len);
                    body_data[body_len] = '\0';
                    free(res->data);
                    res->data = body_data;
                    res->size = body_len;
                }
            } else {
                free(res->data);
                res->data = NULL;
                res->size = 0;
            }
        }
    }

    tls_close(conn);
    return res;
}