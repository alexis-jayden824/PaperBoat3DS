#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pb3ds/compat.h"
#include "pb3ds/memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PB_O2R_NAME_CAPACITY 96U
#define PB_O2R_MAX_REQUESTS 8U

typedef enum {
    PB_O2R_OK = 0,
    PB_O2R_INVALID_ARGUMENT,
    PB_O2R_IO_ERROR,
    PB_O2R_INVALID_ZIP,
    PB_O2R_MULTI_DISK,
    PB_O2R_ZIP64_DIRECTORY,
    PB_O2R_ENTRY_NOT_FOUND,
    PB_O2R_ENTRY_TOO_LARGE,
    PB_O2R_ENTRY_ENCRYPTED,
    PB_O2R_UNSUPPORTED_METHOD,
    PB_O2R_OUT_OF_MEMORY,
    PB_O2R_DECOMPRESSION_FAILED,
    PB_O2R_CHECKSUM_MISMATCH,
    PB_O2R_CAPACITY_EXCEEDED,
} PBO2RResult;

typedef struct {
    char name[PB_O2R_NAME_CAPACITY];
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t local_header_offset;
    uint16_t flags;
    uint16_t method;
    bool found;
} PBO2REntry;

typedef struct {
    const char *name;
    PBO2REntry entry;
} PBO2RRequest;

typedef struct {
    uint32_t directory_entries;
    uint32_t entries_scanned;
    size_t archive_bytes_read;
    size_t compressed_bytes;
    size_t uncompressed_bytes;
} PBO2RStats;

PBO2RResult pb_o2r_find_entries(PBArchive *archive,
                                PBO2RRequest *requests,
                                size_t request_count,
                                PBO2RStats *stats);
PBO2RResult pb_o2r_find_entries_with_prefix(PBArchive *archive,
                                             const char *prefix,
                                             PBO2REntry *entries,
                                             size_t entry_capacity,
                                             size_t *entry_count,
                                             PBO2RStats *stats);
PBO2RResult pb_o2r_find_entry_by_hash(PBArchive *archive, uint64_t hash,
                                      PBO2REntry *entry,
                                      PBO2RStats *stats);
PBO2RResult pb_o2r_extract_entry(PBArchive *archive,
                                 const PBO2REntry *entry,
                                 size_t maximum_size,
                                 PBMemoryMonitor *memory,
                                 PBMemoryClass output_class,
                                 uint8_t **output,
                                 size_t *output_size,
                                 PBO2RStats *stats);
const char *pb_o2r_result_name(PBO2RResult result);

#ifdef __cplusplus
}
#endif
