#ifndef EX3_HTTP_H
#define EX3_HTTP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char url[256];
    char encode_key[16];
    const uint8_t *body;
    size_t body_len;
} Ex3HttpRequest;

typedef struct {
    int status; /* 200 = ok stub */
    uint8_t *body;
    size_t body_len;
} Ex3HttpResponse;

typedef int (*Ex3HttpDispatcher)(const Ex3HttpRequest *req, Ex3HttpResponse *resp);

void ex3_http_set_dispatcher(Ex3HttpDispatcher dispatcher);
int ex3_http_request(const Ex3HttpRequest *req, Ex3HttpResponse *resp);
void ex3_http_response_free(Ex3HttpResponse *resp);

/* Default dispatcher: offline placeholder until S6 ports server handlers. */
int ex3_http_default_offline_dispatch(const Ex3HttpRequest *req, Ex3HttpResponse *resp);

#ifdef __cplusplus
}
#endif

#endif /* EX3_HTTP_H */
