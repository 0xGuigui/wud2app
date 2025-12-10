#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
static inline int wud2app_mkdir(const char *path)
{
	return _mkdir(path);
}
static inline int wud2app_strncasecmp(const char *s1, const char *s2, size_t n)
{
	return _strnicmp(s1, s2, n);
}
static inline int wud2app_fseeko64(FILE *f, int64_t offset, int origin)
{
	return _fseeki64(f, offset, origin);
}
static inline int64_t wud2app_ftello64(FILE *f)
{
	return _ftelli64(f);
}
static inline uint16_t wud2app_bswap16(uint16_t v)
{
	return _byteswap_ushort(v);
}
static inline uint32_t wud2app_bswap32(uint32_t v)
{
	return _byteswap_ulong(v);
}
static inline uint64_t wud2app_bswap64(uint64_t v)
{
	return _byteswap_uint64(v);
}
#else
#include <strings.h>
#include <sys/stat.h>
static inline int wud2app_mkdir(const char *path)
{
	return mkdir(path, 0755);
}
static inline int wud2app_strncasecmp(const char *s1, const char *s2, size_t n)
{
	return strncasecmp(s1, s2, n);
}
static inline int wud2app_fseeko64(FILE *f, int64_t offset, int origin)
{
	return fseeko64(f, offset, origin);
}
static inline int64_t wud2app_ftello64(FILE *f)
{
	return ftello64(f);
}
static inline uint16_t wud2app_bswap16(uint16_t v)
{
	return __builtin_bswap16(v);
}
static inline uint32_t wud2app_bswap32(uint32_t v)
{
	return __builtin_bswap32(v);
}
static inline uint64_t wud2app_bswap64(uint64_t v)
{
	return __builtin_bswap64(v);
}
#endif

static inline uint64_t wud2app_align_forward(uint64_t value, uint64_t alignment)
{
	return (value + (alignment - 1)) & ~(alignment - 1);
}
