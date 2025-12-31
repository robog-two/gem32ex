#include "http.h"
#include <windows.h>
#include <wininet.h>
#include <stdlib.h>
#include <string.h>

void network_response_free(network_response_t *res) {
    if (res) {
        if (res->data) free(res->data);
        if (res->content_type) free(res->content_type);
        free(res);
    }
}

network_response_t* http_fetch(const char *url) {
    HINTERNET hInternet = InternetOpen("Gem32Browser/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return NULL;

    HINTERNET hUrl = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return NULL;
    }

    network_response_t *res = calloc(1, sizeof(network_response_t));
    if (!res) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return NULL;
    }

    char buffer[4096];
    DWORD bytesRead;
    size_t totalSize = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        char *newData = realloc(res->data, totalSize + bytesRead + 1);
        if (!newData) {
            network_response_free(res);
            InternetCloseHandle(hUrl);
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
    char contentType[256];
    DWORD ctSize = sizeof(contentType);
    if (HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_TYPE, contentType, &ctSize, NULL)) {
        res->content_type = strdup(contentType);
    }

    // Get Status Code
    DWORD statusCode = 0;
    DWORD scSize = sizeof(statusCode);
    if (HttpQueryInfo(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &scSize, NULL)) {
        res->status_code = (int)statusCode;
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return res;
}
