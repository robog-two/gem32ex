#include "http.h"
#include "tls.h"
#include "core/log.h"
#include <windows.h>
#include <wininet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void log_last_error(const char *context) {
    DWORD err = GetLastError();
    char buffer[1024];
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
        GetModuleHandleA("wininet.dll"),
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        sizeof(buffer),
        NULL
    );
    if (len > 0) {
        while (len > 0 && (buffer[len-1] == '\r' || buffer[len-1] == '\n')) {
            buffer[--len] = '\0';
        }
        LOG_ERROR("%s failed: %s (%lu)", context, buffer, err);
    } else {
        LOG_ERROR("%s failed: Unknown error %lu", context, err);
    }
}

static network_response_t* https_fetch_raw(const char *host, int port, const char *path, const char *method, const char *body, const char *content_type) {
    LOG_INFO("HTTPS Tunneling: %s:%d%s", host, port, path);
    tls_connection_t *conn = tls_connect(host, port);
    if (!conn) {
        LOG_ERROR("HTTPS Tunneling: TLS connection failed to %s", host);
        return NULL;
    }

    char request[4096];
    int req_len = snprintf(request, sizeof(request), 
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Gem32Browser/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n",
        method, path, host);

    if (body && content_type) {
        req_len += snprintf(request + req_len, sizeof(request) - req_len,
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n",
            content_type, strlen(body));
    }
    strcat(request, "\r\n");
    req_len += 2;

    LOG_DEBUG("HTTPS Request:\n%s", request);

    if (tls_send(conn, request, req_len) < 0) {
        LOG_ERROR("HTTPS Tunneling: Failed to send request");
        tls_close(conn);
        return NULL;
    }

    if (body) {
        tls_send(conn, body, (int)strlen(body));
    }

    network_response_t *res = calloc(1, sizeof(network_response_t));
    char *buffer = malloc(65536);
    int received;
    int total_received = 0;
    char *data = NULL;

    while ((received = tls_recv(conn, buffer, 65535)) > 0) {
        LOG_DEBUG("HTTPS Received %d bytes", received);
        char *new_data = realloc(data, total_received + received + 1);
        if (!new_data) break;
        data = new_data;
        memcpy(data + total_received, buffer, received);
        total_received += received;
        data[total_received] = '\0';
    }
    free(buffer);
    tls_close(conn);

    if (data) {
        LOG_INFO("HTTPS Data received: %d bytes total", total_received);
        // Simple HTTP parser
        char *header_end = strstr(data, "\r\n\r\n");
        if (header_end) {
            *header_end = '\0';
            char *body_start = header_end + 4;
            
            char *first_line_end = strstr(data, "\r\n");
            if (first_line_end) {
                *first_line_end = '\0';
                char *status_code_ptr = strchr(data, ' ');
                if (status_code_ptr) {
                    res->status_code = atoi(status_code_ptr + 1);
                    LOG_INFO("HTTPS Response Status: %d", res->status_code);
                }
            }

            char *ct = strstr(data, "Content-Type: ");
            if (ct) {
                ct += 14;
                char *ct_end = strstr(ct, "\r\n");
                if (ct_end) {
                    int ct_len = (int)(ct_end - ct);
                    res->content_type = malloc(ct_len + 1);
                    memcpy(res->content_type, ct, ct_len);
                    res->content_type[ct_len] = '\0';
                    LOG_DEBUG("HTTPS Content-Type: %s", res->content_type);
                }
            }

            char *loc = strstr(data, "Location: ");
            if (loc) {
                loc += 10;
                char *loc_end = strstr(loc, "\r\n");
                if (loc_end) {
                    int loc_len = (int)(loc_end - loc);
                    if (res->status_code >= 300 && res->status_code <= 308) {
                        res->data = malloc(loc_len + 1);
                        memcpy(res->data, loc, loc_len);
                        res->data[loc_len] = '\0';
                        res->size = loc_len;
                        LOG_INFO("HTTPS Redirect Location: %s", res->data);
                    }
                }
            }

            if (!res->data) {
                size_t body_len = total_received - (body_start - data);
                res->data = malloc(body_len + 1);
                memcpy(res->data, body_start, body_len);
                res->data[body_len] = '\0';
                res->size = body_len;
            }
        } else {
            LOG_WARN("HTTPS Response: Could not find end of headers");
        }
        free(data);
    } else {
        LOG_ERROR("HTTPS Tunneling: No data received");
    }

    return res;
}

static network_response_t* perform_http_request(const char *url, const char *method, const char *body, const char *content_type) {
    LOG_INFO("HTTP %s %s", method, url);

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    char host[256] = {0};
    char path[2048] = {0};
    char extra[1024] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);
    urlComp.lpszExtraInfo = extra;
    urlComp.dwExtraInfoLength = sizeof(extra);
    urlComp.dwSchemeLength = 1;

    if (!InternetCrackUrl(url, 0, 0, &urlComp)) {
        log_last_error("InternetCrackUrl");
        return NULL;
    }

    char full_path[3072];
    strncpy(full_path, path, sizeof(full_path)-1);
    if (strlen(extra) > 0) strncat(full_path, extra, sizeof(full_path) - strlen(full_path) - 1);
    if (strlen(full_path) == 0) strcpy(full_path, "/");

    if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
        return https_fetch_raw(host, urlComp.nPort, full_path, method, body, content_type);
    }

    HINTERNET hInternet = InternetOpen("Gem32Browser/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) { log_last_error("InternetOpen"); return NULL; }

    HINTERNET hConnect = InternetConnect(hInternet, host, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { log_last_error("InternetConnect"); InternetCloseHandle(hInternet); return NULL; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES;
    const char *accept[] = {"*/*", NULL};
    HINTERNET hRequest = HttpOpenRequest(hConnect, method, full_path, NULL, NULL, accept, flags, 0);
    if (!hRequest) { log_last_error("HttpOpenRequest"); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return NULL; }

    const char *headers = NULL;
    DWORD headersLen = 0;
    char headerBuf[256];
    if (body && content_type) {
        snprintf(headerBuf, sizeof(headerBuf), "Content-Type: %s\r\n", content_type);
        headers = headerBuf;
        headersLen = (DWORD)strlen(headers);
    }

    if (!HttpSendRequest(hRequest, headers, headersLen, (LPVOID)body, body ? (DWORD)strlen(body) : 0)) {
        log_last_error("HttpSendRequest");
        InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
        return NULL;
    }

    network_response_t *res = calloc(1, sizeof(network_response_t));
    char buf[4096];
    DWORD bytesRead;
    size_t totalSize = 0;

    while (InternetReadFile(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        char *newData = realloc(res->data, totalSize + bytesRead + 1);
        if (!newData) break;
        res->data = newData;
        memcpy(res->data + totalSize, buf, bytesRead);
        totalSize += bytesRead;
        res->data[totalSize] = '\0';
    }
    res->size = totalSize;

    char ctBuffer[256];
    DWORD ctSize = sizeof(ctBuffer);
    DWORD index = 0;
    if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_TYPE, ctBuffer, &ctSize, &index)) res->content_type = strdup(ctBuffer);

    DWORD statusCode = 0;
    DWORD scSize = sizeof(statusCode);
    index = 0;
    if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &scSize, &index)) res->status_code = (int)statusCode;

    InternetCloseHandle(hRequest); InternetCloseHandle(hConnect); InternetCloseHandle(hInternet);
    return res;
}

network_response_t* http_fetch(const char *url) { return perform_http_request(url, "GET", NULL, NULL); }
network_response_t* http_post(const char *url, const char *body, const char *content_type) { return perform_http_request(url, "POST", body, content_type); }
