#include "http.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} Buffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    Buffer *buf = (Buffer *)userp;
    size_t total = size * nmemb;

    if (buf->len + total >= buf->cap) {
        size_t new_cap = (buf->cap == 0) ? 4096 : buf->cap * 2;
        while (new_cap < buf->len + total) new_cap *= 2;
        unsigned char *new_data = (unsigned char *)realloc(buf->data, new_cap);
        if (!new_data) return 0;
        buf->data = new_data;
        buf->cap = new_cap;
    }

    memcpy(buf->data + buf->len, contents, total);
    buf->len += total;
    return total;
}

HttpResponse http_get(const char *url) {
    HttpResponse resp = {0};

    CURL *curl = curl_easy_init();
    if (!curl) return resp;

    Buffer buf = {0};

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "geoviz/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long status;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        resp.status = (int)status;
        resp.data = buf.data;
        resp.len = buf.len;
    } else {
        resp.status = 0;
        free(buf.data);
    }

    curl_easy_cleanup(curl);
    return resp;
}

void http_response_free(HttpResponse *resp) {
    free(resp->data);
    resp->data = NULL;
    resp->len = 0;
}
