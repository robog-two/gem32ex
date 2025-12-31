#include "tls.h"
#include "core/log.h"
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int openssl_initialized = 0;

static void init_openssl(void) {
    if (!openssl_initialized) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        openssl_initialized = 1;
        LOG_DEBUG("OpenSSL initialized");
    }
}

static void log_openssl_errors(const char *context) {
    unsigned long err;
    char buf[256];
    while ((err = ERR_get_error()) != 0) {
        ERR_error_string_n(err, buf, sizeof(buf));
        LOG_ERROR("%s: %s", context, buf);
    }
}

tls_connection_t* tls_connect(const char *host, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("WSAStartup failed");
        return NULL;
    }

    init_openssl();

    tls_connection_t *conn = calloc(1, sizeof(tls_connection_t));
    if (!conn) {
        LOG_ERROR("Failed to allocate connection structure");
        WSACleanup();
        return NULL;
    }

    // Create SSL context for TLS 1.2 only (OpenSSL 1.1.1 compatible)
    const SSL_METHOD *method = TLSv1_2_client_method();
    conn->ctx = SSL_CTX_new(method);
    if (!conn->ctx) {
        log_openssl_errors("SSL_CTX_new failed");
        free(conn);
        WSACleanup();
        return NULL;
    }

    LOG_INFO("TLS configured to use TLS 1.2 only");

    // Set cipher list - include both ECDSA and RSA cipher suites for Gemini protocol
    // Gemini servers often use ECDSA certificates, so ECDSA ciphers are required
    // All these ciphers are available in OpenSSL 1.1.1
    const char *cipher_list = "ECDHE-ECDSA-AES128-GCM-SHA256:"
                              "ECDHE-ECDSA-AES256-GCM-SHA384:"
                              "ECDHE-RSA-AES128-GCM-SHA256:"
                              "ECDHE-RSA-AES256-GCM-SHA384:"
                              "ECDHE-ECDSA-AES128-SHA256:"
                              "ECDHE-ECDSA-AES256-SHA384:"
                              "ECDHE-RSA-AES128-SHA256:"
                              "ECDHE-RSA-AES256-SHA384:"
                              "AES128-GCM-SHA256:"
                              "AES256-GCM-SHA384";

    if (!SSL_CTX_set_cipher_list(conn->ctx, cipher_list)) {
        log_openssl_errors("Failed to set cipher list");
        LOG_INFO("Using default cipher list");
    }

    // Set elliptic curve for ECDHE - P-256 is widely supported (OpenSSL 1.1.1 compatible)
    EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (ecdh) {
        SSL_CTX_set_tmp_ecdh(conn->ctx, ecdh);
        EC_KEY_free(ecdh);
    }

    // Resolve hostname
    char port_str[16];
    sprintf(port_str, "%d", port);

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        LOG_ERROR("getaddrinfo failed for %s", host);
        SSL_CTX_free(conn->ctx);
        free(conn);
        WSACleanup();
        return NULL;
    }

    // Create socket
    conn->socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (conn->socket == INVALID_SOCKET) {
        LOG_ERROR("socket creation failed");
        freeaddrinfo(result);
        SSL_CTX_free(conn->ctx);
        free(conn);
        WSACleanup();
        return NULL;
    }

    // Connect
    if (connect(conn->socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        LOG_ERROR("connect failed to %s:%d (error: %lu)", host, port, GetLastError());
        closesocket(conn->socket);
        freeaddrinfo(result);
        SSL_CTX_free(conn->ctx);
        free(conn);
        WSACleanup();
        return NULL;
    }
    freeaddrinfo(result);
    LOG_DEBUG("TCP connection established to %s:%d", host, port);

    // Create SSL structure
    conn->ssl = SSL_new(conn->ctx);
    if (!conn->ssl) {
        log_openssl_errors("SSL_new failed");
        closesocket(conn->socket);
        SSL_CTX_free(conn->ctx);
        free(conn);
        WSACleanup();
        return NULL;
    }

    // Set SNI (Server Name Indication)
    if (!SSL_set_tlsext_host_name(conn->ssl, host)) {
        log_openssl_errors("Failed to set SNI");
        LOG_INFO("Continuing without SNI");
    }

    // Associate socket with SSL
    if (!SSL_set_fd(conn->ssl, conn->socket)) {
        log_openssl_errors("SSL_set_fd failed");
        SSL_free(conn->ssl);
        closesocket(conn->socket);
        SSL_CTX_free(conn->ctx);
        free(conn);
        WSACleanup();
        return NULL;
    }

    // Perform TLS handshake
    LOG_DEBUG("Starting TLS handshake with %s", host);
    int ret = SSL_connect(conn->ssl);
    if (ret != 1) {
        int ssl_error = SSL_get_error(conn->ssl, ret);
        LOG_ERROR("SSL_connect failed with error %d", ssl_error);
        log_openssl_errors("TLS handshake failed");

        // Additional diagnostics
        switch (ssl_error) {
            case SSL_ERROR_SSL:
                LOG_ERROR("SSL protocol error during handshake");
                break;
            case SSL_ERROR_SYSCALL:
                LOG_ERROR("System call error: %lu", GetLastError());
                break;
            case SSL_ERROR_ZERO_RETURN:
                LOG_ERROR("Connection closed during handshake");
                break;
            default:
                LOG_ERROR("Unknown SSL error");
                break;
        }

        SSL_free(conn->ssl);
        closesocket(conn->socket);
        SSL_CTX_free(conn->ctx);
        free(conn);
        WSACleanup();
        return NULL;
    }

    // Log connection info
    const char *version = SSL_get_version(conn->ssl);
    const char *cipher = SSL_get_cipher(conn->ssl);
    LOG_INFO("TLS handshake successful: %s using %s", version, cipher);

    return conn;
}

int tls_send(tls_connection_t *conn, const char *buf, int len) {
    if (!conn || !conn->ssl) return -1;

    int sent = SSL_write(conn->ssl, buf, len);
    if (sent <= 0) {
        int ssl_error = SSL_get_error(conn->ssl, sent);
        LOG_ERROR("SSL_write failed with error %d", ssl_error);
        log_openssl_errors("TLS send failed");
        return -1;
    }

    return sent;
}

int tls_recv(tls_connection_t *conn, char *buf, int max_len) {
    if (!conn || !conn->ssl) return -1;

    int received = SSL_read(conn->ssl, buf, max_len);
    if (received < 0) {
        int ssl_error = SSL_get_error(conn->ssl, received);
        if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
            LOG_ERROR("SSL_read failed with error %d", ssl_error);
            log_openssl_errors("TLS recv failed");
            return -1;
        }
        return 0;  // Would block
    }

    return received;
}

void tls_close(tls_connection_t *conn) {
    if (!conn) return;

    if (conn->ssl) {
        SSL_shutdown(conn->ssl);
        SSL_free(conn->ssl);
    }

    if (conn->socket != INVALID_SOCKET) {
        closesocket(conn->socket);
    }

    if (conn->ctx) {
        SSL_CTX_free(conn->ctx);
    }

    free(conn);
    WSACleanup();
}
