#include "tls.h"
#include "core/log.h"
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SP_PROT_TLS1_1_CLIENT
#define SP_PROT_TLS1_1_CLIENT 0x00000200
#endif
#ifndef SP_PROT_TLS1_2_CLIENT
#define SP_PROT_TLS1_2_CLIENT 0x00000800
#endif

static int send_all(SOCKET s, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int n = send(s, buf + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

static SECURITY_STATUS PerformHandshake(SOCKET s, PSecurityFunctionTableA pSSPI, PCredHandle phCreds, const char* szHostName, PCtxtHandle phContext, char **extra_data, int *extra_data_len) {
    SecBufferDesc outBufferDesc, inBufferDesc;
    SecBuffer outBuffer[1], inBuffer[2];
    DWORD dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    SECURITY_STATUS scRet;

    outBufferDesc.ulVersion = SECBUFFER_VERSION;
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = outBuffer;
    outBuffer[0].pvBuffer = NULL;
    outBuffer[0].BufferType = SECBUFFER_TOKEN;
    outBuffer[0].cbBuffer = 0;

    scRet = pSSPI->InitializeSecurityContextA(phCreds, NULL, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, NULL, 0, phContext, &outBufferDesc, &dwSSPIFlags, NULL);
    if (scRet != SEC_I_CONTINUE_NEEDED) return scRet;

    if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
        send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
        pSSPI->FreeContextBuffer(outBuffer[0].pvBuffer);
    }

    char readBuf[8192];
    int readOffset = 0;
    BOOL bDone = FALSE;

    while (!bDone) {
        int n = recv(s, readBuf + readOffset, sizeof(readBuf) - readOffset, 0);
        if (n <= 0) return SEC_E_INTERNAL_ERROR;
        readOffset += n;

        inBufferDesc.ulVersion = SECBUFFER_VERSION;
        inBufferDesc.cBuffers = 2;
        inBufferDesc.pBuffers = inBuffer;
        inBuffer[0].pvBuffer = readBuf;
        inBuffer[0].cbBuffer = readOffset;
        inBuffer[0].BufferType = SECBUFFER_TOKEN;
        inBuffer[1].pvBuffer = NULL;
        inBuffer[1].cbBuffer = 0;
        inBuffer[1].BufferType = SECBUFFER_EMPTY;

        outBuffer[0].pvBuffer = NULL;
        outBuffer[0].cbBuffer = 0;

        scRet = pSSPI->InitializeSecurityContextA(phCreds, phContext, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, &inBufferDesc, 0, NULL, &outBufferDesc, &dwSSPIFlags, NULL);

        if (scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED) {
            if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
                send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
                pSSPI->FreeContextBuffer(outBuffer[0].pvBuffer);
            }
            if (scRet == SEC_E_OK) {
                if (inBuffer[1].BufferType == SECBUFFER_EXTRA) {
                    *extra_data = malloc(inBuffer[1].cbBuffer);
                    if (*extra_data) {
                        memcpy(*extra_data, (char*)inBuffer[0].pvBuffer + (inBuffer[0].cbBuffer - inBuffer[1].cbBuffer), inBuffer[1].cbBuffer);
                        *extra_data_len = inBuffer[1].cbBuffer;
                    }
                }
                bDone = TRUE;
            }
            readOffset = 0; 
        } else if (scRet == SEC_E_INCOMPLETE_MESSAGE) {
            continue;
        } else {
            return scRet;
        }
    }
    return SEC_E_OK;
}

tls_connection_t* tls_connect(const char *host, int port) {
    tls_connection_t *conn = calloc(1, sizeof(tls_connection_t));
    if (!conn) return NULL;

    conn->pSSPI = InitSecurityInterfaceA();
    if (!conn->pSSPI) { free(conn); return NULL; }

    char port_str[16];
    sprintf(port_str, "%d", port);

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port_str, &hints, &result) != 0) { free(conn); return NULL; }
    conn->socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (conn->socket == INVALID_SOCKET) { freeaddrinfo(result); free(conn); return NULL; }
    if (connect(conn->socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) { 
        closesocket(conn->socket); freeaddrinfo(result); free(conn); return NULL; 
    }
    freeaddrinfo(result);

    SCHANNEL_CRED schannelCred = {0};
    schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
    schannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;
    schannelCred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;

    if (conn->pSSPI->AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &schannelCred, NULL, NULL, &conn->hCreds, NULL) != SEC_E_OK) {
        closesocket(conn->socket); free(conn); return NULL;
    }

    if (PerformHandshake(conn->socket, conn->pSSPI, &conn->hCreds, host, &conn->hContext, &conn->extra_data, &conn->extra_data_len) != SEC_E_OK) {
        conn->pSSPI->FreeCredentialsHandle(&conn->hCreds);
        closesocket(conn->socket); free(conn); return NULL;
    }

    return conn;
}

