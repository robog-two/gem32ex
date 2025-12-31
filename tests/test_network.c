#include <windows.h>
#include <string.h>
#include "network/tls.h"
#include "network/http.h"
#include "network/gemini.h"
#include "network/protocol.h"
#include "core/log.h"
#include "test_ui.h"

static int test_tls_impl() {
    tls_connection_t *conn = tls_connect("www.google.com", 443);
    if (!conn) {
        LOG_ERROR("TLS connection failed");
        return 0;
    }
    const char *req = "HEAD / HTTP/1.0\r\nHost: www.google.com\r\n\r\n";
    if (tls_send(conn, req, (int)strlen(req)) <= 0) {
        LOG_ERROR("TLS send failed");
        tls_close(conn);
        return 0;
    }
    char buf[1024];
    if (tls_recv(conn, buf, sizeof(buf)) <= 0) {
        LOG_ERROR("TLS recv failed");
        tls_close(conn);
        return 0;
    }
    LOG_INFO("TLS handshake and minimal exchange successful");
    tls_close(conn);
    return 1;
}

static int test_http_impl() {
    network_response_t *res = network_fetch("http://google.com");
    int ok = (res && res->status_code >= 200 && res->status_code < 400);
    if (!res) LOG_ERROR("Fetch returned NULL");
    else LOG_INFO("HTTP Status: %d, Size: %zu", res->status_code, res->size);
    if (res) network_response_free(res);
    return ok;
}

static int test_https_impl() {
    network_response_t *res = network_fetch("https://www.google.com");
    int ok = (res && res->status_code >= 200 && res->status_code < 400);
    if (!res) LOG_ERROR("Fetch returned NULL");
    else LOG_INFO("HTTPS Status: %d, Size: %zu", res->status_code, res->size);
    if (res) network_response_free(res);
    return ok;
}

static int test_gemini_impl() {
    network_response_t *res = network_fetch("gemini://geminiprotocol.net/");
    int ok = (res && res->status_code == 20);
    if (!res) LOG_ERROR("Fetch returned NULL");
    else LOG_INFO("Gemini Status: %d, Size: %zu", res->status_code, res->size);
    if (res) network_response_free(res);
    return ok;
}

void run_network_tests(int *total_failed) {
    run_test_case("TLS Handshake (google.com)", test_tls_impl, total_failed);
    run_test_case("HTTP Fetch (google.com)", test_http_impl, total_failed);
    run_test_case("HTTPS Fetch (google.com)", test_https_impl, total_failed);
    run_test_case("Gemini Fetch (geminiprotocol.net)", test_gemini_impl, total_failed);
}