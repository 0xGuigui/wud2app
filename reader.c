#ifndef _WIN32
#define __USE_LARGEFILE64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include "reader.h"
#include "platform.h"
#include "wudparts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WUX_MAGIC_0 0x30585557u  /* "WUX0" little-endian */
#define WUX_MAGIC_1 0x1099d02eu

/* Mirror the struct from WUDCompress wud.h — same layout, same padding */
typedef struct {
    uint32_t magic0;
    uint32_t magic1;
    uint32_t sector_size;
    uint64_t uncompressed_size;
    uint32_t flags;
} wux_header_t;

struct reader {
    reader_type_t type;
    FILE         *file;
    /* WUX only */
    uint32_t      wux_sector_size;
    uint32_t      wux_sector_count;
    uint32_t     *wux_index;
    int64_t       wux_sector_array_off;
    uint64_t      wux_pos;
};

reader_t *reader_open(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) {
        /* Detect WUX vs plain WUD */
        uint32_t magic[2] = {0};
        fread(magic, 4, 2, f);
        rewind(f);

        reader_t *r = calloc(1, sizeof(reader_t));
        r->file = f;

        if (magic[0] == WUX_MAGIC_0 && magic[1] == WUX_MAGIC_1) {
            r->type = READER_WUX;

            /*
             * Read header exactly like WUDCompress does — using fread on the
             * full struct so sizeof() matches, then ftello() for the index
             * table offset. This handles struct padding correctly.
             */
            wux_header_t hdr;
            fread(&hdr, sizeof(hdr), 1, f);
            int64_t idx_table_off = wud2app_ftello64(f);

            r->wux_sector_size  = hdr.sector_size;
            r->wux_sector_count = (uint32_t)(
                (hdr.uncompressed_size + hdr.sector_size - 1) / hdr.sector_size);

            /* Index table → then align to sector_size for sector array */
            int64_t sector_array = idx_table_off
                + (int64_t)r->wux_sector_count * (int64_t)sizeof(uint32_t);
            sector_array += hdr.sector_size - 1;
            sector_array -= sector_array % hdr.sector_size;
            r->wux_sector_array_off = sector_array;

            /* Load the full index table into RAM (~2-3 MB for a typical game) */
            r->wux_index = malloc(r->wux_sector_count * sizeof(uint32_t));
            wud2app_fseeko64(f, idx_table_off, SEEK_SET);
            fread(r->wux_index, sizeof(uint32_t), r->wux_sector_count, f);
        } else {
            r->type = READER_WUD;
        }
        return r;
    }

    /* Fall back to wudparts folder */
    if (wudparts_open(path)) {
        reader_t *r = calloc(1, sizeof(reader_t));
        r->type = READER_PARTS;
        return r;
    }

    return NULL;
}

size_t reader_read(reader_t *r, void *buf, size_t len)
{
    switch (r->type) {
    case READER_WUD:
        return fread(buf, 1, len, r->file);

    case READER_PARTS:
        return wudparts_read(buf, len);

    case READER_WUX: {
        size_t   total = 0;
        uint8_t *out   = (uint8_t *)buf;
        while (len > 0) {
            uint32_t sec_idx = (uint32_t)(r->wux_pos / r->wux_sector_size);
            uint32_t sec_off = (uint32_t)(r->wux_pos % r->wux_sector_size);
            uint32_t phys_sec = r->wux_index[sec_idx];
            int64_t  phys_off = r->wux_sector_array_off
                              + (int64_t)phys_sec * r->wux_sector_size
                              + sec_off;
            size_t chunk = r->wux_sector_size - sec_off;
            if (chunk > len) chunk = len;
            wud2app_fseeko64(r->file, phys_off, SEEK_SET);
            size_t n = fread(out, 1, chunk, r->file);
            total        += n;
            out          += n;
            r->wux_pos   += n;
            len          -= n;
            if (n < chunk) break;
        }
        return total;
    }
    }
    return 0;
}

void reader_seek(reader_t *r, uint64_t offset)
{
    switch (r->type) {
    case READER_WUD:
        wud2app_fseeko64(r->file, (int64_t)offset, SEEK_SET);
        break;
    case READER_PARTS:
        wudparts_seek(offset);
        break;
    case READER_WUX:
        r->wux_pos = offset;
        break;
    }
}

reader_type_t reader_type(const reader_t *r)
{
    return r->type;
}

void reader_close(reader_t *r)
{
    if (r->type == READER_WUD || r->type == READER_WUX) {
        fclose(r->file);
        if (r->type == READER_WUX)
            free(r->wux_index);
    } else {
        wudparts_close();
    }
    free(r);
}
