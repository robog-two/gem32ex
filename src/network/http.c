#include "http.h"
#include <windows.h>
#include <wininet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void network_response_free(network_response_t *res) {
    if (res) {
        if (res->data) free(res->data);
        if (res->content_type) free(res->content_type);
        free(res);
    }
}

static network_response_t* perform_http_request(const char *url, const char *method, const char *body, const char *content_type) {
    HINTERNET hInternet = InternetOpen("Gem32Browser/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return NULL;

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    
    // Allocate buffers for cracking URL
    char host[256] = {0};
    char path[2048] = {0};
    
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = sizeof(path);
    urlComp.dwSchemeLength = 1; // Set to non-zero to crack scheme

    if (!InternetCrackUrl(url, 0, 0, &urlComp)) {
        InternetCloseHandle(hInternet);
        return NULL;
    }

    // Default path if empty
    if (strlen(path) == 0) strcpy(path, "/");

    HINTERNET hConnect = InternetConnect(hInternet, host, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return NULL;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
        flags |= INTERNET_FLAG_SECURE;
    }

    HINTERNET hRequest = HttpOpenRequest(hConnect, method, path, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    // Add headers if POST
    const char *headers = NULL;
    DWORD headersLen = 0;
    char headerBuf[256];
    
    if (body && content_type) {
        snprintf(headerBuf, sizeof(headerBuf), "Content-Type: %s\r\n", content_type);
        headers = headerBuf;
        headersLen = strlen(headers);
    }

    BOOL sent = HttpSendRequest(hRequest, headers, headersLen, (LPVOID)body, body ? strlen(body) : 0);
    if (!sent) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    network_response_t *res = calloc(1, sizeof(network_response_t));
    if (!res) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    char buffer[4096];
    DWORD bytesRead;
    size_t totalSize = 0;

    while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        char *newData = realloc(res->data, totalSize + bytesRead + 1);
        if (!newData) {
            network_response_free(res);
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return NULL;
        }
        res->data = newData;
        memcpy(res->data + totalSize, buffer, bytesRead);
        totalSize += bytesRead;
        res->data[totalSize] = '\0';
    }

    res->size = totalSize;

    // Get Content-Type
    char ctBuffer[256];
    DWORD ctSize = sizeof(ctBuffer);
    DWORD index = 0;
    if (HttpQueryInfo(hRequest, HTTP_QUERY_CONTENT_TYPE, ctBuffer, &ctSize, &index)) {
        res->content_type = strdup(ctBuffer);
    }

    // Get Status Code
    DWORD statusCode = 0;
    DWORD scSize = sizeof(statusCode);
    index = 0;
    if (HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &scSize, &index)) {
        res->status_code = (int)statusCode;
    }

    // Get Final URL (WinInet might have followed redirects)
    char finalUrl[2048];
    DWORD fuSize = sizeof(finalUrl);
    if (InternetQueryOption(hRequest, INTERNET_OPTION_URL, finalUrl, &fuSize)) {
        res->final_url = strdup(finalUrl);
    }

    // If it's a redirect and we didn't follow it (or it's a cross-protocol one WinInet won't do)
    if (res->status_code >= 300 && res->status_code <= 308) {
        char locBuffer[2048];
        DWORD locSize = sizeof(locBuffer);
        index = 0;
        if (HttpQueryInfo(hRequest, HTTP_QUERY_LOCATION, locBuffer, &locSize, &index)) {
            // Store redirect location in data if body is empty, or handle in protocol.c
            res->data = strdup(locBuffer);
            res->size = strlen(locBuffer);
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return res;
}

network_response_t* http_fetch(const char *url) {
    return perform_http_request(url, "GET", NULL, NULL);
}

network_response_t* http_post(const char *url, const char *body, const char *content_type) {
    return perform_http_request(url, "POST", body, content_type);
}