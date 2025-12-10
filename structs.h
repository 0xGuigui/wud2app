
#ifndef _STRUCTS_H_
#define _STRUCTS_H_

#include <stdint.h>

#pragma pack(push, 1)
typedef struct _toc_t {
	char name[0x1f];
	char unk; //always 0x1?
	uint32_t offsetBE;
	char unk2[0x5C];
} toc_t;

typedef struct _app_tbl_t {
	uint32_t offsetBE;
	uint32_t size;
	uint64_t tid;
	uint32_t gid;
	char unk[0xC];
} app_tbl_t;
#pragma pack(pop)

#endif
