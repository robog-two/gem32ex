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

static SECURITY_STATUS PerformHandshake(SOCKET s, PSecurityFunctionTableA pSSPI, PCredHandle phCreds, const char* szHostName, PCtxtHandle phContext, char **extra_data, int *extra_data_len) {
    SecBufferDesc outBufferDesc, inBufferDesc;
    SecBuffer outBuffer[1], inBuffer[2];
    DWORD dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | 
                        ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM |
                        ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_USE_SUPPLIED_CREDS |
                        ISC_REQ_EXTENDED_ERROR;
    SECURITY_STATUS scRet;

    outBufferDesc.ulVersion = SECBUFFER_VERSION;
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = outBuffer;
    outBuffer[0].pvBuffer = NULL;
    outBuffer[0].BufferType = SECBUFFER_TOKEN;
    outBuffer[0].cbBuffer = 0;

    scRet = pSSPI->InitializeSecurityContextA(phCreds, NULL, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, NULL, 0, phContext, &outBufferDesc, &dwSSPIFlags, NULL);
    if (scRet != SEC_I_CONTINUE_NEEDED) {
        log_sspi_error("InitializeSecurityContext (Initial)", scRet);
        return scRet;
    }

    if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
        send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
        pSSPI->FreeContextBuffer(outBuffer[0].pvBuffer);
    }

    char readBuf[16384];
    int readOffset = 0;
    BOOL bDone = FALSE;

    while (!bDone) {
        int n = recv(s, readBuf + readOffset, sizeof(readBuf) - readOffset, 0);
        if (n < 0) {
            LOG_ERROR("Handshake recv failed: %lu", GetLastError());
            return SEC_E_INTERNAL_ERROR;
        }
        if (n == 0) {
            LOG_ERROR("Handshake connection closed by peer");
            return SEC_E_INTERNAL_ERROR;
        }
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

        // Passing szHostName to every call can help with SNI on some SChannel versions
        scRet = pSSPI->InitializeSecurityContextA(phCreds, phContext, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, &inBufferDesc, 0, NULL, &outBufferDesc, &dwSSPIFlags, NULL);

        if (scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED || 
            (FAILED(scRet) && (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer))) {
            
            if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
                send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
                pSSPI->FreeContextBuffer(outBuffer[0].pvBuffer);
            }
            
            if (FAILED(scRet)) {
                log_sspi_error("InitializeSecurityContext", scRet);
                return scRet;
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
            } else {
                if (inBuffer[1].BufferType == SECBUFFER_EXTRA) {
                    memmove(readBuf, (char*)inBuffer[0].pvBuffer + (inBuffer[0].cbBuffer - inBuffer[1].cbBuffer), inBuffer[1].cbBuffer);
                    readOffset = inBuffer[1].cbBuffer;
                } else {
                    readOffset = 0;
                }
            }
        } else if (scRet == SEC_E_INCOMPLETE_MESSAGE) {
            continue;
        } else {
            log_sspi_error("InitializeSecurityContext (Final)", scRet);
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
    // Explicitly enable TLS 1.0, 1.1 and 1.2
    schannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;
    
    // SCH_CRED_NO_DEFAULT_CREDS: Don't use current user's certs
    // SCH_CRED_MANUAL_CRED_VALIDATION: We'll check certs later if needed, avoids some early fails
    schannelCred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | 
                           SCH_CRED_MANUAL_CRED_VALIDATION |
                           SCH_CRED_IGNORE_NO_REVOCATION_CHECK |
                           SCH_CRED_IGNORE_REVOCATION_OFFLINE;

    if (conn->pSSPI->AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &schannelCred, NULL, NULL, &conn->hCreds, NULL) != SEC_E_OK) {
        LOG_ERROR("AcquireCredentialsHandle failed");
        closesocket(conn->socket); free(conn); return NULL;
    }

    // On Windows XP, the 3rd parameter to InitializeSecurityContext (szHostName) 
    // is used for SNI if the system has the right updates.
    SECURITY_STATUS sc = PerformHandshake(conn->socket, conn->pSSPI, &conn->hCreds, host, &conn->hContext, &conn->extra_data, &conn->extra_data_len);
    if (sc != SEC_E_OK) {
        LOG_ERROR("PerformHandshake failed: 0x%lx", sc);
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
            int n = recv(conn->socket, recvBuf + recvLen, 32768 - recvLen, 0);
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
