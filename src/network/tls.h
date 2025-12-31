#ifndef TLS_H
#define TLS_H

#include <winsock2.h>
#define SECURITY_WIN32
#include <security.h>
#include <schnlsp.h>

typedef struct {
    SOCKET socket;
    CredHandle hCreds;
    CtxtHandle hContext;
    PSecurityFunctionTableA pSSPI;
    char *extra_data;
    int extra_data_len;
} tls_connection_t;

tls_connection_t* tls_connect(const char *host, int port);
int tls_send(tls_connection_t *conn, const char *buf, int len);
int tls_recv(tls_connection_t *conn, char *buf, int max_len);
void tls_close(tls_connection_t *conn);

#endif // TLS_H
