#include "http.h"
#include "core/log.h"
#include <windows.h>
#include <wininet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void network_response_free(network_response_t *res) {
    if (res) {
        if (res->data) free(res->data);
        if (res->content_type) free(res->content_type);
        if (res->final_url) free(res->final_url);
        free(res);
    }
}

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
        // Remove trailing newlines
        while (len > 0 && (buffer[len-1] == '\r' || buffer[len-1] == '\n')) {
            buffer[--len] = '\0';
        }
        LOG_ERROR("%s failed: %s (%lu)", context, buffer, err);
    } else {
        LOG_ERROR("%s failed: Unknown error %lu", context, err);
    }
}

static network_response_t* perform_http_request(const char *url, const char *method, const char *body, const char *content_type) {
    LOG_INFO("HTTP %s %s", method, url);

    HINTERNET hInternet = InternetOpen("Gem32Browser/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        log_last_error("InternetOpen");
        return NULL;
    }

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
        InternetCloseHandle(hInternet);
        return NULL;
    }

    // Combine path and extra info (query params)
    char full_path[3072];
    strncpy(full_path, path, sizeof(full_path)-1);
    if (strlen(extra) > 0) {
        strncat(full_path, extra, sizeof(full_path) - strlen(full_path) - 1);
    }
    if (strlen(full_path) == 0) strcpy(full_path, "/");

    LOG_DEBUG("Connecting to %s:%d (Scheme: %d)", host, urlComp.nPort, urlComp.nScheme);

    HINTERNET hConnect = InternetConnect(hInternet, host, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        log_last_error("InternetConnect");
        InternetCloseHandle(hInternet);
        return NULL;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES;
    if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
        flags |= INTERNET_FLAG_SECURE;
        flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
        flags |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    HINTERNET hRequest = HttpOpenRequest(hConnect, method, full_path, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        log_last_error("HttpOpenRequest");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    const char *headers = NULL;
    DWORD headersLen = 0;
    char headerBuf[256];
    
    if (body && content_type) {
        snprintf(headerBuf, sizeof(headerBuf), "Content-Type: %s\r\n", content_type);
        headers = headerBuf;
        headersLen = (DWORD)strlen(headers);
    }

    if (body) LOG_DEBUG("HTTP Body: %s", body);

    if (!HttpSendRequest(hRequest, headers, headersLen, (LPVOID)body, body ? (DWORD)strlen(body) : 0)) {
        log_last_error("HttpSendRequest");
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
        LOG_INFO("HTTP Response Status: %d", res->status_code);
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