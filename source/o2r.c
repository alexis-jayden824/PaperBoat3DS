#include "pb3ds/o2r.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "zlib.h"

#define PB_ZIP_LOCAL_SIGNATURE 0x04034B50U
#define PB_ZIP_CENTRAL_SIGNATURE 0x02014B50U
#define PB_ZIP_EOCD_SIGNATURE 0x06054B50U
#define PB_ZIP_LOCAL_HEADER_SIZE 30U
#define PB_ZIP_CENTRAL_HEADER_SIZE 46U
#define PB_ZIP_EOCD_SIZE 22U
#define PB_ZIP_MAX_COMMENT 65535U
#define PB_ZIP_CURSOR_BUFFER 4096U
#define PB_ZIP_METHOD_STORED 0U
#define PB_ZIP_METHOD_DEFLATE 8U
#define PB_ZIP_FLAG_ENCRYPTED 0x0001U

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint16_t entries;
} PBZipDirectory;

typedef struct {
    PBArchive *archive;
    PBO2RStats *stats;
    size_t position;
    size_t end;
    size_t buffer_offset;
    size_t buffer_size;
    uint8_t buffer[PB_ZIP_CURSOR_BUFFER];
} PBZipCursor;

typedef union {
    max_align_t alignment;
    size_t allocation_size;
} PBInflateAllocationHeader;

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static bool add_size(size_t left, size_t right, size_t *result) {
    if (result == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *result = left + right;
    return true;
}

static bool archive_read_exact(PBArchive *archive, size_t offset,
                               void *buffer, size_t size,
                               PBO2RStats *stats) {
    if (archive == NULL || archive->file == NULL || buffer == NULL ||
        offset > archive->size || size > archive->size - offset) {
        return false;
    }
    uint8_t *destination = buffer;
    size_t completed = 0;
    while (completed < size) {
        const size_t read = pb_archive_read(
            archive, offset + completed, destination + completed,
            size - completed);
        if (read == 0) {
            return false;
        }
        completed += read;
        if (stats != NULL) {
            stats->archive_bytes_read += read;
        }
    }
    return true;
}

static PBO2RResult find_directory(PBArchive *archive,
                                  PBZipDirectory *directory,
                                  PBO2RStats *stats) {
    if (archive->file == NULL || archive->size < PB_ZIP_EOCD_SIZE) {
        return PB_O2R_INVALID_ZIP;
    }

    const size_t maximum_tail = PB_ZIP_EOCD_SIZE + PB_ZIP_MAX_COMMENT;
    const size_t search_start =
        archive->size > maximum_tail ? archive->size - maximum_tail : 0;
    size_t search_end = archive->size;
    uint8_t block[PB_ZIP_CURSOR_BUFFER];
    uint8_t eocd[PB_ZIP_EOCD_SIZE];

    while (search_end > search_start) {
        const size_t block_start =
            search_end - search_start > sizeof(block)
                ? search_end - sizeof(block)
                : search_start;
        const size_t block_size = search_end - block_start;
        if (!archive_read_exact(archive, block_start, block, block_size,
                                stats)) {
            return PB_O2R_IO_ERROR;
        }

        if (block_size >= sizeof(uint32_t)) {
            size_t index = block_size - sizeof(uint32_t);
            for (;;) {
                if (read_le32(&block[index]) == PB_ZIP_EOCD_SIGNATURE) {
                    const size_t candidate = block_start + index;
                    size_t eocd_end = 0;
                    if (candidate <= archive->size - PB_ZIP_EOCD_SIZE &&
                        archive_read_exact(archive, candidate, eocd,
                                           sizeof(eocd), stats) &&
                        add_size(candidate, PB_ZIP_EOCD_SIZE,
                                 &eocd_end)) {
                        const uint16_t comment_size = read_le16(&eocd[20]);
                        if (comment_size <= archive->size - eocd_end &&
                            eocd_end + comment_size == archive->size) {
                            const uint16_t disk = read_le16(&eocd[4]);
                            const uint16_t directory_disk =
                                read_le16(&eocd[6]);
                            const uint16_t disk_entries =
                                read_le16(&eocd[8]);
                            const uint16_t total_entries =
                                read_le16(&eocd[10]);
                            const uint32_t directory_size =
                                read_le32(&eocd[12]);
                            const uint32_t directory_offset =
                                read_le32(&eocd[16]);
                            if (disk != 0 || directory_disk != 0 ||
                                disk_entries != total_entries) {
                                return PB_O2R_MULTI_DISK;
                            }
                            if (total_entries == UINT16_MAX ||
                                directory_size == UINT32_MAX ||
                                directory_offset == UINT32_MAX) {
                                return PB_O2R_ZIP64_DIRECTORY;
                            }
                            if ((size_t)directory_offset > candidate ||
                                (size_t)directory_size >
                                    candidate - directory_offset) {
                                return PB_O2R_INVALID_ZIP;
                            }
                            directory->offset = directory_offset;
                            directory->size = directory_size;
                            directory->entries = total_entries;
                            if (stats != NULL) {
                                stats->directory_entries = total_entries;
                            }
                            return PB_O2R_OK;
                        }
                    }
                }
                if (index == 0) {
                    break;
                }
                index--;
            }
        }

        if (block_start == search_start) {
            break;
        }
        search_end = block_start + sizeof(uint32_t) - 1U;
    }
    return PB_O2R_INVALID_ZIP;
}

static void cursor_init(PBZipCursor *cursor, PBArchive *archive,
                        size_t start, size_t size, PBO2RStats *stats) {
    memset(cursor, 0, sizeof(*cursor));
    cursor->archive = archive;
    cursor->stats = stats;
    cursor->position = start;
    cursor->end = start + size;
}

static bool cursor_read(PBZipCursor *cursor, void *buffer, size_t size) {
    uint8_t *destination = buffer;
    if (size > cursor->end - cursor->position) {
        return false;
    }
    while (size > 0) {
        const bool cached =
            cursor->position >= cursor->buffer_offset &&
            cursor->position - cursor->buffer_offset < cursor->buffer_size;
        if (!cached) {
            cursor->buffer_offset = cursor->position;
            const size_t remaining = cursor->end - cursor->position;
            const size_t requested = remaining < sizeof(cursor->buffer)
                                         ? remaining
                                         : sizeof(cursor->buffer);
            cursor->buffer_size = pb_archive_read(
                cursor->archive, cursor->buffer_offset, cursor->buffer,
                requested);
            if (cursor->buffer_size != requested) {
                return false;
            }
            if (cursor->stats != NULL) {
                cursor->stats->archive_bytes_read += cursor->buffer_size;
            }
        }

        const size_t buffer_index =
            cursor->position - cursor->buffer_offset;
        const size_t available = cursor->buffer_size - buffer_index;
        const size_t copied = size < available ? size : available;
        memcpy(destination, &cursor->buffer[buffer_index], copied);
        destination += copied;
        cursor->position += copied;
        size -= copied;
    }
    return true;
}

static bool cursor_skip(PBZipCursor *cursor, size_t size) {
    if (size > cursor->end - cursor->position) {
        return false;
    }
    cursor->position += size;
    return true;
}

static PBO2RResult fill_entry(PBO2REntry *entry, const char *name,
                              size_t name_size, const uint8_t *header) {
    if (entry == NULL || name == NULL || header == NULL ||
        name_size == 0U || name_size >= PB_O2R_NAME_CAPACITY) {
        return PB_O2R_INVALID_ARGUMENT;
    }
    memset(entry, 0, sizeof(*entry));
    entry->flags = read_le16(&header[8]);
    entry->method = read_le16(&header[10]);
    entry->crc32 = read_le32(&header[16]);
    entry->compressed_size = read_le32(&header[20]);
    entry->uncompressed_size = read_le32(&header[24]);
    entry->local_header_offset = read_le32(&header[42]);
    if (entry->compressed_size == UINT32_MAX ||
        entry->uncompressed_size == UINT32_MAX ||
        entry->local_header_offset == UINT32_MAX) {
        return PB_O2R_ZIP64_DIRECTORY;
    }
    memcpy(entry->name, name, name_size + 1U);
    entry->found = true;
    return PB_O2R_OK;
}

PBO2RResult pb_o2r_find_entries(PBArchive *archive,
                                PBO2RRequest *requests,
                                size_t request_count,
                                PBO2RStats *stats) {
    if (archive == NULL || requests == NULL || request_count == 0 ||
        request_count > PB_O2R_MAX_REQUESTS) {
        return PB_O2R_INVALID_ARGUMENT;
    }
    for (size_t request_index = 0; request_index < request_count;
         request_index++) {
        const size_t name_size = requests[request_index].name != NULL
                                     ? strlen(requests[request_index].name)
                                     : 0;
        if (name_size == 0 || name_size >= PB_O2R_NAME_CAPACITY) {
            return PB_O2R_INVALID_ARGUMENT;
        }
        memset(&requests[request_index].entry, 0,
               sizeof(requests[request_index].entry));
    }

    PBZipDirectory directory;
    PBO2RResult result = find_directory(archive, &directory, stats);
    if (result != PB_O2R_OK) {
        return result;
    }

    PBZipCursor cursor;
    cursor_init(&cursor, archive, directory.offset, directory.size, stats);
    size_t found_count = 0;
    for (uint32_t entry_index = 0; entry_index < directory.entries;
         entry_index++) {
        uint8_t header[PB_ZIP_CENTRAL_HEADER_SIZE];
        if (!cursor_read(&cursor, header, sizeof(header))) {
            return PB_O2R_IO_ERROR;
        }
        if (read_le32(header) != PB_ZIP_CENTRAL_SIGNATURE) {
            return PB_O2R_INVALID_ZIP;
        }

        const uint16_t name_size = read_le16(&header[28]);
        const uint16_t extra_size = read_le16(&header[30]);
        const uint16_t comment_size = read_le16(&header[32]);
        char name[PB_O2R_NAME_CAPACITY];
        bool name_available = name_size < sizeof(name);
        if (name_available) {
            if (!cursor_read(&cursor, name, name_size)) {
                return PB_O2R_IO_ERROR;
            }
            name[name_size] = '\0';
        } else if (!cursor_skip(&cursor, name_size)) {
            return PB_O2R_INVALID_ZIP;
        }
        if (!cursor_skip(&cursor, (size_t)extra_size + comment_size)) {
            return PB_O2R_INVALID_ZIP;
        }
        if (stats != NULL) {
            stats->entries_scanned++;
        }

        if (!name_available) {
            continue;
        }
        for (size_t request_index = 0; request_index < request_count;
             request_index++) {
            PBO2REntry *entry = &requests[request_index].entry;
            if (entry->found ||
                strcmp(name, requests[request_index].name) != 0) {
                continue;
            }
            result = fill_entry(entry, name, name_size, header);
            if (result != PB_O2R_OK) {
                return result;
            }
            found_count++;
        }
        if (found_count == request_count) {
            return PB_O2R_OK;
        }
    }
    return PB_O2R_ENTRY_NOT_FOUND;
}

PBO2RResult pb_o2r_find_entries_with_prefix(PBArchive *archive,
                                             const char *prefix,
                                             PBO2REntry *entries,
                                             size_t entry_capacity,
                                             size_t *entry_count,
                                             PBO2RStats *stats) {
    const size_t prefix_size = prefix != NULL ? strlen(prefix) : 0U;
    if (archive == NULL || prefix_size == 0U ||
        prefix_size >= PB_O2R_NAME_CAPACITY || entries == NULL ||
        entry_capacity == 0U || entry_count == NULL) {
        return PB_O2R_INVALID_ARGUMENT;
    }
    *entry_count = 0U;
    memset(entries, 0, entry_capacity * sizeof(*entries));

    PBZipDirectory directory;
    PBO2RResult result = find_directory(archive, &directory, stats);
    if (result != PB_O2R_OK) {
        return result;
    }

    PBZipCursor cursor;
    cursor_init(&cursor, archive, directory.offset, directory.size, stats);
    for (uint32_t entry_index = 0U; entry_index < directory.entries;
         entry_index++) {
        uint8_t header[PB_ZIP_CENTRAL_HEADER_SIZE];
        if (!cursor_read(&cursor, header, sizeof(header))) {
            return PB_O2R_IO_ERROR;
        }
        if (read_le32(header) != PB_ZIP_CENTRAL_SIGNATURE) {
            return PB_O2R_INVALID_ZIP;
        }

        const uint16_t name_size = read_le16(&header[28]);
        const uint16_t extra_size = read_le16(&header[30]);
        const uint16_t comment_size = read_le16(&header[32]);
        char name[PB_O2R_NAME_CAPACITY];
        const bool name_available = name_size < sizeof(name);
        if (name_available) {
            if (!cursor_read(&cursor, name, name_size)) {
                return PB_O2R_IO_ERROR;
            }
            name[name_size] = '\0';
        } else if (!cursor_skip(&cursor, name_size)) {
            return PB_O2R_INVALID_ZIP;
        }
        if (!cursor_skip(&cursor, (size_t)extra_size + comment_size)) {
            return PB_O2R_INVALID_ZIP;
        }
        if (stats != NULL) {
            stats->entries_scanned++;
        }
        if (!name_available || name_size < prefix_size ||
            memcmp(name, prefix, prefix_size) != 0) {
            continue;
        }
        if (*entry_count >= entry_capacity) {
            return PB_O2R_CAPACITY_EXCEEDED;
        }
        result = fill_entry(&entries[*entry_count], name, name_size, header);
        if (result != PB_O2R_OK) {
            return result;
        }
        (*entry_count)++;
    }
    return *entry_count > 0U ? PB_O2R_OK : PB_O2R_ENTRY_NOT_FOUND;
}

static uint64_t path_crc64(const char *text) {
    uint64_t crc = UINT64_MAX;
    while (*text != '\0') {
        crc ^= (uint64_t)(uint8_t)*text++ << 56U;
        for (unsigned int bit = 0U; bit < 8U; bit++) {
            crc = (crc & (UINT64_C(1) << 63U)) != 0U
                      ? (crc << 1U) ^ UINT64_C(0x42F0E1EBA9EA3693)
                      : crc << 1U;
        }
    }
    return crc;
}

PBO2RResult pb_o2r_find_entry_by_hash(PBArchive *archive, uint64_t hash,
                                      PBO2REntry *entry,
                                      PBO2RStats *stats) {
    if (archive == NULL || entry == NULL) {
        return PB_O2R_INVALID_ARGUMENT;
    }
    memset(entry, 0, sizeof(*entry));

    PBZipDirectory directory;
    PBO2RResult result = find_directory(archive, &directory, stats);
    if (result != PB_O2R_OK) {
        return result;
    }

    PBZipCursor cursor;
    cursor_init(&cursor, archive, directory.offset, directory.size, stats);
    for (uint32_t entry_index = 0U; entry_index < directory.entries;
         entry_index++) {
        uint8_t header[PB_ZIP_CENTRAL_HEADER_SIZE];
        if (!cursor_read(&cursor, header, sizeof(header))) {
            return PB_O2R_IO_ERROR;
        }
        if (read_le32(header) != PB_ZIP_CENTRAL_SIGNATURE) {
            return PB_O2R_INVALID_ZIP;
        }

        const uint16_t name_size = read_le16(&header[28]);
        const uint16_t extra_size = read_le16(&header[30]);
        const uint16_t comment_size = read_le16(&header[32]);
        char name[PB_O2R_NAME_CAPACITY];
        const bool name_available = name_size < sizeof(name);
        if (name_available) {
            if (!cursor_read(&cursor, name, name_size)) {
                return PB_O2R_IO_ERROR;
            }
            name[name_size] = '\0';
        } else if (!cursor_skip(&cursor, name_size)) {
            return PB_O2R_INVALID_ZIP;
        }
        if (!cursor_skip(&cursor, (size_t)extra_size + comment_size)) {
            return PB_O2R_INVALID_ZIP;
        }
        if (stats != NULL) {
            stats->entries_scanned++;
        }
        if (name_available && path_crc64(name) == hash) {
            return fill_entry(entry, name, name_size, header);
        }
    }
    return PB_O2R_ENTRY_NOT_FOUND;
}

static uint32_t calculate_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0; index < size; index++) {
        crc ^= data[index];
        for (unsigned int bit = 0; bit < 8U; bit++) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static voidpf inflate_allocate(voidpf opaque, uInt items, uInt size) {
    PBMemoryMonitor *memory = opaque;
    if (memory == NULL || items == 0 || size == 0 ||
        (size_t)items >
            (SIZE_MAX - sizeof(PBInflateAllocationHeader)) / size) {
        return Z_NULL;
    }
    const size_t allocation_size =
        sizeof(PBInflateAllocationHeader) + (size_t)items * size;
    PBInflateAllocationHeader *allocation =
        pb_memory_alloc(memory, PB_MEMORY_TRANSIENT, allocation_size);
    if (allocation == NULL) {
        return Z_NULL;
    }
    allocation->allocation_size = allocation_size;
    return allocation + 1;
}

static void inflate_free(voidpf opaque, voidpf address) {
    PBMemoryMonitor *memory = opaque;
    if (memory == NULL || address == Z_NULL) {
        return;
    }
    PBInflateAllocationHeader *allocation =
        (PBInflateAllocationHeader *)address - 1;
    pb_memory_free(memory, PB_MEMORY_TRANSIENT, allocation,
                   allocation->allocation_size);
}

static bool inflate_raw(const uint8_t *compressed, size_t compressed_size,
                        uint8_t *output, size_t output_size,
                        PBMemoryMonitor *memory) {
    if (compressed_size > UINT_MAX || output_size > UINT_MAX) {
        return false;
    }
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = inflate_allocate;
    stream.zfree = inflate_free;
    stream.opaque = memory;
    stream.next_in = (Bytef *)compressed;
    stream.avail_in = (uInt)compressed_size;
    stream.next_out = output;
    stream.avail_out = (uInt)output_size;

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return false;
    }
    const int result = inflate(&stream, Z_FINISH);
    const bool complete =
        result == Z_STREAM_END && stream.total_in == compressed_size &&
        stream.total_out == output_size;
    (void)inflateEnd(&stream);
    return complete;
}

