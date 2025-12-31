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

static void log_hex(const char *label, const char *data, int len) {
    char hex[128];
    int to_log = len > 32 ? 32 : len;
    char *p = hex;
    for (int i = 0; i < to_log; i++) {
        p += sprintf(p, "%02X ", (unsigned char)data[i]);
    }
    LOG_DEBUG("%s [%d bytes]: %s%s", label, len, hex, len > 32 ? "..." : "");
}

static void log_sspi_error(const char *context, SECURITY_STATUS status) {
    char buffer[1024];
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
        GetModuleHandleA("secur32.dll"),
        status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        sizeof(buffer),
        NULL
    );
    if (len > 0) {
        while (len > 0 && (buffer[len-1] == '\r' || buffer[len-1] == '\n')) buffer[--len] = '\0';
        LOG_ERROR("%s failed: %s (0x%lx)", context, buffer, status);
    } else {
        LOG_ERROR("%s failed: Unknown error 0x%lx", context, status);
    }
}

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
    DWORD dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
                        ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM |
                        ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_EXTENDED_ERROR;
    SECURITY_STATUS scRet = SEC_I_CONTINUE_NEEDED;

    outBufferDesc.ulVersion = SECBUFFER_VERSION;
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = outBuffer;
    outBuffer[0].pvBuffer = NULL;
    outBuffer[0].BufferType = SECBUFFER_TOKEN;
    outBuffer[0].cbBuffer = 0;

    LOG_DEBUG("Starting handshake for %s", szHostName);
    scRet = pSSPI->InitializeSecurityContextA(phCreds, NULL, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, NULL, 0, phContext, &outBufferDesc, &dwSSPIFlags, NULL);
    
    if (scRet != SEC_I_CONTINUE_NEEDED) {
        log_sspi_error("Initial InitializeSecurityContext", scRet);
        return scRet;
    }

    if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
        log_hex("Sending ClientHello", outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
        send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
        pSSPI->FreeContextBuffer(outBuffer[0].pvBuffer);
    }

    char readBuf[32768];
    int readOffset = 0;
    BOOL bDone = FALSE;

    while (!bDone) {
        if (readOffset == 0 || scRet == SEC_E_INCOMPLETE_MESSAGE) {
            int n = recv(s, readBuf + readOffset, sizeof(readBuf) - readOffset, 0);
            if (n < 0) {
                LOG_ERROR("Handshake recv failed: %lu", GetLastError());
                return SEC_E_INTERNAL_ERROR;
            }
            if (n == 0) {
                LOG_ERROR("Handshake connection closed by peer");
                return SEC_E_INTERNAL_ERROR;
            }
            LOG_DEBUG("Handshake received %d bytes", n);
            readOffset += n;
        }

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
        LOG_DEBUG("InitializeSecurityContext returned 0x%lx", scRet);

        if (scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED || 
            (FAILED(scRet) && (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer))) {
            
            if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
                log_hex("Sending handshake response", outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
                send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
                pSSPI->FreeContextBuffer(outBuffer[0].pvBuffer);
            }
            
            if (FAILED(scRet)) {
                log_sspi_error("InitializeSecurityContext (loop)", scRet);
                return scRet;
            }

            if (scRet == SEC_E_OK) {
                LOG_INFO("Handshake completed successfully");
                if (inBuffer[1].BufferType == SECBUFFER_EXTRA) {
                    LOG_DEBUG("Extra data after handshake: %d bytes", inBuffer[1].cbBuffer);
                    *extra_data = malloc(inBuffer[1].cbBuffer);
                    if (*extra_data) {
                        memcpy(*extra_data, inBuffer[1].pvBuffer, inBuffer[1].cbBuffer);
                        *extra_data_len = inBuffer[1].cbBuffer;
                    }
                }
                bDone = TRUE;
            } else {
                // SEC_I_CONTINUE_NEEDED
                if (inBuffer[1].BufferType == SECBUFFER_EXTRA) {
                    memmove(readBuf, inBuffer[1].pvBuffer, inBuffer[1].cbBuffer);
                    readOffset = inBuffer[1].cbBuffer;
                    LOG_DEBUG("Processing extra data (%d bytes)", readOffset);
                } else {
                    readOffset = 0;
                }
            }
        } else if (scRet == SEC_E_INCOMPLETE_MESSAGE) {
            LOG_DEBUG("Incomplete message, reading more...");
            continue;
        } else {
            log_sspi_error("InitializeSecurityContext (fatal)", scRet);
            if (readOffset > 0) log_hex("Fatal packet start", readBuf, readOffset);
            return scRet;
        }
    }
    return SEC_E_OK;
}

