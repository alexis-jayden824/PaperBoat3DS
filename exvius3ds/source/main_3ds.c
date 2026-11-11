#include "ex3/bootstrap.h"
#include "ex3/plat.h"
#include "ex3/version.h"

int main(int argc, char **argv)
{
    Ex3Plat *plat;
    Ex3Bootstrap boot;
    (void)argc;
    (void)argv;

    plat = ex3_plat_create();
    if (plat == NULL) {
        return 1;
    }
    if (ex3_bootstrap_init(&boot, plat) != EX3_PLAT_OK) {
        ex3_plat_destroy(plat);
        return 1;
    }

    while (ex3_bootstrap_frame(&boot)) {
        /* Dual-screen diagnostic clear loop. START exits. */
    }

    ex3_bootstrap_shutdown(&boot);
    ex3_plat_destroy(plat);
    return 0;
}
