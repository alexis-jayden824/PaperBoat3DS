#include "ex3/login.h"

#include <stdio.h>
#include <string.h>

int ex3_login_guest(Ex3LoginSession *out)
{
    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->user_id, sizeof(out->user_id), "%s", "guest-0001");
    snprintf(out->handle, sizeof(out->handle), "%s", "Exvius3DS");
    out->guest = 1;
    return 0;
}
