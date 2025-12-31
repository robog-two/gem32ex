#include "gemini.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#define SECURITY_WIN32
#include <security.h>
#include <schnlsp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

static int send_all(SOCKET s, const char *buf, int len) {
    int total = 0;
    while (total < len) {
        int n = send(s, buf + total, len - total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

static int recv_all(SOCKET s, char *buf, int len) {
    int n = recv(s, buf, len, 0);
    return n;
}

static SECURITY_STATUS PerformHandshake(SOCKET s, PCredHandle phCreds, const char* szHostName, PCtxtHandle phContext, SecBuffer* pExtraData) {
    SecBufferDesc outBufferDesc, inBufferDesc;
    SecBuffer outBuffer[1], inBuffer[2];
    DWORD dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    SECURITY_STATUS scRet;
    DWORD cbData;

    outBufferDesc.ulVersion = SECBUFFER_VERSION;
    outBufferDesc.cBuffers = 1;
    outBufferDesc.pBuffers = outBuffer;
    outBuffer[0].pvBuffer = NULL;
    outBuffer[0].BufferType = SECBUFFER_TOKEN;
    outBuffer[0].cbBuffer = 0;

    scRet = InitializeSecurityContext(phCreds, NULL, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, NULL, 0, phContext, &outBufferDesc, &dwSSPIFlags, NULL);
    if (scRet != SEC_I_CONTINUE_NEEDED) return scRet;

    if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
        send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
        FreeContextBuffer(outBuffer[0].pvBuffer);
    }

    char readBuf[4096];
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

        scRet = InitializeSecurityContext(phCreds, phContext, (SEC_CHAR*)szHostName, dwSSPIFlags, 0, SECURITY_NATIVE_DREP, &inBufferDesc, 0, NULL, &outBufferDesc, &dwSSPIFlags, NULL);

        if (scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED) {
            if (outBuffer[0].cbBuffer > 0 && outBuffer[0].pvBuffer) {
                send_all(s, outBuffer[0].pvBuffer, outBuffer[0].cbBuffer);
                FreeContextBuffer(outBuffer[0].pvBuffer);
            }
            if (scRet == SEC_E_OK) {
                if (inBuffer[1].BufferType == SECBUFFER_EXTRA) {
                    pExtraData->pvBuffer = malloc(inBuffer[1].cbBuffer);
                    memcpy(pExtraData->pvBuffer, (char*)inBuffer[0].pvBuffer + (inBuffer[0].cbBuffer - inBuffer[1].cbBuffer), inBuffer[1].cbBuffer);
                    pExtraData->cbBuffer = inBuffer[1].cbBuffer;
                    pExtraData->BufferType = SECBUFFER_EXTRA;
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

network_response_t* gemini_fetch(const char *url) {
    if (strncmp(url, "gemini://", 9) != 0) return NULL;

    char host[256];
    char port_str[16];
    int port = 1965;
    const char *path = "/";

    const char *host_start = url + 9;
    const char *slash = strchr(host_start, '/');
    const char *colon = strchr(host_start, ':');

    if (slash) {
        size_t host_len = slash - host_start;
        if (colon && colon < slash) {
            host_len = colon - host_start;
            port = atoi(colon + 1);
        }
        if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';
        path = slash;
    } else {
        if (colon) {
            size_t host_len = colon - host_start;
            if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
            strncpy(host, host_start, host_len);
            host[host_len] = '\0';
            port = atoi(colon + 1);
        } else {
            strncpy(host, host_start, sizeof(host)-1);
            host[sizeof(host)-1] = '\0';
        }
    }
    sprintf(port_str, "%d", port);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return NULL;

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port_str, &hints, &result) != 0) { WSACleanup(); return NULL; }
    SOCKET s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(result); WSACleanup(); return NULL; }
    if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) { closesocket(s); freeaddrinfo(result); WSACleanup(); return NULL; }
    freeaddrinfo(result);

    CredHandle hCreds;
    CtxtHandle hContext;
    SCHANNEL_CRED schannelCred = {0};
    schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
    schannelCred.grbitEnabledProtocols = SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_1_CLIENT | SP_PROT_TLS1_2_CLIENT;

    if (AcquireCredentialsHandle(NULL, UNISP_NAME, SECPKG_CRED_OUTBOUND, NULL, &schannelCred, NULL, NULL, &hCreds, NULL) != SEC_E_OK) { closesocket(s); WSACleanup(); return NULL; }

    SecBuffer extraData = {0};
    if (PerformHandshake(s, &hCreds, host, &hContext, &extraData) != SEC_E_OK) {
        DeleteSecurityContext(&hContext); FreeCredentialsHandle(&hCreds); closesocket(s); WSACleanup(); return NULL;
    }

    // Wrap request
    char request[2048];
    // Spec: absolute URI followed by CRLF. 
    // Spec: empty path SHOULD add a trailing /
    if (path[0] == '\0') path = "/";
    snprintf(request, sizeof(request), "gemini://%s%s\r\n", host, path);

    SecPkgContext_StreamSizes sizes;
    QueryContextAttributes(&hContext, SECPKG_ATTR_STREAM_SIZES, &sizes);

    DWORD cbMsg = strlen(request);
    DWORD cbBuffer = sizes.cbHeader + cbMsg + sizes.cbTrailer;
    char* pBuffer = malloc(cbBuffer);
    memcpy(pBuffer + sizes.cbHeader, request, cbMsg);

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

    EncryptMessage(&hContext, 0, &msgDesc, 0);
    send_all(s, pBuffer, msgBuffers[0].cbBuffer + msgBuffers[1].cbBuffer + msgBuffers[2].cbBuffer);
    free(pBuffer);

    // Receive response
    network_response_t *res = calloc(1, sizeof(network_response_t));
    char* recvBuf = malloc(16384);
    int recvLen = 0;
    if (extraData.cbBuffer > 0) {
        memcpy(recvBuf, extraData.pvBuffer, extraData.cbBuffer);
        recvLen = extraData.cbBuffer;
        free(extraData.pvBuffer);
    }

    while (TRUE) {
        int n = recv(s, recvBuf + recvLen, 16384 - recvLen, 0);
        if (n <= 0) break;
        recvLen += n;

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

        SECURITY_STATUS sc = DecryptMessage(&hContext, &decryptDesc, 0, NULL);
        if (sc == SEC_E_OK) {
            SecBuffer* pData = NULL;
            for(int i=0; i<4; i++) if (decryptBuffers[i].BufferType == SECBUFFER_DATA) pData = &decryptBuffers[i];
            if (pData) {
                // Parse Gemini Header
                char* dataPtr = pData->pvBuffer;
                int dataLen = pData->cbBuffer;
                if (!res->data) {
                    char header[1024];
                    int hLen = (dataLen > 1023) ? 1023 : dataLen;
                    memcpy(header, dataPtr, hLen); header[hLen] = '\0';
                    char* space = strchr(header, ' ');
                    if (space) {
                        *space = '\0';
                        res->status_code = atoi(header);
                        char* crlf = strstr(space + 1, "\r\n");
                        if (crlf) *crlf = '\0';
                        res->content_type = strdup(space + 1);
                        
                        // Body follows header
                        char* bodyStart = strstr(dataPtr, "\r\n");
                        if (bodyStart) {
                            bodyStart += 2;
                            int bodyLen = dataLen - (bodyStart - dataPtr);
                            res->data = malloc(bodyLen + 1);
                            memcpy(res->data, bodyStart, bodyLen);
                            res->data[bodyLen] = '\0';
                            res->size = bodyLen;
                        }
                    }
                } else {
                    // Append more data
                    res->data = realloc(res->data, res->size + dataLen + 1);
                    memcpy(res->data + res->size, dataPtr, dataLen);
                    res->size += dataLen;
                    res->data[res->size] = '\0';
                }
            }
            // If extra data is left, we should loop or handle it. For Gemini, we often get all at once.
            break; 
        } else if (sc == SEC_E_INCOMPLETE_MESSAGE) {
            continue;
        } else {
            break;
        }
    }

    free(recvBuf);
    DeleteSecurityContext(&hContext);
    FreeCredentialsHandle(&hCreds);
    closesocket(s);
    WSACleanup();
    return res;
}