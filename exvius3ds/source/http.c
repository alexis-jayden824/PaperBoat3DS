#include "ex3/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ex3HttpDispatcher g_dispatcher;

void ex3_http_set_dispatcher(Ex3HttpDispatcher dispatcher)
{
    g_dispatcher = dispatcher;
}

int ex3_http_default_offline_dispatch(const Ex3HttpRequest *req, Ex3HttpResponse *resp)
{
    static const char kBody[] = "{\"ex3\":\"offline-stub\",\"ok\":1}";
    if (req == NULL || resp == NULL) {
        return -1;
    }
    memset(resp, 0, sizeof(*resp));
    resp->status = 200;
    resp->body_len = sizeof(kBody) - 1;
    resp->body = (uint8_t *)malloc(resp->body_len);
    if (resp->body == NULL) {
        return -1;
    }
    memcpy(resp->body, kBody, resp->body_len);
    (void)req;
    return 0;
}

int ex3_http_request(const Ex3HttpRequest *req, Ex3HttpResponse *resp)
{
    Ex3HttpDispatcher d = g_dispatcher;
    if (d == NULL) {
        d = ex3_http_default_offline_dispatch;
    }
    return d(req, resp);
}

void ex3_http_response_free(Ex3HttpResponse *resp)
{
    if (resp == NULL) {
        return;
    }
    free(resp->body);
    resp->body = NULL;
    resp->body_len = 0;
}
