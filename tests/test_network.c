#include <stdio.h>
#include <windows.h>
#include <string.h>
#include "network/tls.h"
#include "network/http.h"
#include "network/gemini.h"
#include "network/protocol.h"
#include "core/log.h"

static int g_network_failed = 0;

static void assert_net(int condition, const char *msg) {
    if (condition) {
        printf("[ PASS ] %s\n", msg);
    } else {
        printf("[ FAIL ] %s\n", msg);
        g_network_failed++;
    }
}

void test_tls() {
    printf("\n--- Testing TLS Handshake ---\n");
    tls_connection_t *conn = tls_connect("google.com", 443);
    assert_net(conn != NULL, "TLS connection to google.com:443 established");
    if (conn) {
        const char *req = "HEAD / HTTP/1.0\r\nHost: google.com\r\n\r\n";
        int sent = tls_send(conn, req, (int)strlen(req));
        assert_net(sent > 0, "TLS data sent");
        char buf[1024];
        int recvd = tls_recv(conn, buf, sizeof(buf)-1);
        assert_net(recvd > 0, "TLS data received");
        tls_close(conn);
    }
}

void test_http() {
    printf("\n--- Testing HTTP (Cleartext) ---\n");
    network_response_t *res = network_fetch("http://google.com");
    assert_net(res != NULL, "HTTP fetch returned response");
    if (res) {
        assert_net(res->status_code >= 200 && res->status_code < 400, "HTTP status code is success or redirect");
        assert_net(res->size > 0, "HTTP response body is not empty");
        network_response_free(res);
    }
}

void test_https() {
    printf("\n--- Testing HTTPS (SSPI Tunnel) ---\n");
    network_response_t *res = network_fetch("https://www.google.com");
    assert_net(res != NULL, "HTTPS fetch returned response");
    if (res) {
        assert_net(res->status_code >= 200 && res->status_code < 400, "HTTPS status code is success or redirect");
        assert_net(res->size > 0, "HTTPS response body is not empty");
        network_response_free(res);
    }
}

void test_gemini() {
    printf("\n--- Testing Gemini Protocol ---\n");
    network_response_t *res = network_fetch("gemini://geminiprotocol.net/");
    assert_net(res != NULL, "Gemini fetch returned response");
    if (res) {
        assert_net(res->status_code == 20, "Gemini status code is 20 (SUCCESS)");
        assert_net(res->size > 0, "Gemini response body is not empty");
        network_response_free(res);
    }
}

void run_network_tests(int *total_failed) {
    printf("\n>>> RUNNING NETWORK TESTS <<<\n");
    g_network_failed = 0;
    test_tls();
    test_http();
    test_https();
    test_gemini();
    *total_failed += g_network_failed;
}