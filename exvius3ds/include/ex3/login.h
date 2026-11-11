#ifndef EX3_LOGIN_H
#define EX3_LOGIN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char user_id[32];
    char handle[32];
    int guest;
} Ex3LoginSession;

/* Replaces Google/Facebook/Apple/PGS LoginService with offline guest. */
int ex3_login_guest(Ex3LoginSession *out);

#ifdef __cplusplus
}
#endif

#endif /* EX3_LOGIN_H */