tls_connection_t* tls_connect(const char *host, int port) {
    tls_connection_t *conn = calloc(1, sizeof(tls_connection_t));
    if (!conn) return NULL;

    conn->pSSPI = InitSecurityInterfaceA();
    if (!conn->pSSPI) { LOG_ERROR("InitSecurityInterfaceA failed"); free(conn); return NULL; }

    char port_str[16];
    sprintf(port_str, "%d", port);

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port_str, &hints, &result) != 0) { 
        LOG_ERROR("getaddrinfo failed for %s", host);
        free(conn); return NULL; 
    }
    conn->socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (conn->socket == INVALID_SOCKET) { LOG_ERROR("socket creation failed"); freeaddrinfo(result); free(conn); return NULL; }
    if (connect(conn->socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) { 
        LOG_ERROR("connect failed to %s:%d", host, port);
        closesocket(conn->socket); freeaddrinfo(result); free(conn); return NULL; 
    }
    freeaddrinfo(result);

    SCHANNEL_CRED schannelCred = {0};
    schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
    schannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;
    schannelCred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS |
                           SCH_CRED_MANUAL_CRED_VALIDATION |
                           SCH_CRED_IGNORE_NO_REVOCATION_CHECK |
                           SCH_CRED_IGNORE_REVOCATION_OFFLINE;

    SECURITY_STATUS status = conn->pSSPI->AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &schannelCred, NULL, NULL, &conn->hCreds, NULL);
    if (status != SEC_E_OK) {
        log_sspi_error("AcquireCredentialsHandle", status);
        closesocket(conn->socket); free(conn); return NULL;
    }

    SECURITY_STATUS sc = PerformHandshake(conn->socket, conn->pSSPI, &conn->hCreds, host, &conn->hContext, &conn->extra_data, &conn->extra_data_len);
    if (sc != SEC_E_OK) {
        conn->pSSPI->FreeCredentialsHandle(&conn->hCreds);
        closesocket(conn->socket); free(conn); return NULL;
    }

    return conn;
}

int tls_send(tls_connection_t *conn, const char *buf, int len) {
    SecPkgContext_StreamSizes sizes;
    SECURITY_STATUS sc = conn->pSSPI->QueryContextAttributesA(&conn->hContext, SECPKG_ATTR_STREAM_SIZES, &sizes);
    if (sc != SEC_E_OK) return -1;

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

    sc = conn->pSSPI->EncryptMessage(&conn->hContext, 0, &msgDesc, 0);
    if (FAILED(sc)) { free(pBuffer); return -1; }
    
    int sent = send_all(conn->socket, pBuffer, msgBuffers[0].cbBuffer + msgBuffers[1].cbBuffer + msgBuffers[2].cbBuffer);
    free(pBuffer);
    return sent > 0 ? len : -1;
}

int tls_recv(tls_connection_t *conn, char *buf, int max_len) {
    char* recvBuf = malloc(65536);
    int recvLen = 0;
    int decrypted_len = 0;
    SECURITY_STATUS sc = SEC_E_OK;

    if (conn->extra_data_len > 0) {
        memcpy(recvBuf, conn->extra_data, conn->extra_data_len);
        recvLen = conn->extra_data_len;
        free(conn->extra_data);
        conn->extra_data = NULL;
        conn->extra_data_len = 0;
    }

    while (TRUE) {
        if (recvLen == 0 || sc == SEC_E_INCOMPLETE_MESSAGE) {
            int n = recv(conn->socket, recvBuf + recvLen, 65536 - recvLen, 0);
            if (n < 0) { free(recvBuf); return -1; }
            if (n == 0) break;
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

        sc = conn->pSSPI->DecryptMessage(&conn->hContext, &decryptDesc, 0, NULL);
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