int tls_send(tls_connection_t *conn, const char *buf, int len) {
    SecPkgContext_StreamSizes sizes;
    conn->pSSPI->QueryContextAttributesA(&conn->hContext, SECPKG_ATTR_STREAM_SIZES, &sizes);

    DWORD cbMsg = (DWORD)len;
    DWORD cbBuffer = sizes.cbHeader + cbMsg + sizes.cbTrailer;
    char* pBuffer = malloc(cbBuffer);
    if (!pBuffer) return -1;
    memcpy(pBuffer + sizes.cbHeader, buf, cbMsg);

    SecBufferDesc msgDesc;
    SecBuffer msgBuffers[4];
    msgDesc.ulVersion = SECBUFFER_VERSION;
    msgDesc.cBuffers = 4;
    msgDesc.pBuffers = msgBuffers;

    msgBuffers[0].pvBuffer = pBuffer;
    msgBuffers[0].cbBuffer = sizes.cbHeader;
    msgBuffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    msgBuffers[1].pvBuffer = pBuffer + sizes.cbHeader;
    msgBuffers[1].cbBuffer = cbMsg;
    msgBuffers[1].BufferType = SECBUFFER_DATA;
    msgBuffers[2].pvBuffer = pBuffer + sizes.cbHeader + cbMsg;
    msgBuffers[2].cbBuffer = sizes.cbTrailer;
    msgBuffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    msgBuffers[3].BufferType = SECBUFFER_EMPTY;

    conn->pSSPI->EncryptMessage(&conn->hContext, 0, &msgDesc, 0);
    int sent = send_all(conn->socket, pBuffer, msgBuffers[0].cbBuffer + msgBuffers[1].cbBuffer + msgBuffers[2].cbBuffer);
    free(pBuffer);
    return sent > 0 ? len : -1;
}

int tls_recv(tls_connection_t *conn, char *buf, int max_len) {
    char* recvBuf = malloc(32768);
    int recvLen = 0;
    int decrypted_len = 0;

    if (conn->extra_data_len > 0) {
        memcpy(recvBuf, conn->extra_data, conn->extra_data_len);
        recvLen = conn->extra_data_len;
        free(conn->extra_data);
        conn->extra_data = NULL;
        conn->extra_data_len = 0;
    }

    while (TRUE) {
        if (recvLen == 0 || decrypted_len == 0) {
            int n = recv(conn->socket, recvBuf + recvLen, 32768 - recvLen, 0);
            if (n <= 0) break;
            recvLen += n;
        }

        SecBufferDesc decryptDesc;
        SecBuffer decryptBuffers[4];
        decryptDesc.ulVersion = SECBUFFER_VERSION;
        decryptDesc.cBuffers = 4;
        decryptDesc.pBuffers = decryptBuffers;
        decryptBuffers[0].pvBuffer = recvBuf;
        decryptBuffers[0].cbBuffer = recvLen;
        decryptBuffers[0].BufferType = SECBUFFER_DATA;
        decryptBuffers[1].BufferType = SECBUFFER_EMPTY;
        decryptBuffers[2].BufferType = SECBUFFER_EMPTY;
        decryptBuffers[3].BufferType = SECBUFFER_EMPTY;

        SECURITY_STATUS sc = conn->pSSPI->DecryptMessage(&conn->hContext, &decryptDesc, 0, NULL);
        if (sc == SEC_E_OK) {
            SecBuffer* pData = NULL;
            SecBuffer* pExtra = NULL;
            for(int i=0; i<4; i++) {
                if (decryptBuffers[i].BufferType == SECBUFFER_DATA) pData = &decryptBuffers[i];
                if (decryptBuffers[i].BufferType == SECBUFFER_EXTRA) pExtra = &decryptBuffers[i];
            }
            if (pData) {
                int to_copy = pData->cbBuffer;
                if (to_copy > max_len) to_copy = max_len;
                memcpy(buf, pData->pvBuffer, to_copy);
                decrypted_len = to_copy;
            }
            if (pExtra) {
                conn->extra_data = malloc(pExtra->cbBuffer);
                if (conn->extra_data) {
                    memcpy(conn->extra_data, pExtra->pvBuffer, pExtra->cbBuffer);
                    conn->extra_data_len = pExtra->cbBuffer;
                }
            }
            break; 
        } else if (sc == SEC_E_INCOMPLETE_MESSAGE) {
            continue;
        } else if (sc == SEC_I_CONTEXT_EXPIRED) {
            break;
        } else {
            break;
        }
    }

    free(recvBuf);
    return decrypted_len;
}

void tls_close(tls_connection_t *conn) {
    if (!conn) return;
    conn->pSSPI->DeleteSecurityContext(&conn->hContext);
    conn->pSSPI->FreeCredentialsHandle(&conn->hCreds);
    closesocket(conn->socket);
    if (conn->extra_data) free(conn->extra_data);
    free(conn);
}