PBO2RResult pb_o2r_extract_entry(PBArchive *archive,
                                 const PBO2REntry *entry,
                                 size_t maximum_size,
                                 PBMemoryMonitor *memory,
                                 PBMemoryClass output_class,
                                 uint8_t **output,
                                 size_t *output_size,
                                 PBO2RStats *stats) {
    if (archive == NULL || entry == NULL || !entry->found || memory == NULL ||
        output == NULL || output_size == NULL || maximum_size == 0) {
        return PB_O2R_INVALID_ARGUMENT;
    }
    *output = NULL;
    *output_size = 0;
    if (entry->uncompressed_size == 0 ||
        entry->uncompressed_size > maximum_size ||
        entry->compressed_size == 0) {
        return PB_O2R_ENTRY_TOO_LARGE;
    }
    if ((entry->flags & PB_ZIP_FLAG_ENCRYPTED) != 0) {
        return PB_O2R_ENTRY_ENCRYPTED;
    }
    if (entry->method != PB_ZIP_METHOD_STORED &&
        entry->method != PB_ZIP_METHOD_DEFLATE) {
        return PB_O2R_UNSUPPORTED_METHOD;
    }

    uint8_t local[PB_ZIP_LOCAL_HEADER_SIZE];
    if (!archive_read_exact(archive, entry->local_header_offset, local,
                            sizeof(local), stats)) {
        return PB_O2R_IO_ERROR;
    }
    if (read_le32(local) != PB_ZIP_LOCAL_SIGNATURE ||
        read_le16(&local[8]) != entry->method ||
        (read_le16(&local[6]) & PB_ZIP_FLAG_ENCRYPTED) != 0) {
        return PB_O2R_INVALID_ZIP;
    }

    const uint16_t local_name_size = read_le16(&local[26]);
    const uint16_t local_extra_size = read_le16(&local[28]);
    const size_t expected_name_size = strlen(entry->name);
    if (local_name_size != expected_name_size) {
        return PB_O2R_INVALID_ZIP;
    }
    char local_name[PB_O2R_NAME_CAPACITY];
    size_t local_name_offset = entry->local_header_offset;
    if (!add_size(local_name_offset, sizeof(local), &local_name_offset)) {
        return PB_O2R_INVALID_ZIP;
    }
    if (!archive_read_exact(archive, local_name_offset, local_name,
                            local_name_size, stats)) {
        return PB_O2R_IO_ERROR;
    }
    local_name[local_name_size] = '\0';
    if (strcmp(local_name, entry->name) != 0) {
        return PB_O2R_INVALID_ZIP;
    }

    size_t data_offset = entry->local_header_offset;
    if (!add_size(data_offset, sizeof(local), &data_offset) ||
        !add_size(data_offset, local_name_size, &data_offset) ||
        !add_size(data_offset, local_extra_size, &data_offset) ||
        data_offset > archive->size ||
        entry->compressed_size > archive->size - data_offset) {
        return PB_O2R_INVALID_ZIP;
    }

    uint8_t *compressed = pb_memory_alloc(
        memory, PB_MEMORY_ARCHIVE, entry->compressed_size);
    if (compressed == NULL) {
        return PB_O2R_OUT_OF_MEMORY;
    }
    PBO2RResult result = PB_O2R_OK;
    uint8_t *decoded = NULL;
    if (!archive_read_exact(archive, data_offset, compressed,
                            entry->compressed_size, stats)) {
        result = PB_O2R_IO_ERROR;
        goto finish;
    }
    decoded = pb_memory_alloc(memory, output_class,
                              entry->uncompressed_size);
    if (decoded == NULL) {
        result = PB_O2R_OUT_OF_MEMORY;
        goto finish;
    }

    if (entry->method == PB_ZIP_METHOD_STORED) {
        if (entry->compressed_size != entry->uncompressed_size) {
            result = PB_O2R_INVALID_ZIP;
            goto finish;
        }
        memcpy(decoded, compressed, entry->uncompressed_size);
    } else if (!inflate_raw(compressed, entry->compressed_size, decoded,
                            entry->uncompressed_size, memory)) {
        result = PB_O2R_DECOMPRESSION_FAILED;
        goto finish;
    }
    if (calculate_crc32(decoded, entry->uncompressed_size) != entry->crc32) {
        result = PB_O2R_CHECKSUM_MISMATCH;
        goto finish;
    }

    *output = decoded;
    *output_size = entry->uncompressed_size;
    decoded = NULL;
    if (stats != NULL) {
        stats->compressed_bytes += entry->compressed_size;
        stats->uncompressed_bytes += entry->uncompressed_size;
    }

finish:
    if (decoded != NULL) {
        pb_memory_free(memory, output_class, decoded,
                       entry->uncompressed_size);
    }
    pb_memory_free(memory, PB_MEMORY_ARCHIVE, compressed,
                   entry->compressed_size);
    return result;
}

