#ifndef TLS_H
#define TLS_H

#include <winsock2.h>
#include <windows.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
    SOCKET socket;
    SSL_CTX *ctx;
    SSL *ssl;
} tls_connection_t;

tls_connection_t* tls_connect(const char *host, int port);
int tls_send(tls_connection_t *conn, const char *buf, int len);
int tls_recv(tls_connection_t *conn, char *buf, int max_len);
void tls_close(tls_connection_t *conn);

#endif // TLS_H
