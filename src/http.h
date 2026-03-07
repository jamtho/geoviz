#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

typedef struct {
    unsigned char *data;    /* Response body (caller frees) */
    size_t len;             /* Body length in bytes */
    int status;             /* HTTP status code, 0 on connection failure */
} HttpResponse;

HttpResponse http_get(const char *url);
void http_response_free(HttpResponse *resp);

#endif /* HTTP_H */
