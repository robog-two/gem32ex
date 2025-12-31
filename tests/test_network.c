#include <stdio.h>
#include <windows.h>
#include <string.h>
#include "network/tls.h"
#include "network/http.h"
#include "network/gemini.h"
#include "network/protocol.h"

static int assert_net(int condition, const char *msg, int *failed) {
    if (condition) {
        return 0;
    } else {
        printf("[ FAIL ] %s\n", msg);
        (*failed)++;
        return 1;
    }
}

void run_network_tests(int *total_failed) {
    int local_failed = 0;
    printf("Running network tests...\n");

    // TLS Test
    tls_connection_t *conn = tls_connect("google.com", 443);
    if (assert_net(conn != NULL, "TLS connection established", &local_failed) == 0) {
        const char *req = "HEAD / HTTP/1.0\r\nHost: google.com\r\n\r\n";
        assert_net(tls_send(conn, req, (int)strlen(req)) > 0, "TLS send works", &local_failed);
        char buf[1024];
        assert_net(tls_recv(conn, buf, sizeof(buf)) > 0, "TLS recv works", &local_failed);
        tls_close(conn);
    }

    // HTTP Test
    network_response_t *res_http = network_fetch("http://google.com");
    if (assert_net(res_http != NULL, "HTTP fetch works", &local_failed) == 0) {
        assert_net(res_http->status_code >= 200 && res_http->status_code < 400, "HTTP status is success/redirect", &local_failed);
        assert_net(res_http->size > 0, "HTTP body is not empty", &local_failed);
        network_response_free(res_http);
    }

    // HTTPS Test
    network_response_t *res_https = network_fetch("https://www.google.com");
    if (assert_net(res_https != NULL, "HTTPS fetch (tunnel) works", &local_failed) == 0) {
        assert_net(res_https->status_code >= 200 && res_https->status_code < 400, "HTTPS status is success/redirect", &local_failed);
        assert_net(res_https->size > 0, "HTTPS body is not empty", &local_failed);
        network_response_free(res_https);
    }

    // Gemini Test
    network_response_t *res_gem = network_fetch("gemini://geminiprotocol.net/");
    if (assert_net(res_gem != NULL, "Gemini fetch works", &local_failed) == 0) {
        assert_net(res_gem->status_code == 20, "Gemini status is 20", &local_failed);
        assert_net(res_gem->size > 0, "Gemini body is not empty", &local_failed);
        network_response_free(res_gem);
    }

    if (local_failed == 0) printf("[ PASS ] All network tests passed.\n");
    *total_failed += local_failed;
}