const char *pb_o2r_result_name(PBO2RResult result) {
    switch (result) {
        case PB_O2R_OK:
            return "ready";
        case PB_O2R_INVALID_ARGUMENT:
            return "invalid argument";
        case PB_O2R_IO_ERROR:
            return "archive I/O";
        case PB_O2R_INVALID_ZIP:
            return "invalid ZIP";
        case PB_O2R_MULTI_DISK:
            return "multi-disk ZIP";
        case PB_O2R_ZIP64_DIRECTORY:
            return "ZIP64 directory";
        case PB_O2R_ENTRY_NOT_FOUND:
            return "entry missing";
        case PB_O2R_ENTRY_TOO_LARGE:
            return "entry too large";
        case PB_O2R_ENTRY_ENCRYPTED:
            return "encrypted entry";
        case PB_O2R_UNSUPPORTED_METHOD:
            return "unsupported compression";
        case PB_O2R_OUT_OF_MEMORY:
            return "out of memory";
        case PB_O2R_DECOMPRESSION_FAILED:
            return "deflate failed";
        case PB_O2R_CHECKSUM_MISMATCH:
            return "checksum mismatch";
        case PB_O2R_CAPACITY_EXCEEDED:
            return "entry capacity";
        default:
            return "unknown";
    }
}
