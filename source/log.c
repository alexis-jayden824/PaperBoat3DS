#include "pb3ds/log.h"

#include <3ds.h>
#include <stdarg.h>
#include <sys/stat.h>

#define PB_LOG_DIRECTORY "sdmc:/3ds/PaperBoat3DS"
#define PB_LOG_PATH PB_LOG_DIRECTORY "/PaperBoat3DS.log"

static const char *level_name(PBLogLevel level) {
    switch (level) {
        case PB_LOG_WARNING:
            return "WARN";
        case PB_LOG_ERROR:
            return "ERROR";
        case PB_LOG_INFO:
        default:
            return "INFO";
    }
}

bool pb_log_init(PBLog *log) {
    log->file = NULL;
    log->entries_written = 0;
    log->entries_dropped = 0;

    (void)mkdir("sdmc:/3ds", 0777);
    (void)mkdir(PB_LOG_DIRECTORY, 0777);
    log->file = fopen(PB_LOG_PATH, "a");
    if (log->file == NULL) {
        return false;
    }

    setvbuf(log->file, NULL, _IOLBF, BUFSIZ);
    return true;
}

bool pb_log_is_persistent(const PBLog *log) {
    return log->file != NULL;
}

void pb_log_write(PBLog *log, PBLogLevel level, const char *component,
                  const char *format, ...) {
    if (log->file == NULL) {
        log->entries_dropped++;
        return;
    }

    fprintf(log->file, "[%llu][%s][%s] ",
            (unsigned long long)osGetTime(), level_name(level), component);

    va_list args;
    va_start(args, format);
    vfprintf(log->file, format, args);
    va_end(args);

    fputc('\n', log->file);
    log->entries_written++;
}

void pb_log_close(PBLog *log) {
    if (log->file != NULL) {
        pb_log_write(log, PB_LOG_INFO, "logger", "closing entries=%u",
                     log->entries_written);
        fclose(log->file);
        log->file = NULL;
    }
}
