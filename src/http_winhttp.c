#include "http.h"
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

HttpResponse http_get(const char *url) {
    HttpResponse resp = {0};

    /* Convert URL to wide string */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
    wchar_t *wurl = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (!wurl) return resp;
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, wlen);

    /* Crack the URL */
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = -1;
    uc.dwHostNameLength = -1;
    uc.dwUrlPathLength = -1;
    uc.dwExtraInfoLength = -1;

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        free(wurl);
        return resp;
    }

    /* Extract host */
    wchar_t host[256] = {0};
    if (uc.dwHostNameLength > 0 && uc.dwHostNameLength < 255) {
        wcsncpy(host, uc.lpszHostName, uc.dwHostNameLength);
    }

    HINTERNET session = WinHttpOpen(L"geoviz/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        free(wurl);
        return resp;
    }

    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        free(wurl);
        return resp;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", uc.lpszUrlPath,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        free(wurl);
        return resp;
    }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                             WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        free(wurl);
        return resp;
    }

    if (!WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        free(wurl);
        return resp;
    }

    /* Get status code */
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &status, &status_size, NULL);
    resp.status = (int)status;

    /* Read body */
    size_t total_len = 0;
    size_t cap = 4096;
    unsigned char *buf = (unsigned char *)malloc(cap);

    DWORD bytes_read;
    do {
        DWORD available = 0;
        WinHttpQueryDataAvailable(request, &available);
        if (available == 0) break;

        if (total_len + available >= cap) {
            while (cap < total_len + available) cap *= 2;
            unsigned char *new_buf = (unsigned char *)realloc(buf, cap);
            if (!new_buf) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = new_buf;
        }

        WinHttpReadData(request, buf + total_len, available, &bytes_read);
        total_len += bytes_read;
    } while (bytes_read > 0);

    resp.data = buf;
    resp.len = total_len;

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    free(wurl);

    return resp;
}

void http_response_free(HttpResponse *resp) {
    free(resp->data);
    resp->data = NULL;
    resp->len = 0;
}
