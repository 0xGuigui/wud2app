#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct reader reader_t;
typedef enum { READER_WUD, READER_WUX, READER_PARTS } reader_type_t;

reader_t     *reader_open(const char *path);
size_t        reader_read(reader_t *r, void *buf, size_t len);
void          reader_seek(reader_t *r, uint64_t offset);
reader_type_t reader_type(const reader_t *r);
void          reader_close(reader_t *r);
