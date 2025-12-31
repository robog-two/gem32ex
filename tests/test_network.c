#include <stdio.h>
#include <windows.h>
#include <string.h>
#include "network/tls.h"
#include "network/http.h"
#include "network/gemini.h"
#include "network/protocol.h"
#include "core/log.h"

int g_failed_count = 0;

void assert_test(int condition, const char *msg) {
    if (condition) {
        printf("[ PASS ] %s\n", msg);
    } else {
        printf("[ FAIL ] %s\n", msg);
        g_failed_count++;
    }
}

void test_tls() {
    printf("\n--- Testing TLS Handshake ---\n");
    tls_connection_t *conn = tls_connect("google.com", 443);
    assert_test(conn != NULL, "TLS connection to google.com:443 established");
    if (conn) {
        // Try a minimal send/recv
        const char *req = "HEAD / HTTP/1.0\r\nHost: google.com\r\n\r\n";
        int sent = tls_send(conn, req, (int)strlen(req));
        assert_test(sent > 0, "TLS data sent");
        
        char buf[1024];
        int recvd = tls_recv(conn, buf, sizeof(buf)-1);
        assert_test(recvd > 0, "TLS data received");
        tls_close(conn);
    }
}

void test_http() {
    printf("\n--- Testing HTTP (Cleartext) ---\n");
    network_response_t *res = network_fetch("http://google.com");
    assert_test(res != NULL, "HTTP fetch returned response");
    if (res) {
        assert_test(res->status_code >= 200 && res->status_code < 400, "HTTP status code is success or redirect");
        assert_test(res->size > 0, "HTTP response body is not empty");
        assert_test(res->data != NULL, "HTTP data pointer is valid");
        network_response_free(res);
    }
}

void test_https() {
    printf("\n--- Testing HTTPS (SSPI Tunnel) ---\n");
    // We use a known stable URL
    network_response_t *res = network_fetch("https://www.google.com");
    assert_test(res != NULL, "HTTPS fetch returned response");
    if (res) {
        assert_test(res->status_code >= 200 && res->status_code < 400, "HTTPS status code is success or redirect");
        assert_test(res->size > 0, "HTTPS response body is not empty");
        // Check for common HTML marker if it's supposed to be HTML
        int has_html = (res->data && strstr(res->data, "<html") != NULL);
        assert_test(has_html, "HTTPS response contains HTML marker");
        network_response_free(res);
    }
}

void test_gemini() {
    printf("\n--- Testing Gemini Protocol ---\n");
    network_response_t *res = network_fetch("gemini://geminiprotocol.net/");
    assert_test(res != NULL, "Gemini fetch returned response");
    if (res) {
        assert_test(res->status_code == 20, "Gemini status code is 20 (SUCCESS)");
        assert_test(res->size > 0, "Gemini response body is not empty");
        network_response_free(res);
    }
}

int main() {
    // Initialize logging but keep it quiet if possible
    log_init();
    
    printf("=======================================\n");
    printf("   Gem32 Automated Network Tests\n");
    printf("=======================================\n");
    
    test_tls();
    test_http();
    test_https();
    test_gemini();
    
    printf("\n=======================================\n");
    if (g_failed_count == 0) {
        printf("  ALL TESTS PASSED SUCCESSFULLY\n");
    } else {
        printf("  TEST SUITE FAILED: %d errors\n", g_failed_count);
    }
    printf("=======================================\n");
    
    return g_failed_count;
}